// Developer tools for working on the recompilation (not used in normal play).

#include "debug_tools.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

REXCVAR_DEFINE_STRING(dump_image, "", "Oratan",
                      "Write the loaded XEX image (0x82000000-0x83A30000, big-endian) "
                      "to this file after load, for offline analysis");

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
    w.address = uint32_t(std::stoul(spec_item.substr(0, plus), nullptr, 16));
    if (plus != std::string::npos) w.offset = uint32_t(std::stoul(spec_item.substr(plus + 1), nullptr, 16));
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
