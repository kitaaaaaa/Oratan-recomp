// Developer tools for working on the recompilation (not used in normal play).

#pragma once

#include <cstdint>
#include <string>

namespace rex {
class Runtime;
}

namespace oratan {

// Writes the loaded guest image to the file named by --dump_image, if set.
void DumpGuestImageIfRequested(rex::Runtime* runtime);

// Writes the XEX data section and the battle state to DIR/snapN_*.bin.
void SaveSnapshot(const uint8_t* base, const std::string& dir);

// Logs changes to the addresses listed in --watch_u32. Called once per frame.
void CheckWatches(const uint8_t* base, uint32_t frame);

}  // namespace oratan
