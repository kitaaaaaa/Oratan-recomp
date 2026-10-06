// Developer tools for working on the recompilation (not used in normal play).

#include "debug_tools.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

#include <cstdio>
#include <string>

REXCVAR_DEFINE_STRING(dump_image, "", "Oratan",
                      "Write the loaded XEX image (0x82000000-0x83A30000, big-endian) "
                      "to this file after load, for offline analysis");

namespace oratan {

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
