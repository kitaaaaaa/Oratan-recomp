// The game's own data in guest memory, as C++ types.
//
// Everything mapped so far about Virtual-On Oratorio Tangram's memory (see
// notes/DEV_DIARY.md, section 3): fixed addresses, and structures with their
// known fields at their real offsets. Unknown stretches are padding named by
// their offset, so a newly found field just replaces part of one. Each known
// offset is checked at compile time.
//
// Guest memory is big-endian: multi-byte fields are rex::be<T>, which
// byte-swaps on every read and write. Guest pointers are 32-bit guest
// addresses (be<uint32_t>); turn them into host pointers with GuestPtr.

#pragma once

#include <rex/types.h>

#include <cstddef>
#include <cstdint>

namespace oratan::game {

using rex::be;

// --- Addresses ---------------------------------------------------------------

// u32 guest pointer to the battle state (Game), 0 outside battles.
inline constexpr uint32_t kGamePointer = 0x839EEA98;
// u8: the running module (Module), 0xFF if the switcher didn't know the name.
inline constexpr uint32_t kModuleIndex = 0x839C21BC;
// be<uint32_t>[]: guest pointers to the module file names ("OPEN.BIN" ...),
// in Module order, ending with 0. The switcher looks names up here.
inline constexpr uint32_t kModuleFileNames = 0x8298AC9C;
// SideInput[2]: each side's input record.
inline constexpr uint32_t kSideInputs = 0x829BEFA8;
// ControllerState[4]: controllers as read by sub_821135C0.
inline constexpr uint32_t kControllerStates = 0x829BF2A8;
// be<uint32_t>: the active controller, 0-3; any other value means none
// (sub_82114C88 reads it as -1). Set when the title screen hands out
// controllers; sub_8213CCD0 gives it to the side chosen in the main menu.
inline constexpr uint32_t kActiveController = 0x8289030C;
// ControllerMasks: per-controller bit masks (bit n = controller n).
inline constexpr uint32_t kControllerMasks = 0x829B2268;
// PlayerActions[2]: each player's input actions (sub_82187C28 returns side's
// record; any other side gets side 0's). sub_822AABE0 builds the robot
// command word from them.
inline constexpr uint32_t kPlayerActions = 0x82AFF1E0;
// OnlineInput: a second source of command words that replaces the players'
// when active (sub_8213D830/890/8E0); probably Xbox Live versus.
inline constexpr uint32_t kOnlineInput = 0x829BF3A0;
// CommandCapture: when `active` is set, sub_822A71B0 only stores side 0's
// command word here and builds no robot commands (meaning not yet known).
inline constexpr uint32_t kCommandCapture = 0x839C28AC;
// Stored four times into Game::input_sources each tick (sub_82113A98 returns
// it whatever its argument); what it points to isn't known yet.
inline constexpr uint32_t kInputSource = 0x829B2DE0;
// be<int32_t>: stage number of the next fight (the Training stage select stores it here).
inline constexpr uint32_t kNextStage = 0x83876DF4;
// be<uint32_t>[17]: guest pointers handed out by sub_822AC098(i): 0 = Game
// (kGamePointer), 16 = an object whose +0x181DD0 / +0x181EC0 holds the
// per-side direction tables read on battle step 0x13 (sub_82200430/498).
inline constexpr uint32_t kObjectTable = kGamePointer;
// be<int32_t>: below 0 means those tables are not in use; be<uint32_t> at
// kScriptedIndex: the current entry.
inline constexpr uint32_t kScriptedActive = 0x8386E9CC;
inline constexpr uint32_t kScriptedIndex = 0x8386E9D8;
// DirectionLookup x2: turn a robot's packed lever word (RobotInput::held) into
// a direction code, the first lever's in the high byte and the second's in
// the low byte (sub_821FB390, sub_82212A98, sub_821CBB28).
inline constexpr uint32_t kDirectionLookupFirst = 0x8203ADA4;
inline constexpr uint32_t kDirectionLookupSecond = 0x8203ADD4;
// ChargeTable: the charge parameters sub_821F9DE0 uses this tick, pointed at
// one of the robot's ChargeParams sets (or a fixed one) by sub_821FB390.
inline constexpr uint32_t kChargeTable = 0x8386E948;
// be<int32_t>[8] each: quarter-rate copies of the robot's normal set, used
// while RobotControl::action is 0 and unk_1C2 is above 0.
inline constexpr uint32_t kChargeQuarterDrop = 0x8386E958;
inline constexpr uint32_t kChargeQuarterDecay = 0x8386E978;
inline constexpr uint32_t kChargeQuarterBuild = 0x8386E998;
// be<int32_t>[8] each: fixed rates the three tables are set to in some states
// (all three pointers get the same table).
inline constexpr uint32_t kChargeFixedA = 0x8289CD14;  // RobotControl::unk_1BE set
inline constexpr uint32_t kChargeFixedB = 0x8289CD34;  // action 4/11/12, unk_1C2 negative
inline constexpr uint32_t kChargeFixedC = 0x8289CD54;  // action 11, unk_1C2 above 0
// AiControl per side: the CPU player's state (sub_82212A98).
inline constexpr uint32_t kAiControl1 = 0x83876CC8;
inline constexpr uint32_t kAiControl2 = 0x83876BA8;
// The game's random numbers: sub_822A7460 counts calls in kRandomCalls
// (be<uint32_t>) and returns 15 bits from sub_821721E8(1), a Mersenne Twister
// (sub_8215C540) whose state is on the heap: entry n of the 16-byte records
// at *kRandomGenerators holds its address at +4. The state is 624 words plus
// the index at +2500.
inline constexpr uint32_t kRandomCalls = 0x839C28E0;
inline constexpr uint32_t kRandomGenerators = 0x82A3D74C;
inline constexpr uint32_t kRandomStateSize = 2504;
// PlaybackInput[2] (144 bytes apart): a third input source a robot reads in
// RobotControl mode 5 (sub_821CBB28); who fills it isn't known yet.
inline constexpr uint32_t kPlaybackInput = 0x83865120;
// be<uint32_t>[22]: guest pointers to stage file names (stage -> "carrier", "duct", ...).
inline constexpr uint32_t kStageFileNames = 0x8201F530;
// be<uint32_t>[19]: guest pointers to robot file names (robot -> "dor" ... "tgm").
inline constexpr uint32_t kRobotFileNames = 0x8201F588;
// 300-byte stage records (display name, announcer lines, music); layout unmapped.
inline constexpr uint32_t kStageRecords = 0x82898AF8;
inline constexpr uint32_t kStageRecordSize = 300;
// StageCard[17]: the Training stage select carousel.
inline constexpr uint32_t kStageCarousel = 0x8203FCA8;
inline constexpr uint32_t kStageCarouselEntries = 17;
// uint8_t[18]: Training character select grid (2 rows x 9), robot per cell; column 8 is RANDOM.
inline constexpr uint32_t kTrainingRoster = 0x82010698;
inline constexpr uint32_t kTrainingRosterCells = 18;
// be<uint32_t>[2][18]: portrait sprite id per Training select cell (side 1's 18, then side 2's).
inline constexpr uint32_t kTrainingPortraits = 0x820106B0;
// TrainingCharSelect: the Training character select's state (sub_8218E4C0's r3).
inline constexpr uint32_t kTrainingCharSelect = 0x82B035EC;
// The Arcade character select's robot table; layout not yet mapped.
inline constexpr uint32_t kArcadeRoster = 0x8203B978;
// The engine's per-frame draw list: guest pointer to DrawRecord[count], and count.
inline constexpr uint32_t kDrawListPointer = 0x839EFB78;
inline constexpr uint32_t kDrawListCount = 0x839EFB7C;

// --- Enumerations -----------------------------------------------------------

// The byte at kModuleIndex. The switcher (sub_822A6A30, src/decomp/game_flow.cpp)
// takes the module's file name ("GAME.BIN") and the scene and step to start in.
enum class Module : uint8_t {
  kOpen = 0,       // title, main menu, attract demo
  kGame = 1,       // Arcade and Score Attack battles, Continue, Game Over
  kNetVs = 2,      // Training (and online) battles and their select screens
  kChrSel = 3,     // Arcade character select
  kEnding = 4,     // endings
  kCustomize = 5,  // Customize
  kMTest = 6,      // removed test menu
};

// Game::camera_view: whose view a frame is drawn from.
enum class CameraView : uint8_t {
  kRobot1 = 0,       // GAME CAMERA DNA (side 1)
  kRobot2 = 1,       // GAME CAMERA RNA (side 2)
  kLiveMonitor = 2,  // LIVE MONITOR (observer camera)
};

// Game::scene: the scene of the running module, an index into the module's
// scene table (each module runs its own; Game::scene_step counts through the
// scene). Values seen in the logs (notes/DEV_DIARY.md section 7):
namespace scene_id {
inline constexpr int16_t kMenu = 1;         // OPEN: title and main menu
inline constexpr int16_t kCharSelect = 0;   // CHRSEL: Arcade character select
inline constexpr int16_t kBattleEnter = 2;  // GAME / NETVS: step 9 for one frame before
                                            // kBattle; step 0 after quitting a fight
inline constexpr int16_t kBattle = 3;       // GAME / NETVS: a stage, from loading to
                                            // Continue / Game Over (see battle_step)
inline constexpr int16_t kTrainingMenu = 5;  // NETVS: Training's select screens (also 0 on entry)
inline constexpr int16_t kTrainingAfterFight = 4;  // NETVS: Training's after-fight menu
}  // namespace scene_id

// Game::scene_step during scene_id::kBattle. Each value lasts until the next.
namespace battle_step {
inline constexpr uint16_t kLoad = 0x09;         // stage load; 0x0A follows next frame
inline constexpr uint16_t kIntro = 0x0B;        // stage intro (~4 s)
inline constexpr uint16_t kRoundStart = 0x0C;   // "ready" (0x0D, 0x0E follow, ~2 s)
inline constexpr uint16_t kFight = 0x0F;        // fighting (pause doesn't change it)
inline constexpr uint16_t kDecided = 0x10;      // fight won or lost (0x11 next frame)
inline constexpr uint16_t kAftermath = 0x12;    // ~1.5 s later (0x13 next frame)
inline constexpr uint16_t kResults = 0x14;      // ~6.5 s; a win goes back to kLoad
inline constexpr uint16_t kContinue = 0x15;     // Arcade Continue screen (0x16 next frame)
inline constexpr uint16_t kGameOver = 0x17;     // Game Over (0x18 next frame), then OPEN
}  // namespace battle_step

// A robot command word (Game::robot_commands; sub_822AABE0). Virtual-On
// is played with two levers: each has four directions and two buttons. Which
// bit is which direction or button isn't confirmed yet; the names say lever
// and position only. The action numbers are PlayerActions bits.
namespace command {
inline constexpr uint16_t kLeftDirection0 = 0x0001;   // action 3
inline constexpr uint16_t kLeftDirection1 = 0x0002;   // action 4
inline constexpr uint16_t kLeftDirection2 = 0x0004;   // action 5
inline constexpr uint16_t kLeftDirection3 = 0x0008;   // action 6
inline constexpr uint16_t kRightDirection0 = 0x0010;  // action 12
inline constexpr uint16_t kRightDirection1 = 0x0020;  // action 13
inline constexpr uint16_t kRightDirection2 = 0x0040;  // action 14
inline constexpr uint16_t kRightDirection3 = 0x0080;  // action 15
inline constexpr uint16_t kLeftButton0 = 0x0100;      // action 8
inline constexpr uint16_t kLeftButton1 = 0x0200;      // action 9
inline constexpr uint16_t kRightButton0 = 0x0400;     // action 10
inline constexpr uint16_t kRightButton1 = 0x0800;     // action 11
inline constexpr uint16_t kStart = 0x1000;            // action 2; never stored in the robot
                                                      // commands (sub_822A71B0 clears it)
inline constexpr uint16_t kExtra0 = 0x2000;           // action 7
inline constexpr uint16_t kExtra1 = 0x4000;           // only from OnlineInput
inline constexpr uint16_t kExtra2 = 0x8000;           // only from OnlineInput
}  // namespace command

// Robot numbers confirmed so far (Robot::robot_id, kRobotFileNames index, roster
// bytes). 0 is "dor" and 18 the last entry; the rest are not yet matched to names.
namespace robot_id {
// Bal-Bados and its two stage forms (sub_821C5060, src/decomp/robot_loader.cpp):
// file names "bbd", "bks" and "bbs". Earlier notes had the forms the other way
// round; the code gives Bal-Baros on the water stage and Bal-Keros on the final one.
inline constexpr uint8_t kBalBados = 1;
inline constexpr uint8_t kBalKeros = 5;   // Bal-Bados' form on the final stage (stage_id::kTangram)
inline constexpr uint8_t kAjim = 15;
inline constexpr uint8_t kBalBaros = 16;  // Bal-Bados' form on the water stage (stage_id::kWater)
inline constexpr uint8_t kBradtos = 17;
inline constexpr uint8_t kTangram = 18;   // "tgm"; empty motion file, not playable
}  // namespace robot_id

// Stage numbers confirmed so far (kNextStage, StageCard::stage).
namespace stage_id {
inline constexpr int32_t kRandom = -1;  // carousel entry 0
inline constexpr int32_t kWater = 5;    // "water"
inline constexpr int32_t kDistortedShrine = 10;
inline constexpr int32_t kSpaceCarrier = 16;  // no files
inline constexpr int32_t kTangram = 21;       // "final"; last entry of the stage tables
inline constexpr int32_t kTangramCore = 22;   // past the end of the stage tables
}  // namespace stage_id

// --- Structures --------------------------------------------------------------

struct BeVec3 {
  be<float> x, y, z;
};
static_assert(sizeof(BeVec3) == 12);

// An angle: 65536 per full turn in the low 16 bits (32-bit fields wrap).
using BeAngle32 = be<int32_t>;

// One entry of a robot's bone table.
struct Bone {
  BeAngle32 rotation[3];
  BeVec3 position;
};
static_assert(sizeof(Bone) == 24);

// One robot in a battle (Game::robots). 4496 bytes.
struct Robot {
  uint8_t robot_id;  // see robot_id
  uint8_t unk_0001[0x6D - 0x01];
  // 1 = the AI drives this robot and ignores its command words
  // (local versus clears it).
  uint8_t cpu_controlled;
  uint8_t unk_006E[0x74 - 0x6E];
  be<uint16_t> facing;  // the upper half of the word at +0x74; 65536 per turn
  uint8_t unk_0076[0x84 - 0x76];
  BeVec3 position;
  uint8_t unk_0090[0xB4 - 0x90];
  // Guest heap pointers (targets not yet known). Their values depend on where
  // the allocator placed things, which can differ between sessions with the
  // same gameplay (seen: all three 0xC1A0 higher with the native renderer
  // off), so replays leave them out of the state fingerprint.
  be<uint32_t> heap_pointer_0B4;
  uint8_t unk_00B8[0xC8 - 0xB8];
  be<uint32_t> heap_pointer_0C8;
  uint8_t unk_00CC[0x2D8 - 0xCC];
  be<uint32_t> heap_pointer_2D8;  // heap_pointer_0C8 + 0x10 when seen
  uint8_t unk_02DC[0x4B8 - 0x2DC];
  // Bone table: Bone entries from here; the count per robot isn't known yet.
  uint8_t bone_table[0x1190 - 0x4B8];

  Bone* bones() { return reinterpret_cast<Bone*>(bone_table); }
  const Bone* bones() const { return reinterpret_cast<const Bone*>(bone_table); }
};
static_assert(sizeof(Robot) == 4496);
static_assert(offsetof(Robot, cpu_controlled) == 0x6D);
static_assert(offsetof(Robot, facing) == 0x74);
static_assert(offsetof(Robot, position) == 0x84);
static_assert(offsetof(Robot, heap_pointer_0B4) == 0xB4);
static_assert(offsetof(Robot, heap_pointer_0C8) == 0xC8);
static_assert(offsetof(Robot, heap_pointer_2D8) == 0x2D8);
static_assert(offsetof(Robot, bone_table) == 0x4B8);

// A robot's input as its movement code sees it (RobotControl::input, robot
// +0x10C; sub_821FB390 and the functions it calls). The packed words hold the
// two levers' channel bits as LeverInput has them: the first lever (left
// channel) in the low byte, the second (right channel) in the high byte, so
// 0x0001/0x0002 are the first lever's buttons and 0x0100/0x0200 the second's.
struct RobotInput {
  uint8_t same_direction_ticks;  // +0x00: ticks `direction` stayed the same (up to 255)
  uint8_t no_direction_ticks;    // +0x01: ticks with no direction (0xFFFF), up to 255
  uint8_t direction_class;   // +0x02: `direction` sorted into 0-7 (sub_821F9BC8), 255 = none
  uint8_t direction_class2;  // +0x03: set together with it
  uint8_t latched_class;     // +0x04: direction_class copied when a button press meets the state rules
  uint8_t unk_05;
  uint8_t both_held_timer;   // +0x06: 255, then counts down, while 0x0002 and 0x0200 are both held
  // +0x07/+0x08: set to 255 once hold_0100/hold_0001 has counted down to 253
  // (two ticks into a press), then count down; cleared on release.
  uint8_t fired_0100;
  uint8_t fired_0001;
  uint8_t fired_both;        // +0x09: 255 when hold_0001 and hold_0100 are both above 251
  uint8_t lockout;           // +0x0A: while set, the charge meters only drain (sub_821F9DE0)
  // +0x0B: 255, then counts down, while the direction code's two lever signs
  // are (first < 0, second <= 0) or (0, second < 0), or for a few fixed codes.
  uint8_t lever_timer;
  uint8_t unk_0C;            // +0x0C: sub_821F9DE0
  uint8_t unk_0D;            // +0x0D: minus unk_0C
  uint8_t unk_0E;            // +0x0E: sub_821FA290 sets 255, or 1 / 0 with unk_0F
  int8_t unk_0F;             // +0x0F: counts up or down for some direction codes (third rate set)
  uint8_t hold_0001;  // +0x10: 255 on press, counts down while held
  uint8_t hold_0100;  // +0x11
  uint8_t hold_0002;  // +0x12
  uint8_t hold_0200;  // +0x13
  uint8_t hold_extra; // +0x14: the extra channel's 0x20 (command::kExtra0)
  uint8_t direction_changed;  // +0x15: 0 same as last tick, else 1 or 2
  // +0x16-0x1B: how much sum_y, sum_x and their length changed this tick
  // (scaled to an int; jumps to a bigger change, else eases to 0 by 256).
  be<uint16_t> sum_y_change;
  be<uint16_t> sum_x_change;
  be<uint16_t> length_change;
  be<uint16_t> previous_direction;  // +0x1C
  // +0x1E: direction code: first lever's direction in the high byte, the
  // second's in the low byte (DirectionLookup); 0xFFFF = none.
  be<uint16_t> direction;
  uint8_t unk_20[4];
  // +0x24: charge meters, per lever and direction: build while held, drain
  // otherwise, capped at 1536 (sub_821F9DE0).
  be<uint16_t> charge[2][8];
  be<uint32_t> held;     // +0x44
  be<uint32_t> pressed;  // +0x48: held now and not last tick
  // +0x4C-0x57 (sub_821FA290): each lever's charge meters make an (x, y)
  // vector; these are the two levers' y sum (negated), y difference
  // (negated) and x sum, scaled, and scaled again by a robot value in some
  // ranges. What each one moves isn't confirmed yet.
  be<float> sum_y;
  be<float> diff_y;
  be<float> sum_x;
  // +0x58: an angle from sum_y and sum_x (sub_822A73D8), or a quadrant
  // (0, 16384, 32768, 49152) when `spread` is large; -1 = none.
  be<int32_t> angle;
  be<int32_t> held_angle;  // +0x5C: `angle` while the direction is held under 11 ticks, else -1
  be<int32_t> spread;      // +0x60: the levers' x difference, squared, times a robot parameter
  uint8_t unk_64[4];
  be<int32_t> spread_sign;  // +0x68: 1 / -1 when the levers point opposite ways sideways, else 0
  be<int32_t> y_spread;     // +0x6C: like `spread` for the y difference (third rate set, both buttons held)
};
static_assert(sizeof(RobotInput) == 0x70);
static_assert(offsetof(RobotInput, sum_y_change) == 0x16);
static_assert(offsetof(RobotInput, direction) == 0x1E);
static_assert(offsetof(RobotInput, charge) == 0x24);
static_assert(offsetof(RobotInput, held) == 0x44);
static_assert(offsetof(RobotInput, sum_y) == 0x4C);
static_assert(offsetof(RobotInput, angle) == 0x58);
static_assert(offsetof(RobotInput, y_spread) == 0x6C);

// A robot's control block, at Robot +0x6C (Game +25020 for side 1, +29516
// for side 2); sub_821FB390 runs on it once per tick.
struct RobotControl {
  uint8_t unk_000;
  // +0x01 (= Robot::cpu_controlled): where the input comes from: 0 the
  // player's levers, 1 or 4 the CPU (AiControl), 5 PlaybackInput, 8 none.
  uint8_t mode;
  uint8_t action;  // +0x02: compared with 0, 4, 11, 12 when choosing charge rates
  uint8_t unk_003[0x06 - 0x03];
  // +0x06 (Robot +0x72) and +0x08 (Robot +0x74, Robot::facing): subtracted
  // from the u16s at kControlRef1 / kControlRef2 (sub_821FA290).
  be<uint16_t> unk_006;
  be<uint16_t> facing;
  uint8_t unk_00A[0x4C - 0x0A];
  be<uint32_t> params;  // +0x4C: guest address of the robot's parameters (ChargeParams inside)
  uint8_t unk_050[0x90 - 0x50];
  uint8_t unk_090;  // +0x90: picks among the spread parameters
  uint8_t unk_091;
  uint8_t unk_092;  // +0x92: picks among the y_spread parameters (third rate set)
  uint8_t unk_093;  // +0x93: like unk_090, third rate set
  uint8_t unk_094[0xA0 - 0x94];
  RobotInput input;  // +0xA0
  be<uint16_t> movement_state;  // +0x110: decides when a button latches the direction
  be<uint16_t> movement_substate;  // +0x112
  uint8_t unk_114[0x190 - 0x114];
  be<uint32_t> movement_flags;  // +0x190: low 6 bits set select the second ChargeParams set
  uint8_t unk_194[0x1BE - 0x194];
  be<uint16_t> unk_1BE;  // +0x1BE: nonzero selects kChargeFixedA
  uint8_t unk_1C0[2];
  be<uint16_t> unk_1C2;  // +0x1C2: signed; picks the quarter or fixed rates
};
static_assert(offsetof(RobotControl, facing) == 0x08);
static_assert(offsetof(RobotControl, unk_093) == 0x93);
static_assert(offsetof(RobotControl, movement_substate) == 0x112);
static_assert(offsetof(RobotInput, unk_0F) == 0x0F);
// Past the control block (RobotControl offsets): a float the movement code
// scales by (x0.7 below 70, rising to x1.0 at 100), and two u16s it measures
// against unk_006 / facing (meaning not confirmed).
inline constexpr uint32_t kControlScale = 4464;
inline constexpr uint32_t kControlRef1 = 4488;
inline constexpr uint32_t kControlRef2 = 4490;
// Movement fields (RobotControl offsets, sub_821F6A10). Velocity along a
// 16-bit angle a is (-A(a), B(a)) times the speed, A = sub_822A7558 and
// B = sub_822A75A0; in the default state RobotInput::sum_x pushes along the
// facing and sum_y across it.
inline constexpr uint32_t kControlMoveAngle = 16;   // u16, added to the facing in state 2
inline constexpr uint32_t kControlPosition = 24;    // BeVec3 (= Robot::position)
inline constexpr uint32_t kControlFreeze = 156;     // u16: while set, no velocity
inline constexpr uint32_t kControlRunUp = 338;      // u16: counts some states up; speed 0 after 38
inline constexpr uint32_t kControlSpeed = 372;      // float: this tick's speed
inline constexpr uint32_t kControlVelocity = 380;   // float x2 at +380 and +388
inline constexpr uint32_t kControlWarpFrom = 3328;  // BeVec3: where a warp ends if there's no ground
inline constexpr uint32_t kControlWarpTo = 3340;    // BeVec3: where a warp eases toward
inline constexpr uint32_t kControlWarp = 3354;      // u16 countdown from 60
static_assert(0x6C + kControlPosition == offsetof(Robot, position));  // control block at Robot +0x6C
// be<uint32_t> in Game: the guest address of the robot being updated's data
// (speed multipliers at +1448/+1452/+1460, run-up limits at +1476-1484,
// fixed pushes at +3880/+3884). sub_821F8168 sets it each tick.
inline constexpr uint32_t kGameRobotData = 38508;
static_assert(offsetof(RobotControl, mode) == 0x01);
static_assert(offsetof(RobotControl, params) == 0x4C);
static_assert(offsetof(RobotControl, input) == 0xA0);
static_assert(offsetof(RobotControl, movement_state) == 0x110);
static_assert(offsetof(RobotControl, movement_flags) == 0x190);
static_assert(offsetof(RobotControl, unk_1BE) == 0x1BE);
static_assert(offsetof(RobotControl, unk_1C2) == 0x1C2);
inline constexpr uint32_t kRobotControlOffset = 0x6C;

// One set of charge rates in a robot's parameters (at params +120, +220 and
// +320: normal, movement_flags set, and while Game +0x6164 is set).
struct ChargeParams {
  be<int32_t> build[8];  // added to the held direction's meter
  be<int32_t> decay[8];  // taken from the others
  be<int32_t> drop[8];   // taken from meter (d - 4) & 7 while direction d is held
  be<int32_t> threshold; // after a build, meters below it drop to 0
};
static_assert(sizeof(ChargeParams) == 100);

// kChargeTable: what sub_821F9DE0 reads this tick (guest addresses of
// be<int32_t>[8] tables).
struct ChargeTable {
  be<int32_t> threshold;
  be<uint32_t> drop;
  be<uint32_t> decay;
  be<uint32_t> build;
};
static_assert(sizeof(ChargeTable) == 16);

// kDirectionLookupFirst/Second: a lever's 4 direction bits (taken at `shift`
// from a packed word) to its byte of the direction code.
struct DirectionLookup {
  be<uint16_t> code[16];
  be<uint32_t> shift;
};
static_assert(offsetof(DirectionLookup, shift) == 32);

// The CPU player's state for one robot (kAiControl1/2, sub_82212A98). Its
// steps (eight functions, still original code) fill `held` like a player's
// levers; the rest is their bookkeeping, meaning mostly not known.
struct AiControl {
  be<uint32_t> held;    // +0x00: packed like RobotInput::held
  uint8_t hold_extra;   // +0x04
  uint8_t unk_05[0x14 - 0x05];
  be<int32_t> unk_14;    // +0x14
  be<int32_t> unk_18;    // +0x18
  be<int32_t> unk_1C;    // +0x1C
  uint8_t unk_20[0x30 - 0x20];
  be<int32_t> plan;      // +0x30: compared before and after a step to count repeats
  be<int32_t> unk_34;    // +0x34
  be<int32_t> timer;     // +0x38: below 0 = idle, else counts up to `duration`
  be<int32_t> duration;  // +0x3C
  uint8_t unk_40[0x44 - 0x40];
  be<int32_t> unk_44;    // +0x44
  uint8_t unk_48[0x6C - 0x48];
  be<int32_t> repeats;   // +0x6C: ticks `plan` stayed the same
};
static_assert(offsetof(AiControl, timer) == 0x38);
static_assert(offsetof(AiControl, repeats) == 0x6C);

// One of kPlaybackInput's two records (144 bytes apart).
struct PlaybackInput {
  uint8_t unk_00[8];
  be<uint32_t> held;      // +0x08
  be<uint32_t> previous;  // +0x0C
};
inline constexpr uint32_t kPlaybackInputStride = 144;

// One input channel of a side (LeverInput): now, at the previous tick, and
// pressed this tick (now and not previous).
struct InputChannel {
  be<uint16_t> now;
  be<uint16_t> previous;
  be<uint16_t> pressed;
  uint8_t unk_6[2];
};
static_assert(sizeof(InputChannel) == 8);

// One side's command word split into channels (Game::levers; sub_822A7070).
// left: the left lever's directions (bits 0x10-0x80, from command 0x1-0x8)
// and buttons (0x2, 0x1 from command 0x100, 0x200); right: the same for the
// right lever (from 0x10-0x80 and 0x400, 0x800); extra: command 0x1000,
// 0x2000, 0x4000, 0x8000 as 0x10, 0x20, 0x08, 0x04.
struct LeverInput {
  InputChannel left;
  InputChannel right;
  InputChannel extra;
  uint8_t unk_18[8];
};
static_assert(sizeof(LeverInput) == 32);

// The battle state, at *kGamePointer. Only the start is laid out; the full
// size isn't known (debug snapshots save 0x40000 bytes of it).
// A per-side record in Game (side_records). 300 bytes; only the first field is known.
struct GameSideRecord {
  be<int32_t> stage;  // a stage number: sub_821C5060 checks it for the water / final stage
  uint8_t unk_004[300 - 4];
};
static_assert(sizeof(GameSideRecord) == 300);

struct Game {
  be<float> camera[16];  // 4x4 matrix the frame is drawn with
  uint8_t unk_0040[0x240 - 0x40];
  // The running module's scene (scene_id) and the step within it
  // (battle_step for scene_id::kBattle). Set by the module switcher and by
  // each module's scene code.
  be<int16_t> scene;        // +0x240
  be<uint16_t> scene_step;  // +0x242
  uint8_t unk_0244[0x264 - 0x244];
  LeverInput levers[2];  // +0x264, per side (sub_822A7070)
  uint8_t unk_02A4[0x6148 - 0x2A4];
  // This tick's command word per side (sub_822A71B0), without command::kStart.
  be<uint16_t> robot_commands[2];  // +0x6148
  // Not yet typed: game+0x614C (24908) is 1 in Arcade and Score Attack
  // battles (0 in Training); game+0x614E (24910) is 1 from a run's first
  // fight and drops at Game Over (battle_step::kGameOver + 1); game+0x614F
  // counts the stages started in the run (a continue counts again).
  uint8_t unk_614C;
  int8_t record_side;  // +0x614D: which side_records entry sub_821C5060 reads
  uint8_t unk_614E[0x6150 - 0x614E];
  Robot robots[2];  // side 1 (DNA), side 2 (RNA)
  uint8_t unk_8470[0x96E4 - 0x8470];
  GameSideRecord side_records[2];  // +0x96E4
  uint8_t unk_993C[0xCEA0 - 0x993C];
  be<uint32_t> input_sources[4];  // +0xCEA0: kInputSource, stored each tick by sub_822A71B0
  // The running module's file name ("GAME.BIN"), copied by the switcher. The
  // buffer's real size isn't known; this is the gap to the next known field.
  char module_file[0xCEF7 - 0xCEB0];
  uint8_t module_switch_cleared;  // +0xCEF7: the switcher sets it to 0; meaning unknown
  uint8_t unk_CEF8;
  uint8_t module_switch_set;      // +0xCEF9: the switcher sets it to 1; meaning unknown
  uint8_t unk_CEFA[0xCF85 - 0xCEFA];
  CameraView camera_view;  // which view Draw renders (split screen sets it per view)
};
static_assert(offsetof(Game, scene) == 0x240);
static_assert(offsetof(Game, scene_step) == 0x242);
static_assert(offsetof(Game, module_file) == 0xCEB0);
static_assert(offsetof(Game, levers) == 612);
static_assert(offsetof(Game, robot_commands) == 24904);
static_assert(offsetof(Game, input_sources) == 52896);
static_assert(offsetof(Game, module_switch_cleared) == 52983);
static_assert(offsetof(Game, module_switch_set) == 52985);
static_assert(offsetof(Game, record_side) == 24909);
static_assert(offsetof(Game, side_records) == 38628);
static_assert(offsetof(Game, robots) == 0x6150);
static_assert(offsetof(Game, robots) + offsetof(Robot, cpu_controlled) == 25021);
static_assert(offsetof(Game, robots) + sizeof(Robot) + offsetof(Robot, cpu_controlled) == 29517);
static_assert(offsetof(Game, camera_view) == 53125);

// One side's input record (kSideInputs). 316 bytes.
struct SideInput {
  uint8_t unk_000[300];
  // Controller this side reads, or -1 for none. Choosing a side in the main
  // menu (sub_8213CCD0) gives it the active controller and sets the other to -1.
  be<int32_t> controller;
  uint8_t unk_130[316 - 304];
};
static_assert(sizeof(SideInput) == 316);
static_assert(offsetof(SideInput, controller) == 300);

// Per-controller bit masks at kControllerMasks (bit n = controller n).
// sub_82114A00(pad, on) sets or clears pad's bit in `enabled` and always clears
// it in the other two, whose meaning isn't known yet.
struct ControllerMasks {
  be<uint32_t> enabled;
  be<uint32_t> unk_mask_4;
  be<uint32_t> unk_mask_8;
};
static_assert(sizeof(ControllerMasks) == 12);

// One controller's state (kControllerStates), filled by sub_821135C0 every
// tick. It keeps its own history: `pressed` and `released` are computed
// against `previous` (the last read's `held`), and an unchanged XInput packet
// number skips the update.
struct ControllerState {
  uint8_t unk_00[4];
  be<uint32_t> held;      // +4: buttons held, in the game's own bit layout
  be<uint32_t> previous;  // +8: `held` at the previous read
  be<uint32_t> pressed;   // +12: held and not previous
  be<uint32_t> released;  // +16: previous and not held
  uint8_t unk_14[28 - 20];
  be<uint32_t> packet_number;  // +28: the last XInput packet number processed
  uint8_t unk_20[60 - 32];
};
static_assert(sizeof(ControllerState) == 60);
static_assert(offsetof(ControllerState, held) == 4);
static_assert(offsetof(ControllerState, pressed) == 12);
static_assert(offsetof(ControllerState, packet_number) == 28);

// One player's input actions (kPlayerActions). 2152 bytes; only the action
// bits are known: bit n of the big-endian words from +32 is action n
// (sub_82187FB0; it checks numbers from 99 up with sub_8213EA40 first).
struct PlayerActions {
  uint8_t unk_000[32];
  be<uint32_t> action_bits[4];
  uint8_t unk_030[2152 - 48];

  bool Has(uint32_t action) const { return (action_bits[action >> 5] >> (action & 31)) & 1; }
};
static_assert(sizeof(PlayerActions) == 2152);
static_assert(offsetof(PlayerActions, action_bits) == 32);

// The online command source (kOnlineInput); only what sub_8213D890/8E0 read.
struct OnlineInput {
  uint8_t unk_00[76];
  uint8_t active;  // +76
  uint8_t unk_4D[88 - 77];
  uint8_t ready;   // +88
  uint8_t unk_59[100 - 89];
  be<uint16_t> commands[2];  // +100: per side
  uint8_t unk_68[128 - 104];
  be<int32_t> state;  // +128: used only when not negative
};
static_assert(offsetof(OnlineInput, active) == 76);
static_assert(offsetof(OnlineInput, ready) == 88);
static_assert(offsetof(OnlineInput, commands) == 100);
static_assert(offsetof(OnlineInput, state) == 128);

// kCommandCapture.
struct CommandCapture {
  be<uint32_t> active;
  uint8_t unk_04[20 - 4];
  be<uint16_t> command;  // +20: side 0's command word while active
};
static_assert(offsetof(CommandCapture, command) == 20);

// One entry of the Training stage select carousel (kStageCarousel). 24 bytes.
struct StageCard {
  be<int32_t> stage;  // stage number; stage_id::kRandom for entry 0
  // Preview image position and sheet, name, panel image id; order not yet mapped.
  uint8_t unk_04[20];
};
static_assert(sizeof(StageCard) == 24);

// The Training character select's state (kTrainingCharSelect, also r3 of
// sub_8218E4C0). Only the cursors are known.
struct TrainingCharSelect {
  uint8_t unk_000[0x168];
  be<int32_t> cursor[2];  // cell index (into kTrainingRoster) per side
};
static_assert(offsetof(TrainingCharSelect, cursor) == 0x168);

// One entry of the draw list (kDrawListPointer). 0x15C bytes.
struct DrawRecord {
  be<float> modelview[16];
  uint8_t unk_040[0x4C - 0x40];
  be<uint32_t> part;  // guest address of the model part, pointing at the part's +0x2C
  uint8_t unk_050[0x60 - 0x50];
  be<float> world[16];
  be<float> view[16];
  uint8_t unk_0E0[0x15C - 0xE0];
};
static_assert(sizeof(DrawRecord) == 0x15C);
static_assert(offsetof(DrawRecord, part) == 0x4C);
static_assert(offsetof(DrawRecord, world) == 0x60);
static_assert(offsetof(DrawRecord, view) == 0xA0);

// --- Access ------------------------------------------------------------------

// Host pointer to guest address `address`.
template <typename T>
T* GuestPtr(uint8_t* base, uint32_t address) {
  return reinterpret_cast<T*>(base + address);
}
template <typename T>
const T* GuestPtr(const uint8_t* base, uint32_t address) {
  return reinterpret_cast<const T*>(base + address);
}

// The guest pointer (a guest address) stored at guest address `address`.
inline uint32_t LoadGuestAddress(const uint8_t* base, uint32_t address) {
  return *GuestPtr<be<uint32_t>>(base, address);
}

// The battle state, or nullptr outside battles.
inline Game* GetGame(uint8_t* base) {
  const uint32_t game = LoadGuestAddress(base, kGamePointer);
  return game ? GuestPtr<Game>(base, game) : nullptr;
}

inline Module CurrentModule(const uint8_t* base) { return Module(base[kModuleIndex]); }

// Battles run in GAME (Arcade, Score Attack) and NETVS (Training, online).
inline bool IsBattleModule(Module module) {
  return module == Module::kGame || module == Module::kNetVs;
}

// A stage is under way: from its load and intro through the fight to the
// results, but not Arcade's Continue and Game Over screens (which run in the
// battle module too), the Training select screens, or a quit to the menu.
inline bool IsFightInProgress(const Game& g, Module module) {
  return IsBattleModule(module) && g.scene == scene_id::kBattle &&
         g.scene_step < battle_step::kContinue;
}

inline SideInput* GetSideInputs(uint8_t* base) { return GuestPtr<SideInput>(base, kSideInputs); }

inline ControllerMasks* GetControllerMasks(uint8_t* base) {
  return GuestPtr<ControllerMasks>(base, kControllerMasks);
}

}  // namespace oratan::game
