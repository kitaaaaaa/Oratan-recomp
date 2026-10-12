// Local two-player versus (see local_versus.cpp).

#pragma once

#include <cstdint>

namespace oratan {

// After the main menu's side choice (sub_8213CCD0, src/decomp/menu.cpp): with
// local_versus on, gives the other side the second controller.
void OnSideChosen(uint8_t* base, uint32_t side);

}  // namespace oratan
