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
//   - Ticks and controller reads come from tick_hooks.cpp (Update and
//     sub_821135C0). Recording saves the controller buffer
//     (game::kControllerStates, 4 x 60 bytes) after each read, tagged with the
//     tick; replaying overwrites it with the recording's, read for read.
//     Anything the game derives from that buffer follows the recording.
//   - After each Update the state fingerprint (state_hash.h) is taken.
//     Recording stores it; replaying compares it.
// Replays always start at boot: the recomp has no save states.

#include <rex/cvar.h>
#include <rex/logging.h>

#include "game_memory.h"
#include "state_hash.h"
#include "tick_hooks.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

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

namespace {

namespace game = oratan::game;

// --- File format ---------------------------------------------------------------
// Header, then records, all host byte order (the file never leaves the PC):
//   'I' u32 tick, ControllerState[4]   one controller read
//   'H' u32 tick, u64 hash[kRegions]   the state after one tick
//   'E' u32 tick                       end (written on a clean stop)

constexpr char kMagic[4] = {'O', 'T', 'R', 'P'};
// 2: robot heap pointers left out of the fingerprint.
constexpr uint32_t kVersion = 2;
constexpr uint32_t kControllers = 4;
constexpr uint32_t kInputBytes = kControllers * sizeof(game::ControllerState);

constexpr uint32_t kRegions = oratan::kStateRegions;

struct Header {
  char magic[4];
  uint32_t version;
  uint32_t input_bytes;
  uint32_t regions;
  int32_t hash_bytes;  // replay_hash_bytes at record time
};

using Hashes = oratan::StateHashes;

// Writes the hashed regions after `tick` next to the recording, for diffing a
// recording run against a drifting replay.
void DumpState(uint8_t* base, const std::string& file, uint32_t tick, const char* mode,
               int32_t hash_bytes) {
  const size_t slash = file.find_last_of("/\\");
  const std::string dir = slash == std::string::npos ? "." : file.substr(0, slash);
  oratan::DumpStateRegions(
      base, dir + "/replay_tick" + std::to_string(tick) + "_" + mode + ".bin", hash_bytes);
}

// --- Session -------------------------------------------------------------------

enum class Mode { kOff, kRecord, kPlay, kFinished };

struct Session {
  Mode mode = Mode::kOff;
  std::string file_name;
  FILE* file = nullptr;
  int32_t hash_bytes = 0;
  bool warned_thread = false;

  // Replay: the next unread record.
  char next_type = 0;
  uint32_t next_tick = 0;
  uint32_t drifts = 0;
  uint32_t input_misses = 0;

  // No logging here: the logger may already be gone at exit.
  ~Session() { Stop("game exit", false); }

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
    const uint32_t tick = oratan::GameTick();
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
    if (!oratan::OnGameThread() && !warned_thread) {
      warned_thread = true;
      REXLOG_WARN("replay: controllers read outside the Update thread; replays may not be exact");
    }
  }

  // After the game read the controllers.
  void OnControllerRead(uint8_t* base) {
    const uint32_t tick = oratan::GameTick();
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
    const uint32_t tick = oratan::GameTick();
    const Hashes now = oratan::HashState(base, hash_bytes);
    const int32_t dump_tick = REXCVAR_GET(replay_dump_tick);
    if (mode == Mode::kRecord) {
      const char type = 'H';
      std::fwrite(&type, 1, 1, file);
      std::fwrite(&tick, 4, 1, file);
      std::fwrite(now.data(), sizeof(Hashes), 1, file);
      if (dump_tick >= 0 && uint32_t(dump_tick) == tick) {
        DumpState(base, file_name, tick, "record", hash_bytes);
      }
      // Every tick: closing the window hard-exits the process (ReXApp::OnClosing),
      // so Stop never runs and nothing unflushed would survive. A replay treats
      // the end of the file as the end of the recording.
      std::fflush(file);
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
        const std::string regions = oratan::DifferingRegions(recorded, now);
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

namespace oratan::replay {

void Start() { g_session.Start(); }

bool Playing() { return g_session.mode == Mode::kPlay; }

void OnControllerRead(uint8_t* base) {
  if (g_session.mode == Mode::kRecord || g_session.mode == Mode::kPlay) {
    g_session.OnControllerRead(base);
  }
}

void OnTickEnd(uint8_t* base) {
  if (g_session.mode == Mode::kRecord || g_session.mode == Mode::kPlay) {
    g_session.OnTickEnd(base);
  }
}

}  // namespace oratan::replay
