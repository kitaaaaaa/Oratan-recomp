// Developer tools for working on the recompilation (not used in normal play).

#pragma once

namespace rex {
class Runtime;
}

namespace oratan {

// Writes the loaded guest image to the file named by --dump_image, if set.
void DumpGuestImageIfRequested(rex::Runtime* runtime);

}  // namespace oratan
