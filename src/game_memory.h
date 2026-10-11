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
// u8: the running module (Module).
inline constexpr uint32_t kModuleIndex = 0x839C21BC;
// SideInput[2]: each side's input record.
inline constexpr uint32_t kSideInputs = 0x829BEFA8;
// ControllerState[4]: controllers as read by sub_821135C0.
inline constexpr uint32_t kControllerStates = 0x829BF2A8;
// be<int32_t>: stage number of the next fight (the Training stage select stores it here).
inline constexpr uint32_t kNextStage = 0x83876DF4;
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

// The byte at kModuleIndex (switcher: sub_822A6A30).
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
  uint8_t unk_0090[0x4B8 - 0x90];
  // Bone table: Bone entries from here; the count per robot isn't known yet.
  uint8_t bone_table[0x1190 - 0x4B8];

  Bone* bones() { return reinterpret_cast<Bone*>(bone_table); }
  const Bone* bones() const { return reinterpret_cast<const Bone*>(bone_table); }
};
static_assert(sizeof(Robot) == 4496);
static_assert(offsetof(Robot, cpu_controlled) == 0x6D);
static_assert(offsetof(Robot, facing) == 0x74);
static_assert(offsetof(Robot, position) == 0x84);
static_assert(offsetof(Robot, bone_table) == 0x4B8);

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
  // Two values whose meaning isn't known yet. sub_821C5060 keeps plain Bal-Bados
  // when they are (3, 21), or (0, 31) in the Ending module, and reads the
  // side record's stage only when scene_mode is 0.
  be<uint16_t> scene_mode;   // +0x240
  be<uint16_t> scene_param;  // +0x242
  // Not yet typed: game+0x614C (24908) stays 1 through Arcade's Continue
  // screen; game+0x614E (24910) only drops at Game Over.
  uint8_t unk_0244[0x614D - 0x244];
  int8_t record_side;  // +0x614D: which side_records entry sub_821C5060 reads
  uint8_t unk_614E[0x6150 - 0x614E];
  Robot robots[2];  // side 1 (DNA), side 2 (RNA)
  uint8_t unk_8470[0x96E4 - 0x8470];
  GameSideRecord side_records[2];  // +0x96E4
  uint8_t unk_993C[0xCF85 - 0x993C];
  CameraView camera_view;  // which view Draw renders (split screen sets it per view)
};
static_assert(offsetof(Game, scene_mode) == 0x240);
static_assert(offsetof(Game, scene_param) == 0x242);
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

// One controller's state (kControllerStates). 60 bytes, not yet mapped.
struct ControllerState {
  uint8_t unk_00[60];
};
static_assert(sizeof(ControllerState) == 60);

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

inline SideInput* GetSideInputs(uint8_t* base) { return GuestPtr<SideInput>(base, kSideInputs); }

}  // namespace oratan::game
