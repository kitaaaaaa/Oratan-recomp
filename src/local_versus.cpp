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
#include <rex/system/split_screen.h>

#include <cstdint>
#include <cstring>
#include <string>

REXCVAR_DEFINE_BOOL(local_versus, false, "Oratan",
                    "Experimental: controller 2 controls the opponent robot (local versus)");

// Camera selector used by the replay/observer viewer: game+53125 =
// 0 GAME CAMERA DNA (robot slot 0), 1 GAME CAMERA RNA (slot 1),
// 2 LIVE MONITOR (sub_822ADCF0 cycles it, sub_822057B0 labels it).
REXCVAR_DEFINE_INT32(view_camera, -1, "Oratan",
                     "Developer: force the battle camera (-1 game default, 0 robot 1, "
                     "1 robot 2, 2 live monitor)");

REXCVAR_DEFINE_BOOL(split_screen, false, "Oratan",
                    "Experimental: during battles, draw each robot's view and show them "
                    "side by side (left = robot 1, right = robot 2)");

REXCVAR_DEFINE_INT32(split_display, 0, "Oratan",
                     "Split screen display: 0 staggered on one screen (robot 1 top-left, "
                     "robot 2 bottom-right), 1 side by side on one screen, 2 one monitor per "
                     "player (window spans two side-by-side monitors)")
    .range(0, 2);

REX_EXTERN(__imp__sub_8213CCD0);
REX_EXTERN(__imp__sub_82123758);
REX_EXTERN(__imp__sub_822A71B0);

namespace {

constexpr uint32_t kSideInputBase = 0x829BEFA8;
constexpr uint32_t kSideInputStride = 316;
constexpr uint32_t kPadIndexOffset = 300;

constexpr uint32_t kGamePointer = 0x839EEA98;
constexpr uint32_t kCameraOffset = 53125;
constexpr uint32_t kModuleIndex = 0x839C21BC;  // byte, see IsBattleModule

// Battles run in the GAME module (1: Arcade, Score Attack) and the NETVS
// module (2: Training, online versus).
bool IsBattleModule(const uint8_t* base) {
  const uint8_t module = base[kModuleIndex];
  return module == 1 || module == 2;
}

// View the frame being drawn right now belongs to (split screen), or
// kNoView; read by the present hook when it tags the swap.
int32_t g_drawing_view = rex::system::split_screen::kNoView;
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
  if (const int32_t camera = REXCVAR_GET(view_camera);
      camera >= 0 && g_drawing_view == rex::system::split_screen::kNoView) {
    if (const uint32_t game = LoadU32(base, kGamePointer)) base[game + kCameraOffset] = uint8_t(camera);
  }
  // Sides whose CPU flag we cleared in the current battle, so turning the
  // option off mid-battle can hand them back to the AI instead of leaving the
  // robot frozen.
  static bool switched[2] = {false, false};
  const uint32_t game = LoadU32(base, kGamePointer);
  const bool in_battle = game && IsBattleModule(base);
  if (!in_battle) {
    switched[0] = switched[1] = false;  // the next battle sets its own flags
    return;
  }
  for (int side = 0; side < 2; ++side) {
    uint8_t* cpu_flag = base + game + kCpuFlagOffset[side];
    if (REXCVAR_GET(local_versus)) {
      if (*cpu_flag == 1) {
        *cpu_flag = 0;
        switched[side] = true;
        REXLOG_INFO("local_versus: side {} robot switched from CPU to controller", side);
      }
    } else if (switched[side]) {
      *cpu_flag = 1;
      switched[side] = false;
      REXLOG_INFO("local_versus: side {} robot handed back to the CPU", side);
    }
  }
}

// sub_82123758: the app's Draw (vtable slot 4 of the run loop object); renders
// the frame and presents it. For split screen it runs once per robot camera.
REX_HOOK_RAW(sub_82123758) {
  const uint32_t game = LoadU32(base, kGamePointer);
  const bool active = REXCVAR_GET(split_screen) && game && IsBattleModule(base);
  // Layout for the GPU side to compose with (runtime state, cheap to refresh).
  const int32_t display = REXCVAR_GET(split_display);
  rex::system::split_screen::SetLayout(display == 0 ? rex::system::split_screen::Layout::kStaggered
                                                    : rex::system::split_screen::Layout::kSideBySide);
  // One monitor per player: everything outside split battles is shown on both.
  rex::system::split_screen::SetMirrorUntagged(REXCVAR_GET(split_screen) && display == 2);
  static int last_state = -1;
  const int state = (REXCVAR_GET(split_screen) ? 4 : 0) | (game ? 2 : 0) |
                    (IsBattleModule(base) ? 1 : 0);
  if (state != last_state) {
    REXLOG_INFO("split_screen: enabled={} game={:08X} module={} -> {}", REXCVAR_GET(split_screen),
                game, base[kModuleIndex], active ? "split" : "normal");
    last_state = state;
    // The game waits for one vertical blank per presented frame; drawing every
    // view per tick needs that many vblanks per tick to keep normal speed.
    // This is runtime-only state (a cvar write would end up in the saved
    // config and run the next session's menus at double speed).
    rex::system::split_screen::SetVblankMultiplier(active ? rex::system::split_screen::kViewCount
                                                          : 1);
  }
  if (!active) {
    g_drawing_view = rex::system::split_screen::kNoView;
    __imp__sub_82123758(ctx, base);
    return;
  }
  uint8_t& camera = base[game + kCameraOffset];
  const uint8_t saved_camera = camera;
  // Each view's Draw gets the same arguments: the first call can clobber any
  // volatile register (r3-r12, f0-f13, v0-v19), not just r3, and the
  // callee's signature is not known. Callee-saved registers come back
  // unchanged anyway, so restoring the whole context is a plain repeat call.
  const PPCContext saved_ctx = ctx;
  // The second view follows robot 2 when player 2 controls it; otherwise it
  // shows the LIVE MONITOR camera, like the arcade's second cabinet.
  const uint8_t second_camera = REXCVAR_GET(local_versus) ? 1 : 2;
  for (int32_t view = 0; view < rex::system::split_screen::kViewCount; ++view) {
    camera = view == 0 ? 0 : second_camera;
    g_drawing_view = view;
    ctx = saved_ctx;
    __imp__sub_82123758(ctx, base);
  }
  camera = saved_camera;
  g_drawing_view = rex::system::split_screen::kNoView;
}

namespace oratan {

// Called when the game's present starts, before it issues VdSwap: names the
// view being drawn. VdSwap queues it with the swap itself, so a present that
// returns without swapping cannot shift later tags onto the wrong side (see
// rex/system/split_screen.h).
void TagSwap() { rex::system::split_screen::SetNextSwapView(g_drawing_view); }

// View being drawn right now (split screen), or -1.
int32_t CurrentDrawView() { return g_drawing_view; }

}  // namespace oratan
