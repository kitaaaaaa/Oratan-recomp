// Developer tools for working on the recompilation (not used in normal play).

#include "debug_tools.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/ppc/func.h>
#include <rex/system/xmemory.h>

#include "game_memory.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define NOMINMAX
#include <windows.h>
#include <algorithm>

REXCVAR_DEFINE_STRING(dump_image, "", "Oratan",
                      "Write the loaded XEX image (0x82000000-0x83A30000, big-endian) "
                      "to this file after load, for offline analysis");

REXCVAR_DEFINE_STRING(capture_dir, "", "Oratan",
                      "Developer: pressing F9 writes the XEX data section and guest memory "
                      "0xA4000000-0xAE000000 (models, draw list, palettes) to this folder "
                      "as capN_data.bin / capN_mem.bin, for exporting models");

REXCVAR_DEFINE_STRING(memory_excerpt_list, "", "Oratan",
                      "Developer: a text file of guest memory ranges to write once to "
                      "--memory_excerpt_out, on the first fight tick. Each line: an address "
                      "expression, a length, and optionally 'as ADDRESS' (where to file the "
                      "block) or 'committed' (only its committed pages, as separate blocks). "
                      "Expression tokens: numbers (0x.. hex or decimal), +N / -N, and "
                      "'*' (replace the value by the big-endian u32 at it); '#' starts a comment");
REXCVAR_DEFINE_STRING(memory_excerpt_out, "", "Oratan",
                      "Developer: where --memory_excerpt_list writes its blocks: "
                      "\"OXMX\", u32 version 1, u32 count, then per block u32 address, u32 "
                      "length and the bytes (header fields little-endian)");

REXCVAR_DEFINE_STRING(watch_u32, "", "Oratan",
                      "Developer: comma-separated guest addresses (hex) whose big-endian u32 "
                      "values are logged whenever they change, checked once per frame. "
                      "'*PTR+OFF' reads the pointer at PTR and adds OFF");

namespace oratan {
namespace {

struct Watch {
  std::string label;
  uint32_t address;  // or the pointer's address, if deref
  uint32_t offset;
  bool deref;
  uint32_t last;
  bool seen;
};

std::vector<Watch> g_watches;
std::string g_watch_spec;

void ParseWatches(const std::string& spec) {
  g_watches.clear();
  std::stringstream ss(spec);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (item.empty()) continue;
    Watch w{item, 0, 0, false, 0, false};
    std::string spec_item = item;
    if (spec_item[0] == '*') {
      w.deref = true;
      spec_item = spec_item.substr(1);
    }
    const size_t plus = spec_item.find('+');
    try {
      w.address = uint32_t(std::stoul(spec_item.substr(0, plus), nullptr, 16));
      if (plus != std::string::npos) w.offset = uint32_t(std::stoul(spec_item.substr(plus + 1), nullptr, 16));
    } catch (const std::exception&) {
      // Thrown from the present hook, this would take the game down.
      REXLOG_ERROR("watch_u32: ignoring bad entry '{}'", item);
      continue;
    }
    g_watches.push_back(w);
  }
}

}  // namespace

void CheckWatches(const uint8_t* base, uint32_t frame) {
  const std::string spec = REXCVAR_GET(watch_u32);
  if (spec != g_watch_spec) {
    g_watch_spec = spec;
    ParseWatches(spec);
  }
  auto load = [base](uint32_t addr) {
    uint32_t v;
    std::memcpy(&v, base + addr, 4);
    return __builtin_bswap32(v);
  };
  for (auto& w : g_watches) {
    uint32_t addr = w.address;
    if (w.deref) {
      addr = load(addr);
      if (!addr) continue;  // pointer not set up yet
    }
    const uint32_t v = load(addr + w.offset);
    if (!w.seen || v != w.last) {
      REXLOG_INFO("watch frame {} {} = {:08X} ({})", frame, w.label, v, int32_t(v));
      w.last = v;
      w.seen = true;
    }
  }
}

// --- crash report: which recompiled guest function faulted ---
namespace {
std::vector<std::pair<uintptr_t, uint32_t>> g_host_to_guest;   // sorted by host address

uint32_t GuestFunctionAt(uintptr_t host) {
  auto it = std::upper_bound(g_host_to_guest.begin(), g_host_to_guest.end(),
                             std::make_pair(host, UINT32_MAX));
  if (it == g_host_to_guest.begin()) return 0;
  --it;
  // Recompiled functions are at most a few hundred KB; further away is not ours.
  return host - it->first < 0x200000 ? it->second : 0;
}

LPTOP_LEVEL_EXCEPTION_FILTER g_previous_crash_filter = nullptr;

// Unhandled-exception filter: only sees exceptions nothing else handled, so an
// access violation the game catches itself (guest SEH scopes) or the runtime
// resolves (MMIO, write watches) is never reported, and this costs nothing
// until the game is really going down.
LONG WINAPI ReportGuestCrash(EXCEPTION_POINTERS* info) {
  if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    const auto* ctx = info->ContextRecord;
    if (const uint32_t at = GuestFunctionAt(ctx->Rip)) {
      std::string chain;
      // Return addresses on this thread's stack that land in recompiled code.
      // The filter runs on the faulting thread, so its stack limits bound the
      // scan.
      ULONG_PTR low = 0, high = 0;
      GetCurrentThreadStackLimits(&low, &high);
      const uintptr_t first = ctx->Rsp & ~uintptr_t(7);
      if (first >= low && first < high) {
        const uintptr_t last = std::min<uintptr_t>(high, first + 2048 * sizeof(uintptr_t));
        int found = 0;
        for (uintptr_t p = first; p + sizeof(uintptr_t) <= last && found < 12; p += sizeof(uintptr_t)) {
          if (const uint32_t g = GuestFunctionAt(*reinterpret_cast<const uintptr_t*>(p))) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), " %08X", g);
            chain += buf;
            ++found;
          }
        }
      }
      REXLOG_ERROR(
          "guest crash: access violation {} 0x{:X} in sub_{:08X} (host rip 0x{:X}); callers "
          "(approx):{}",
          info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
          info->ExceptionRecord->ExceptionInformation[1], at, ctx->Rip, chain);
    }
  }
  return g_previous_crash_filter ? g_previous_crash_filter(info) : EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace

void InstallCrashReport() {
  static bool installed = false;
  if (installed) return;
  installed = true;
  for (const PPCFuncMapping* m = PPCFuncMappings; m->guest; ++m) {
    g_host_to_guest.emplace_back(reinterpret_cast<uintptr_t>(m->host), uint32_t(m->guest));
  }
  std::sort(g_host_to_guest.begin(), g_host_to_guest.end());
  g_previous_crash_filter = SetUnhandledExceptionFilter(ReportGuestCrash);
}

namespace {

// One --memory_excerpt_list line: where the block comes from and where it is filed.
struct ExcerptRange {
  std::string expression;
  uint32_t length;
  bool filed_elsewhere;
  uint32_t filed_at;
  bool committed_only;
  std::string line;
};

uint32_t ParseNumber(const std::string& token) { return uint32_t(std::stoul(token, nullptr, 0)); }

// Evaluates an address expression; false if a pointer on the way is unreadable.
bool EvaluateExpression(const uint8_t* base, const std::string& expression, uint32_t* out) {
  std::stringstream ss(expression);
  std::string token;
  uint32_t value = 0;
  while (ss >> token) {
    if (token == "*") {
      if (value == 0) return false;
      value = game::LoadGuestAddress(base, value);
    } else if (token[0] == '+') {
      value += ParseNumber(token.substr(1));
    } else if (token[0] == '-') {
      value -= ParseNumber(token.substr(1));
    } else {
      value = ParseNumber(token);
    }
  }
  *out = value;
  return true;
}

bool ReadExcerptList(const std::string& path, std::vector<ExcerptRange>* ranges) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    REXLOG_ERROR("memory_excerpt: can't open {}", path);
    return false;
  }
  std::string text;
  char buffer[4096];
  size_t n;
  while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, n);
  std::fclose(f);
  std::stringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const size_t hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    while (ss >> token) tokens.push_back(token);
    if (tokens.empty()) continue;
    ExcerptRange r{"", 0, false, 0, false, line};
    size_t count = tokens.size();
    try {
      if (count >= 3 && tokens[count - 1] == "committed") {
        r.committed_only = true;
        count -= 1;
      }
      if (count >= 4 && tokens[count - 2] == "as") {
        r.filed_elsewhere = true;
        r.filed_at = ParseNumber(tokens[count - 1]);
        count -= 2;
      }
      if (count < 2) throw std::invalid_argument("too short");
      r.length = ParseNumber(tokens[count - 1]);
      for (size_t i = 0; i + 1 < count; ++i) r.expression += tokens[i] + " ";
    } catch (const std::exception&) {
      REXLOG_ERROR("memory_excerpt: ignoring bad line '{}'", line);
      continue;
    }
    ranges->push_back(r);
  }
  return true;
}

void WriteLe32(FILE* f, uint32_t v) {
  const uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
  std::fwrite(b, 1, 4, f);
}

}  // namespace

void WriteMemoryExcerptIfRequested(const uint8_t* base) {
  static bool done = false;
  const std::string list = REXCVAR_GET(memory_excerpt_list);
  const std::string out = REXCVAR_GET(memory_excerpt_out);
  if (done || list.empty() || out.empty()) return;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  const auto& g = *game::GuestPtr<game::Game>(base, game_address);
  if (!game::IsFightInProgress(g, game::CurrentModule(base)) ||
      uint16_t(g.scene_step) != game::battle_step::kFight) {
    return;
  }
  done = true;
  std::vector<ExcerptRange> ranges;
  if (!ReadExcerptList(list, &ranges)) return;
  struct Block {
    uint32_t address, filed_at, length;
  };
  std::vector<Block> blocks;
  for (const ExcerptRange& r : ranges) {
    uint32_t address = 0;
    if (!EvaluateExpression(base, r.expression, &address) || address == 0) {
      REXLOG_ERROR("memory_excerpt: can't resolve '{}'", r.line);
      continue;
    }
    if (!r.committed_only) {
      blocks.push_back({address, r.filed_elsewhere ? r.filed_at : address, r.length});
      continue;
    }
    // Only the committed, readable pages of the range, one block each run.
    uint64_t at = address;
    const uint64_t end = uint64_t(address) + r.length;
    while (at < end) {
      MEMORY_BASIC_INFORMATION info;
      if (!VirtualQuery(base + at, &info, sizeof(info))) break;
      const uint64_t region_end = std::min<uint64_t>(
          end, uint64_t(static_cast<const uint8_t*>(info.BaseAddress) - base) + info.RegionSize);
      if (info.State == MEM_COMMIT && !(info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        blocks.push_back({uint32_t(at), uint32_t(at), uint32_t(region_end - at)});
      }
      at = region_end;
    }
  }
  FILE* f = std::fopen(out.c_str(), "wb");
  if (!f) {
    REXLOG_ERROR("memory_excerpt: can't write {}", out);
    return;
  }
  std::fwrite("OXMX", 1, 4, f);
  WriteLe32(f, 1);
  WriteLe32(f, uint32_t(blocks.size()));
  for (const Block& b : blocks) {
    WriteLe32(f, b.filed_at);
    WriteLe32(f, b.length);
    std::fwrite(base + b.address, 1, b.length, f);
  }
  std::fclose(f);
  REXLOG_INFO("memory_excerpt: wrote {} blocks to {}", blocks.size(), out);
}

void CaptureIfRequested(const uint8_t* base) {
  WriteMemoryExcerptIfRequested(base);
  const std::string dir = REXCVAR_GET(capture_dir);
  if (dir.empty()) return;
  static bool was_down = false;
  static int index = 0;
  const bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
  const bool pressed = down && !was_down;
  was_down = down;
  if (!pressed) return;
  const std::string prefix = dir + "/cap" + std::to_string(index);
  if (FILE* f = std::fopen((prefix + "_data.bin").c_str(), "wb")) {
    std::fwrite(base + 0x82890000, 1, 0x01171DFC, f);
    std::fclose(f);
  }
  // Committed memory only, as sparse {u32 guest address, u32 size, bytes} records.
  if (FILE* f = std::fopen((prefix + "_mem.bin").c_str(), "wb")) {
    uint64_t addr = 0xA4000000;
    const uint64_t end = 0xAE000000;
    while (addr < end) {
      MEMORY_BASIC_INFORMATION info;
      if (!VirtualQuery(base + addr, &info, sizeof(info))) break;
      const uint64_t region_end = std::min<uint64_t>(
          end, uint64_t(static_cast<const uint8_t*>(info.BaseAddress) - base) + info.RegionSize);
      if (info.State == MEM_COMMIT && !(info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        const uint32_t header[2] = {uint32_t(addr), uint32_t(region_end - addr)};
        std::fwrite(header, 4, 2, f);
        std::fwrite(base + addr, 1, size_t(region_end - addr), f);
      }
      addr = region_end;
    }
    std::fclose(f);
  }
  REXLOG_INFO("capture: wrote {}_*.bin", prefix);
  ++index;
}

void SaveSnapshot(const uint8_t* base, const std::string& dir) {
  static int index = 0;
  auto write = [](const std::string& path, const uint8_t* data, size_t size) {
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
      std::fwrite(data, 1, size, f);
      std::fclose(f);
    }
  };
  ++index;
  // .data/.bss of the XEX, and the battle state the game pointer refers to.
  constexpr uint32_t kDataStart = 0x82890000, kDataSize = 0x01171DFC;
  write(dir + "/snap" + std::to_string(index) + "_data.bin", base + kDataStart, kDataSize);
  if (const uint32_t game = game::LoadGuestAddress(base, game::kGamePointer)) {
    write(dir + "/snap" + std::to_string(index) + "_game.bin", base + game, 0x40000);
  }
  REXLOG_INFO("snapshot {} saved to {}", index, dir);
}

void DumpGuestImageIfRequested(rex::Runtime* runtime) {
  const std::string path = REXCVAR_GET(dump_image);
  if (path.empty() || !runtime || !runtime->memory()) return;

  constexpr uint32_t kImageBase = 0x82000000;
  constexpr uint32_t kImageSize = 0x01A30000;
  const uint8_t* src = runtime->memory()->TranslateVirtual(kImageBase);

  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) {
    REXLOG_ERROR("dump_image: cannot open {}", path);
    return;
  }
  std::fwrite(src, 1, kImageSize, f);
  std::fclose(f);
  REXLOG_INFO("dump_image: wrote {:#x} bytes to {}", kImageSize, path);
}

}  // namespace oratan
