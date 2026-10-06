// Developer tools for working on the recompilation (not used in normal play).

#pragma once

#include <cstdint>

namespace rex {
class Runtime;
}

namespace oratan {

// Writes the loaded guest image to the file named by --dump_image, if set.
void DumpGuestImageIfRequested(rex::Runtime* runtime);

// Logs changes to the addresses listed in --watch_u32. Called once per frame.
void CheckWatches(const uint8_t* base, uint32_t frame);

}  // namespace oratan
