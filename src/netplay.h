// Netplay (experimental): lockstep online versus over UDP (see netplay.cpp).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace oratan {

// True while connected and running in lockstep with the other player. Any
// thread.
bool NetplayActive();

// The controller slot this machine's player drives in the shared input
// (0 = host, 1 = joiner), or -1 when netplay is off.
int32_t NetplayLocalSlot();

// What netplay is doing, for the Options window. Any thread.
struct NetplayStatus {
  enum class State {
    kOff,        // not set up this session
    kWaiting,    // hosting: waiting for player 2 (the game waits at boot)
    kJoining,    // joining: waiting for the host's answer
    kConnected,  // running in lockstep
    kEnded,      // stopped (see message); the game plays on offline
  };
  State state = State::kOff;
  bool host = false;
  std::string peer;           // ADDRESS:PORT of the other player (joining: the host asked for)
  uint32_t port = 0;          // hosting: the UDP port
  uint32_t player = 0;        // 1 or 2 once connected
  uint32_t delay = 0;         // input delay in ticks once connected
  uint32_t recent_stalls = 0;     // in the last 30 s report
  double recent_stall_ms = 0.0;
  bool desynced = false;
  uint32_t desync_tick = 0;
  std::string message;        // why it ended or failed
};
NetplayStatus GetNetplayStatus();

// While waiting for player 2 or for the host: stop and play offline.
void CancelNetplayWait();

// This PC's IPv4 addresses on its networks (not loopback), to tell player 2.
std::vector<std::string> LocalIpv4Addresses();

}  // namespace oratan
