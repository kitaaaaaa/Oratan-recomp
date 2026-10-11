// Input recording and replay (developer).
//
// --replay_record=FILE records every controller read from boot; --replay_play=FILE
// feeds a recording back instead of the real controllers, so the game replays
// the session by itself. Both also store or check a fingerprint of the game
// state after every tick, so a replay that drifts reports the first tick and
// the part of the state that differs.
//
// Uses: proving the game is deterministic (needed for lockstep netplay),
// checking decompiled functions that depend on input history (sub_822A71B0)
// by replaying the same session through both versions, and regression tests.
//
// How:
//   - Ticks are counted by Update (sub_821236E0, vtable slot 2 of the run loop
//     object), one call per 1/60 s game tick, menus included, from boot.
//   - Controllers are read by sub_821135C0 into game::kControllerStates
//     (4 x 60 bytes). Recording saves those bytes after each read, tagged with
//     the tick; replaying overwrites them with the recording's, read for read.
//     Anything the game derives from that buffer follows the recording.
//   - After each Update, a few regions are hashed (see HashState). Recording
//     stores the hashes; replaying compares them.
// Replays always start at boot: the recomp has no save states.

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "game_memory.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

REXCVAR_DEFINE_STRING(replay_record, "", "Oratan/Replay",
                      "Developer: record every controller read and a per-tick state "
                      "fingerprint from boot to this file");
REXCVAR_DEFINE_STRING(replay_play, "", "Oratan/Replay",
                      "Developer: replay a recording from boot instead of the real controllers, "
                      "and report the first tick where the game state drifts");
REXCVAR_DEFINE_INT32(replay_dump_tick, -1, "Oratan/Replay",
                     "Developer: write the hashed state regions after this tick to "
                     "replay_tickN_<mode>.bin next to the recording (also done automatically at "
                     "the first drift during replay)");
REXCVAR_DEFINE_INT32(replay_hash_bytes, 0xD000, "Oratan/Replay",
                     "Developer: bytes of the battle state (game::Game) included in the per-tick "
                     "fingerprint; the default covers every field mapped so far")
    .range(0, 0x40000);

REX_EXTERN(__imp__sub_821236E0);
REX_EXTERN(__imp__sub_821135C0);

namespace {

namespace game = oratan::game;

// --- File format ---------------------------------------------------------------
// Header, then records, all host byte order (the file never leaves the PC):
//   'I' u32 tick, ControllerState[4]   one controller read
//   'H' u32 tick, u64 hash[kRegions]   the state after one tick
//   'E' u32 tick                       end (written on a clean stop)

constexpr char kMagic[4] = {'O', 'T', 'R', 'P'};
constexpr uint32_t kVersion = 1;
constexpr uint32_t kControllers = 4;
constexpr uint32_t kInputBytes = kControllers * sizeof(game::ControllerState);

enum Region : uint32_t { kGameRegion, kRobot1Region, kRobot2Region, kFlowRegion, kRegions };
constexpr const char* kRegionNames[kRegions] = {"game", "robot 1", "robot 2",
                                                "inputs/module/stage"};

struct Header {
  char magic[4];
  uint32_t version;
  uint32_t input_bytes;
  uint32_t regions;
  int32_t hash_bytes;  // replay_hash_bytes at record time
};

using Hashes = std::array<uint64_t, kRegions>;

// FNV-1a; the regions are small (tens of KB per tick).
uint64_t Hash(const uint8_t* data, size_t size, uint64_t h = 0xCBF29CE484222325ull) {
  for (size_t i = 0; i < size; ++i) {
    h = (h ^ data[i]) * 0x100000001B3ull;
  }
  return h;
}

// The state fingerprint: the battle state (first replay_hash_bytes), each robot
// record on its own (so a drift says which robot), and the side input records,
// module and next stage. Outside battles the game regions hash to 0.
Hashes HashState(uint8_t* base, int32_t hash_bytes) {
  Hashes h{};
  if (game::Game* g = game::GetGame(base)) {
    h[kGameRegion] = Hash(reinterpret_cast<const uint8_t*>(g), size_t(hash_bytes));
    h[kRobot1Region] = Hash(reinterpret_cast<const uint8_t*>(&g->robots[0]), sizeof(game::Robot));
    h[kRobot2Region] = Hash(reinterpret_cast<const uint8_t*>(&g->robots[1]), sizeof(game::Robot));
  }
  uint64_t flow = Hash(reinterpret_cast<const uint8_t*>(game::GetSideInputs(base)),
                       2 * sizeof(game::SideInput));
  flow = Hash(base + game::kModuleIndex, 1, flow);
  flow = Hash(base + game::kNextStage, 4, flow);
  h[kFlowRegion] = flow;
  return h;
}

// Writes the hashed regions after `tick` next to the recording, for diffing a
// recording run against a drifting replay.
void DumpState(uint8_t* base, const std::string& file, uint32_t tick, const char* mode,
               int32_t hash_bytes) {
  const size_t slash = file.find_last_of("/\\");
  const std::string dir = slash == std::string::npos ? "." : file.substr(0, slash);
  const std::string path = dir + "/replay_tick" + std::to_string(tick) + "_" + mode + ".bin";
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  if (game::Game* g = game::GetGame(base)) {
    std::fwrite(g, 1, size_t(hash_bytes), f);
  }
  std::fwrite(game::GetSideInputs(base), 1, 2 * sizeof(game::SideInput), f);
  std::fclose(f);
  REXLOG_INFO("replay: state after tick {} written to {}", tick, path);
}

// --- Session -------------------------------------------------------------------

enum class Mode { kOff, kRecord, kPlay, kFinished };

struct Session {
  Mode mode = Mode::kOff;
  std::string file_name;
  FILE* file = nullptr;
  int32_t hash_bytes = 0;
  uint32_t tick = 0;  // Update calls started so far
  std::thread::id game_thread;  // the thread running Update
  bool have_game_thread = false;
  bool warned_thread = false;

  // Replay: the next unread record.
  char next_type = 0;
  uint32_t next_tick = 0;
  uint32_t drifts = 0;
  uint32_t input_misses = 0;

  // No logging here: the logger may already be gone at exit.
  ~Session() { Stop("game exit", false); }

  // From whichever hook runs first, so reads before the first Update count too.
  void EnsureStarted() {
    if (mode == Mode::kOff) Start();
  }

  void Start() {
    const std::string play = REXCVAR_GET(replay_play);
    const std::string record = REXCVAR_GET(replay_record);
    if (!play.empty()) {
      if (!record.empty()) REXLOG_WARN("replay: both replay_play and replay_record set; playing");
      StartPlay(play);
    } else if (!record.empty()) {
      StartRecord(record);
    } else {
      mode = Mode::kFinished;  // nothing to do; don't check again
    }
  }

  void StartRecord(const std::string& path) {
    file = std::fopen(path.c_str(), "wb");
    if (!file) {
      REXLOG_ERROR("replay: cannot create {}", path);
      mode = Mode::kFinished;
      return;
    }
    hash_bytes = REXCVAR_GET(replay_hash_bytes);
    const Header header{{kMagic[0], kMagic[1], kMagic[2], kMagic[3]}, kVersion, kInputBytes,
                        kRegions, hash_bytes};
    std::fwrite(&header, sizeof(header), 1, file);
    file_name = path;
    mode = Mode::kRecord;
    REXLOG_INFO("replay: recording to {}", path);
  }

  void StartPlay(const std::string& path) {
    file = std::fopen(path.c_str(), "rb");
    Header header{};
    if (!file || std::fread(&header, sizeof(header), 1, file) != 1 ||
        std::memcmp(header.magic, kMagic, 4) != 0 || header.version != kVersion ||
        header.input_bytes != kInputBytes || header.regions != kRegions) {
      REXLOG_ERROR("replay: {} is missing or not a recording of this version", path);
      if (file) std::fclose(file);
      file = nullptr;
      mode = Mode::kFinished;
      return;
    }
    hash_bytes = header.hash_bytes;  // compare like with like
    file_name = path;
    mode = Mode::kPlay;
    ReadNextHeader();
    REXLOG_INFO("replay: playing {}", path);
  }

  void Stop(const char* why, bool log = true) {
    if (!file) return;
    if (mode == Mode::kRecord) {
      const char type = 'E';
      std::fwrite(&type, 1, 1, file);
      std::fwrite(&tick, 4, 1, file);
      if (log) REXLOG_INFO("replay: recorded {} ticks ({})", tick, why);
    } else if (mode == Mode::kPlay && log) {
      REXLOG_INFO("replay: stopped at tick {} ({}): {} drifting ticks, {} unmatched reads", tick,
                  why, drifts, input_misses);
    }
    std::fclose(file);
    file = nullptr;
    mode = Mode::kFinished;
  }

  // Reads the type and tick of the next record (its payload stays unread).
  void ReadNextHeader() {
    if (std::fread(&next_type, 1, 1, file) != 1 || std::fread(&next_tick, 4, 1, file) != 1) {
      next_type = 'E';
    }
  }

  void SkipPayload() {
    if (next_type == 'I') std::fseek(file, kInputBytes, SEEK_CUR);
    if (next_type == 'H') std::fseek(file, sizeof(Hashes), SEEK_CUR);
  }

  void CheckThread() {
    if (have_game_thread && std::this_thread::get_id() != game_thread && !warned_thread) {
      warned_thread = true;
      REXLOG_WARN("replay: controllers read outside the Update thread; replays may not be exact");
    }
  }

  // After the game read the controllers.
  void OnControllerRead(uint8_t* base) {
    uint8_t* states = base + game::kControllerStates;
    if (mode == Mode::kRecord) {
      CheckThread();
      const char type = 'I';
      std::fwrite(&type, 1, 1, file);
      std::fwrite(&tick, 4, 1, file);
      std::fwrite(states, 1, kInputBytes, file);
    } else if (mode == Mode::kPlay) {
      CheckThread();
      // Skip hash records still pending from earlier ticks (they are checked in
      // OnTickEnd; reaching one here means this read is out of step).
      while (next_type == 'H' && next_tick < tick) {
        SkipPayload();
        ReadNextHeader();
      }
      if (next_type == 'I' && next_tick == tick) {
        if (std::fread(states, 1, kInputBytes, file) != kInputBytes) {
          Stop("recording is truncated");
          return;
        }
        ReadNextHeader();
      } else {
        // The recording has no read here: the game reads controllers at
        // different points than when it was recorded.
        if (input_misses++ < 10) {
          REXLOG_ERROR("replay: tick {}: controller read the recording doesn't have", tick);
        }
      }
      if (next_type == 'E') Stop("end of recording");
    }
  }

  // After each Update.
  void OnTickEnd(uint8_t* base) {
    const Hashes now = HashState(base, hash_bytes);
    const int32_t dump_tick = REXCVAR_GET(replay_dump_tick);
    if (mode == Mode::kRecord) {
      const char type = 'H';
      std::fwrite(&type, 1, 1, file);
      std::fwrite(&tick, 4, 1, file);
      std::fwrite(now.data(), sizeof(Hashes), 1, file);
      if (dump_tick >= 0 && uint32_t(dump_tick) == tick) {
        DumpState(base, file_name, tick, "record", hash_bytes);
      }
      if (tick % 600 == 0) std::fflush(file);  // keep most of it if the game crashes
      return;
    }
    if (mode != Mode::kPlay) return;
    // Reads the recording didn't consume belong to earlier ticks: this tick
    // read the controllers fewer times than the recording did.
    while (next_type == 'I' && next_tick <= tick) {
      if (input_misses++ < 10) {
        REXLOG_ERROR("replay: tick {}: recorded controller read the game didn't make", next_tick);
      }
      SkipPayload();
      ReadNextHeader();
    }
    if (next_type == 'H' && next_tick == tick) {
      Hashes recorded{};
      if (std::fread(recorded.data(), sizeof(Hashes), 1, file) != 1) {
        Stop("recording is truncated");
        return;
      }
      ReadNextHeader();
      if (recorded != now) {
        std::string regions;
        for (uint32_t r = 0; r < kRegions; ++r) {
          if (recorded[r] != now[r]) regions += std::string(regions.empty() ? "" : ", ") + kRegionNames[r];
        }
        if (drifts++ == 0) {
          REXLOG_ERROR("replay: first drift after tick {}: {} differ", tick, regions);
          DumpState(base, file_name, tick, "replay", hash_bytes);
        } else if (drifts <= 10) {
          REXLOG_ERROR("replay: drift after tick {}: {} differ", tick, regions);
        }
      }
    }
    if (dump_tick >= 0 && uint32_t(dump_tick) == tick) {
      DumpState(base, file_name, tick, "replay", hash_bytes);
    }
    if (next_type == 'E') Stop("end of recording");
  }
};

Session g_session;

}  // namespace

// sub_821236E0: the run loop's Update, one game tick.
REX_HOOK_RAW(sub_821236E0) {
  if (!g_session.have_game_thread) {
    g_session.game_thread = std::this_thread::get_id();
    g_session.have_game_thread = true;
  }
  g_session.EnsureStarted();
  if (g_session.mode == Mode::kFinished) {
    __imp__sub_821236E0(ctx, base);
    return;
  }
  ++g_session.tick;
  __imp__sub_821236E0(ctx, base);
  if (g_session.mode == Mode::kRecord || g_session.mode == Mode::kPlay) {
    g_session.OnTickEnd(base);
  }
}

// sub_821135C0: reads the controllers into game::kControllerStates.
REX_HOOK_RAW(sub_821135C0) {
  g_session.EnsureStarted();
  __imp__sub_821135C0(ctx, base);
  if (g_session.mode == Mode::kRecord || g_session.mode == Mode::kPlay) {
    g_session.OnControllerRead(base);
  }
}
