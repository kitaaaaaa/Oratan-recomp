// Local two-player versus (see local_versus.cpp).

#pragma once

#include <cstdint>

namespace oratan {

// After the main menu's side choice (sub_8213CCD0, src/decomp/menu.cpp): with
// local_versus on, gives the other side the second controller.
void OnSideChosen(uint8_t* base, uint32_t side);

// Around the robot command words (sub_822A71B0, src/decomp/robot_control.cpp):
// before, gives a side without a controller one; after, applies view_camera
// and hands robots to their controllers (or back to the CPU). Both only act
// with local versus or netplay on (except view_camera).
void BeforeRobotCommands(uint8_t* base);
void AfterRobotCommands(uint8_t* base);

}  // namespace oratan
