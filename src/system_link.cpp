// System Link matchmaking (with the SDK's --system_link).
//
// The game's network session object runs a state machine (table at
// 0x82001198, five words a state: enter, update, ?, exit, 0); sub_8212AAE8
// (session, state) switches it. The states met so far:
//   0  idle: picks the next step from the request the menus made
//   2  creating the session (XSessionCreate), then 3 if hosting, 5 if joining
//   3  host: listens on UDP 1002 for probes and answers with the session
//   4  search (update sub_8212B250): broadcasts a probe (16 bytes,
//      0x01328E96) to UDP 1002 once a second and joins the first host that
//      answers (176 bytes); it never gives up on its own
//   5  connecting, 6  in the match, 7  ending
// The switch pushes the new state and runs its enter function; it doesn't run
// the old one's exit (the search's, sub_82129EC8, closes the LAN discovery
// socket that hosting needs to open again).
// The idle state goes to 4 when the request's "host" flag (+804, copied from
// +5264) is clear, to 2 when it's set. The System Link menu only ever asks to
// host, so two consoles both wait as hosts and never meet.
//
// So here every System Link "create" searches first (probing four times a
// second), and only if no host answers within a random 1.5-4.5 s does it
// create and host the session. Whoever comes second joins whoever waits; two
// players confirming at the same moment get different deadlines, and the
// faster probes find the first host before the other's deadline.
// --system_link_role=host or join keeps a copy to one role.

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>

REXCVAR_DEFINE_STRING(system_link_role, "auto", "Oratan/Netplay",
                      "System Link: auto (join a waiting player, otherwise wait for one), host "
                      "or join");

REX_EXTERN(__imp__sub_8212AAE8);
REX_EXTERN(__imp__sub_8212B250);
REX_EXTERN(__imp__sub_82129EC8);

namespace {

using Clock = std::chrono::steady_clock;

// Fields of the session object (r3 of the state functions).
constexpr uint32_t kHostFlag = 804;         // be<uint32_t>: 1 = create and host
constexpr uint32_t kRequestHost = 5264;     // be<uint32_t>: the menu's "host" request
constexpr uint32_t kSystemLinkFlag = 5284;  // be<uint32_t>: nonzero in System Link
// The LAN discovery object inside it (sub_82123130's r3).
constexpr uint32_t kDiscovery = 48;
constexpr uint32_t kLastProbeTime = kDiscovery + 8;  // be<uint32_t>: ms, sub_824AC570's clock

constexpr uint32_t kStateCreate = 2;
constexpr uint32_t kStateSearch = 4;

constexpr int kMinSearchMs = 1500, kMaxSearchMs = 4500;
constexpr int kProbeEveryMs = 250;  // the game's own pace is 1000

enum class Role { kAuto, kHost, kJoin };

Role CurrentRole() {
  const std::string role = REXCVAR_GET(system_link_role);
  if (role == "host") return Role::kHost;
  if (role == "join") return Role::kJoin;
  return Role::kAuto;
}

uint32_t g_state = 0;
// The current search: when it gives up and hosts (auto only), and when the
// last probe was hurried along.
bool g_search_deadline_set = false;
Clock::time_point g_search_deadline, g_last_hurry;

uint32_t Load32(const uint8_t* base, uint32_t address) {
  uint32_t v;
  std::memcpy(&v, base + address, 4);
  return __builtin_bswap32(v);
}

void Store32(uint8_t* base, uint32_t address, uint32_t value) {
  const uint32_t v = __builtin_bswap32(value);
  std::memcpy(base + address, &v, 4);
}

int RandomSearchMs() {
  static std::mt19937 random{std::random_device{}()};
  return std::uniform_int_distribution<int>(kMinSearchMs, kMaxSearchMs)(random);
}

void LogState(const uint8_t* base, uint32_t session, uint32_t state) {
  REXLOG_INFO("system link: session {:08X} -> state {} (host {}, request {})", session, state,
              Load32(base, session + kHostFlag), Load32(base, session + kRequestHost));
}

}  // namespace

// sub_8212AAE8(session, state): the session state machine's switch.
REX_HOOK_RAW(sub_8212AAE8) {
  const uint32_t session = ctx.r3.u32;
  uint32_t state = ctx.r4.u32;
  const bool system_link = Load32(base, session + kSystemLinkFlag) != 0;
  if (system_link && state == kStateCreate && Load32(base, session + kHostFlag) != 0 &&
      CurrentRole() != Role::kHost) {
    // Create: look for a waiting player first.
    Store32(base, session + kHostFlag, 0);
    Store32(base, session + kRequestHost, 0);
    state = kStateSearch;
    ctx.r4.u64 = state;
    g_search_deadline_set = CurrentRole() == Role::kAuto;
    const int search_ms = RandomSearchMs();
    g_search_deadline = Clock::now() + std::chrono::milliseconds(search_ms);
    g_last_hurry = Clock::now();
    if (g_search_deadline_set) {
      REXLOG_INFO("system link: looking for a waiting player for {} ms", search_ms);
    } else {
      REXLOG_INFO("system link: looking for a waiting player");
    }
  }
  if (system_link) LogState(base, session, state);
  g_state = state;
  __imp__sub_8212AAE8(ctx, base);
}

// sub_8212B250(session): the search state's update, once a frame.
REX_HOOK_RAW(sub_8212B250) {
  const uint32_t session = ctx.r3.u32;
  const bool system_link = Load32(base, session + kSystemLinkFlag) != 0;
  const auto now = Clock::now();
  if (system_link && now - g_last_hurry >= std::chrono::milliseconds(kProbeEveryMs)) {
    // Probe again on this update: the game probes once a second after the
    // last one.
    Store32(base, session + kLastProbeTime, Load32(base, session + kLastProbeTime) - 1000);
    g_last_hurry = now;
  }
  __imp__sub_8212B250(ctx, base);
  if (!system_link || g_state != kStateSearch || !g_search_deadline_set ||
      now < g_search_deadline) {
    return;
  }
  // Nobody is waiting: create the session and wait for a player ourselves.
  g_search_deadline_set = false;
  const uint64_t result = ctx.r3.u64;
  Store32(base, session + kHostFlag, 1);
  Store32(base, session + kRequestHost, 1);
  REXLOG_INFO("system link: no one waiting; creating a session");
  LogState(base, session, kStateCreate);
  g_state = kStateCreate;
  ctx.r3.u64 = session;
  __imp__sub_82129EC8(ctx, base);
  ctx.r3.u64 = session;
  ctx.r4.u64 = kStateCreate;
  __imp__sub_8212AAE8(ctx, base);
  ctx.r3.u64 = result;
}
