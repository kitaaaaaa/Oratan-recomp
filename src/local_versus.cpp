// Local two-player versus (experimental).
//
// Input: the game keeps a 316-byte input record per side at 0x829BEFA8
// (side 0 = 1P/DNA, side 1 = 2P/RNA); +300 is the controller index the side
// reads. Choosing a side in the main menu (sub_8213CCD0) gives that side the
// active controller and sets the other side to -1, so a second controller is
// ignored. With local_versus on, the other side keeps the second controller.
//
// Control: every frame sub_822A71B0 turns both sides' controller state into
// robot command words, but a robot whose "CPU" byte is 1 ignores them and
// runs the AI. The battle state ("game") is reached through the pointer at
// 0x839EEA98; the CPU bytes are at +25021 (side 0) and +29517 (side 1), one
// 4496-byte robot record apart. With local_versus on, both are cleared after
// the commands are built, so each robot follows its own controller.

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include <cstdint>
#include <cstring>

REXCVAR_DEFINE_BOOL(local_versus, false, "Oratan",
                    "Experimental: controller 2 controls the opponent robot (local versus)");

REX_EXTERN(__imp__sub_8213CCD0);
REX_EXTERN(__imp__sub_822A71B0);

namespace {

constexpr uint32_t kSideInputBase = 0x829BEFA8;
constexpr uint32_t kSideInputStride = 316;
constexpr uint32_t kPadIndexOffset = 300;

constexpr uint32_t kGamePointer = 0x839EEA98;
constexpr uint32_t kCpuFlagOffset[2] = {25021, 29517};

uint32_t LoadU32(const uint8_t* base, uint32_t addr) {
  uint32_t v;
  std::memcpy(&v, base + addr, 4);
  return __builtin_bswap32(v);
}

void StoreS32(uint8_t* base, uint32_t addr, int32_t value) {
  const uint32_t v = __builtin_bswap32(uint32_t(value));
  std::memcpy(base + addr, &v, 4);
}

}  // namespace

// sub_8213CCD0(side): assigns the active controller to `side`.
REX_HOOK_RAW(sub_8213CCD0) {
  const uint32_t side = ctx.r3.u32;
  __imp__sub_8213CCD0(ctx, base);
  if (!REXCVAR_GET(local_versus) || side > 1) return;

  const uint32_t own = kSideInputBase + side * kSideInputStride + kPadIndexOffset;
  const uint32_t other = kSideInputBase + (1 - side) * kSideInputStride + kPadIndexOffset;
  const int32_t own_pad = int32_t(LoadU32(base, own));
  if (own_pad < 0) return;
  const int32_t other_pad = own_pad == 0 ? 1 : 0;
  StoreS32(base, other, other_pad);
  REXLOG_INFO("local_versus: side {} uses pad {}, side {} uses pad {}", side, own_pad, 1 - side,
              other_pad);
}

// Gives a side without a controller (-1) the one the other side isn't using,
// so turning the option on mid-session works (controllers are normally
// handed out when Start is pressed on the title screen).
static void AssignMissingControllers(uint8_t* base) {
  int32_t pad[2];
  for (int side = 0; side < 2; ++side) {
    pad[side] = int32_t(LoadU32(base, kSideInputBase + side * kSideInputStride + kPadIndexOffset));
  }
  for (int side = 0; side < 2; ++side) {
    if (pad[side] >= 0 || pad[1 - side] < 0) continue;
    pad[side] = pad[1 - side] == 0 ? 1 : 0;
    StoreS32(base, kSideInputBase + side * kSideInputStride + kPadIndexOffset, pad[side]);
    REXLOG_INFO("local_versus: side {} given controller {}", side, pad[side]);
  }
}

// sub_822A71B0: builds both sides' robot command words for this frame.
REX_HOOK_RAW(sub_822A71B0) {
  if (REXCVAR_GET(local_versus)) AssignMissingControllers(base);
  __imp__sub_822A71B0(ctx, base);
  if (!REXCVAR_GET(local_versus)) return;

  const uint32_t game = LoadU32(base, kGamePointer);
  if (!game) return;
  for (int side = 0; side < 2; ++side) {
    uint8_t* cpu_flag = base + game + kCpuFlagOffset[side];
    if (*cpu_flag == 1) {
      *cpu_flag = 0;
      REXLOG_INFO("local_versus: side {} robot switched from CPU to controller", side);
    }
  }
}
