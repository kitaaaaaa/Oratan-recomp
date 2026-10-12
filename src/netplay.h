// Netplay (experimental): lockstep online versus over UDP (see netplay.cpp).

#pragma once

#include <cstdint>

namespace oratan {

// True while connected and running in lockstep with the other player. Any
// thread.
bool NetplayActive();

// The controller slot this machine's player drives in the shared input
// (0 = host, 1 = joiner), or -1 when netplay is off.
int32_t NetplayLocalSlot();

}  // namespace oratan
