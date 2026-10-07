// Developer tools for working on the recompilation (not used in normal play).

#include "debug_tools.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/ppc/func.h>
#include <rex/system/xmemory.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <sstream>
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

LONG CALLBACK ReportGuestCrash(EXCEPTION_POINTERS* info) {
  if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
  const auto* ctx = info->ContextRecord;
  const uint32_t at = GuestFunctionAt(ctx->Rip);
  if (!at) return EXCEPTION_CONTINUE_SEARCH;
  std::string chain;
  // Return addresses on the host stack that land in recompiled code.
  const auto* sp = reinterpret_cast<const uintptr_t*>(ctx->Rsp);
  int found = 0;
  for (int i = 0; i < 2048 && found < 12; ++i) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(sp + i, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) break;
    if (const uint32_t g = GuestFunctionAt(sp[i])) {
      char buf[16];
      std::snprintf(buf, sizeof(buf), " %08X", g);
      chain += buf;
      ++found;
    }
  }
  REXLOG_ERROR("guest crash: access violation {} 0x{:X} in sub_{:08X} (host rip 0x{:X}); callers (approx):{}",
               info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
               info->ExceptionRecord->ExceptionInformation[1], at, ctx->Rip, chain);
  return EXCEPTION_CONTINUE_SEARCH;
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
  // Last in the chain: the runtime's own handler (MMIO, write watches) runs first.
  AddVectoredExceptionHandler(0, ReportGuestCrash);
}

void CaptureIfRequested(const uint8_t* base) {
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
  auto load = [base](uint32_t addr) {
    uint32_t v;
    std::memcpy(&v, base + addr, 4);
    return __builtin_bswap32(v);
  };
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
  if (const uint32_t game = load(0x839EEA98)) {
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
