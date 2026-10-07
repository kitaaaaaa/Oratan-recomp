// Extra choices on the stage select and character select: things the game has
// but does not list. LB/RB (or Page Up / Page Down) switches them in.
//
// Stage select (Training): a carousel driven by a 17-entry table at 0x8203FCA8
// (24 bytes each: stage number, preview image x/y and sheet, name, panel image
// id; entry 0 is RANDOM, stage -1). On confirm the game stores the entry's
// stage number in 0x83876DF4. The carousel code (per-entry state arrays, angle
// step, modulo 17) assumes exactly 17 entries, so the switch swaps the first
// entries after RANDOM between the normal stages and the unlisted ones, which
// show a related stage's card (they have no preview or panel art of their own).
// Tested by the user: DISTORTED SHRINE (10) and TANGRAM (21) work. SPACE
// CARRIER (16, no files), TANGRAM CORE (22, past the end of the stage name and
// per-stage tables), SELECT (33) and CONTINUE (34) crash, and the attract-mode
// tutorial scenery ("man") has no terrain grid, so those are left out.
//
// Character select (Training): a 2 x 9 grid; each side's cursor (int, state
// 0x82B035EC + 0x168 / +0x16C) indexes a robot byte table at 0x82010698
// (sub_8218D5E8; column 8 is RANDOM). The switch toggles AJIM's slot (7)
// between AJIM and BRADTOS (17), the boss, otherwise never playable. (The
// Arcade select uses a different table, 0x8203B978.)
// Each cell's portrait is a sprite id from a u32 table at 0x820106B0 (18 cells
// for side 1, then 18 for side 2): robot n uses 0x792 + 2n (+1 on side 2), and
// AJIM uses BAL-KEROS' id, as BAL-KEROS was never selectable. BRADTOS has no
// portrait, so its cell shows another sprite the screen has (0x72C).

#include <rex/hook.h>
#include <rex/logging.h>

#include <cstdint>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

REX_EXTERN(__imp__sub_8220EC30);
REX_EXTERN(__imp__sub_8220E528);
REX_EXTERN(__imp__sub_8218E4C0);

namespace {

uint32_t LoadU32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return __builtin_bswap32(v);
}
void StoreS32(uint8_t* p, int32_t v) {
  const uint32_t b = __builtin_bswap32(uint32_t(v));
  std::memcpy(p, &b, 4);
}
void MakeWritable(uint8_t* p, size_t n) {
  DWORD old;
  VirtualProtect(p, n, PAGE_READWRITE, &old);
}

// --- input: LB/RB on any controller, Page Up/Down when the game has focus ---
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, void*);
struct PadState {
  DWORD packet;
  WORD buttons;
  BYTE lt, rt;
  SHORT lx, ly, rx, ry;
};
XInputGetStateFn LoadXInput() {
  static XInputGetStateFn fn = [] {
    for (const char* dll : {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"}) {
      if (HMODULE m = LoadLibraryA(dll)) {
        if (auto f = reinterpret_cast<XInputGetStateFn>(GetProcAddress(m, "XInputGetState"))) return f;
      }
    }
    return XInputGetStateFn(nullptr);
  }();
  return fn;
}
bool GameHasFocus() {
  DWORD pid = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &pid);
  return pid == GetCurrentProcessId();
}
// True once per press. Each screen keeps its own edge state.
bool SwitchPressed(bool& was_down) {
  bool down = false;
  if (auto get = LoadXInput()) {
    for (DWORD pad = 0; pad < 4; ++pad) {
      PadState s{};
      if (get(pad, &s) == ERROR_SUCCESS && (s.buttons & (0x0100 | 0x0200))) down = true;  // LB/RB
    }
  }
  if (GameHasFocus() && ((GetAsyncKeyState(VK_PRIOR) | GetAsyncKeyState(VK_NEXT)) & 0x8000)) down = true;
  const bool pressed = down && !was_down;
  was_down = down;
  return pressed;
}

// --- stage select ---
constexpr uint32_t kStageTable = 0x8203FCA8;
constexpr uint32_t kEntrySize = 24;
constexpr uint32_t kEntries = 17;
constexpr uint32_t kStageVar = 0x83876DF4;

struct ExtraStage {
  int32_t stage;
  uint32_t card_from;   // entry whose preview and panel it shows
  const char* name;
};
constexpr ExtraStage kExtraStages[] = {
    {10, 10, "DISTORTED SHRINE"},  // card: SANCTUARY2
    {21, 3, "TANGRAM"},            // card: SPACE STATION
};

uint8_t g_stage_page1[kEntries * kEntrySize];
uint8_t g_stage_page2[kEntries * kEntrySize];
bool g_stage_built = false;
int g_stage_page = 0;

// --- character select ---
constexpr uint32_t kRoster = 0x82010698;
constexpr uint32_t kCursorOffsets[] = {0x168, 0x16C};   // in the select state (r3)
constexpr uint32_t kPortraits = 0x820106B0;    // u32 sprite ids, 2 x 18 cells
constexpr uint32_t kCellsPerSide = 18;
struct CyclingSlot {
  uint32_t slot;
  int count;
  uint8_t robots[3];
  uint32_t portraits[3];   // side-1 sprite ids; side 2 uses the next id, except the shared tiles
  const char* names[3];
  int choice;
};
// Tested by the user: BRADTOS plays on any stage (as the AI it also plays its
// Arcade intro and defeat scenes). Left out: TANGRAM (18; empty motion file,
// crashes as the fight starts in sub_82240808, the animation lookup), and
// BAL-KEROS (5) / BAL-BAROS (16), BAL-BADOS' stage forms: picked on other
// stages BAL-KEROS crashes when dashing and BAL-BAROS' LW shot is invisible
// (they rely on their own stages; the robot loader's sub_821C5060 normally
// maps BAL-BADOS to the form for the stage).
CyclingSlot g_cycling[] = {
    {7, 2, {15, 17}, {0x79C, 0x72C}, {"AJIM", "BRADTOS"}, 0},
};
void SetPortrait(uint8_t* base, uint32_t slot, uint32_t sprite) {
  const bool shared = sprite == 0x72B || sprite == 0x72C;   // same tile on both sides
  for (uint32_t side = 0; side < 2; ++side) {
    uint8_t* p = base + kPortraits + 4 * (side * kCellsPerSide + slot);
    MakeWritable(p, 4);
    const uint32_t v = __builtin_bswap32(sprite + (shared ? 0 : side));
    std::memcpy(p, &v, 4);
  }
}

}  // namespace

// sub_8220EC30: stage select per-frame update (cursor movement).
REX_HOOK_RAW(sub_8220EC30) {
  uint8_t* table = base + kStageTable;
  if (!g_stage_built) {
    MakeWritable(table, sizeof(g_stage_page1));
    std::memcpy(g_stage_page1, table, sizeof(g_stage_page1));
    std::memcpy(g_stage_page2, table, sizeof(g_stage_page2));
    for (uint32_t i = 0; i < sizeof(kExtraStages) / sizeof(kExtraStages[0]); ++i) {
      uint8_t* e = g_stage_page2 + (1 + i) * kEntrySize;   // slots after RANDOM
      std::memcpy(e, g_stage_page1 + kExtraStages[i].card_from * kEntrySize, kEntrySize);
      StoreS32(e, kExtraStages[i].stage);
    }
    g_stage_built = true;
  }
  static bool was_down = false;
  if (SwitchPressed(was_down)) {
    g_stage_page ^= 1;
    std::memcpy(table, g_stage_page ? g_stage_page2 : g_stage_page1, sizeof(g_stage_page1));
    REXLOG_INFO("stage select: page {}", g_stage_page + 1);
  }
  __imp__sub_8220EC30(ctx, base);
}

// sub_8220E528: stage select update/confirm (runs every frame); logs changes
// of the chosen stage.
REX_HOOK_RAW(sub_8220E528) {
  __imp__sub_8220E528(ctx, base);
  const int32_t stage = int32_t(LoadU32(base + kStageVar));
  static int32_t last = -2;
  if (stage == last) return;
  last = stage;
  const char* extra = "";
  for (const auto& e : kExtraStages) {
    if (e.stage == stage) extra = e.name;
  }
  REXLOG_INFO("stage select: stage {} {}", stage, extra);
}

// sub_8218E4C0: Training character select update (r3 = select state).
REX_HOOK_RAW(sub_8218E4C0) {
  static bool was_down = false;
  if (SwitchPressed(was_down)) {
    const uint32_t state = ctx.r3.u32;
    for (auto& c : g_cycling) {
      bool under_cursor = false;
      for (uint32_t off : kCursorOffsets) {
        if (int32_t(LoadU32(base + state + off)) == int32_t(c.slot)) under_cursor = true;
      }
      if (!under_cursor) continue;
      uint8_t* slot = base + kRoster + c.slot;
      MakeWritable(slot, 1);
      c.choice = (c.choice + 1) % c.count;
      *slot = c.robots[c.choice];
      SetPortrait(base, c.slot, c.portraits[c.choice]);
      REXLOG_INFO("character select: slot {} is now {}", c.slot, c.names[c.choice]);
    }
  }
  __imp__sub_8218E4C0(ctx, base);
}



