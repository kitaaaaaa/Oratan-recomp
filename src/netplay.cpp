// Netplay (experimental): lockstep online versus over UDP.
//
// The game is deterministic from boot (notes/DEV_DIARY.md, "Replays"): the same
// controller input on the same tick gives the same game. So netplay is a live
// replay shared by two machines:
//   - Each tick, each machine samples its own player's controller (local slot
//     netplay_pad) and schedules it netplay_delay ticks ahead, sending it to the
//     other machine (with the recent unacknowledged ones, as UDP can drop
//     packets).
//   - When the game reads its controllers for tick t, both machines write the
//     same buffer: the host's input for t in slot 0, the joiner's in slot 1,
//     slots 2 and 3 empty (zero). If the other machine's input for t hasn't
//     arrived yet, the game waits for it (lockstep; no prediction).
//   - Both machines exchange their per-tick state fingerprint (state_hash.h).
//     The first tick where they differ is logged with the regions that differ,
//     and the state at that tick is written out for diffing (desync).
//   - Local versus is forced on (player 2 drives the opponent), split screen is
//     off, and each machine draws its own robot's view (local_versus.cpp).
// Both machines start the game with the same game files, save data and
// gameplay settings; the handshake checks the game files. The game waits at
// its first tick until the connection is made, so both run from boot in step.
//
// Settings: --netplay_host=PORT (player 1) or --netplay_join=ADDRESS:PORT
// (player 2), --netplay_delay=TICKS (host's value is used), --netplay_pad=N.
// The host's UDP port must be reachable (port forwarding, or a VPN such as
// ZeroTier or Tailscale). IPv4 only for now.

#include "netplay.h"

#include <rex/cvar.h>
#include <rex/logging.h>

#include "game_memory.h"
#include "state_hash.h"
#include "tick_hooks.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <random>
#include <string>
#include <vector>

REXCVAR_DEFINE_INT32(netplay_host, 0, "Oratan/Netplay",
                     "Experimental: host a netplay session as player 1 on this UDP port")
    .range(0, 65535);
REXCVAR_DEFINE_STRING(netplay_join, "", "Oratan/Netplay",
                      "Experimental: join a netplay session as player 2 (ADDRESS:PORT)");
REXCVAR_DEFINE_INT32(netplay_delay, 3, "Oratan/Netplay",
                     "Netplay input delay in ticks (1/60 s); the host's value is used. Higher "
                     "hides more network latency")
    .range(1, 15);
REXCVAR_DEFINE_INT32(netplay_pad, 0, "Oratan/Netplay",
                     "Which local controller (0-3) this machine's player uses in netplay")
    .range(0, 3);
REXCVAR_DEFINE_INT32(netplay_timeout, 10000, "Oratan/Netplay",
                     "Netplay: give up after this many milliseconds without hearing from the "
                     "other player, and continue offline")
    .range(1000, 600000);
REXCVAR_DEFINE_INT32(netplay_sim_latency, 0, "Oratan/Netplay",
                     "Developer: delay every outgoing netplay packet by this many milliseconds")
    .range(0, 2000);
REXCVAR_DEFINE_INT32(netplay_sim_loss, 0, "Oratan/Netplay",
                     "Developer: drop this percentage of outgoing netplay packets")
    .range(0, 90);

namespace {

namespace game = oratan::game;
using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;

constexpr uint32_t kMagic = 0x504E544F;  // "OTNP"
constexpr uint16_t kProtocol = 1;
constexpr size_t kSlotBytes = sizeof(game::ControllerState);
constexpr uint32_t kSlots = 4;
constexpr uint32_t kRing = 512;           // ticks of inputs and fingerprints kept
// 20 inputs keep a packet near 1,300 bytes, under the usual 1,500-byte MTU.
constexpr uint32_t kMaxInputsPerPacket = 20;
constexpr uint32_t kDumpRing = 32;        // ticks of state copies kept for desync dumps
constexpr int32_t kHashBytes = 0xD000;    // as replays' default
// The game's code and read-only data: must match on both machines.
constexpr uint32_t kImageStart = 0x82000000, kImageEnd = 0x8288B3B4;

enum Message : uint8_t { kHello = 1, kWelcome = 2, kInput = 3, kBye = 4 };

using SlotBytes = std::array<uint8_t, kSlotBytes>;

uint64_t Fnv(const uint8_t* data, size_t size) {
  uint64_t h = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < size; ++i) h = (h ^ data[i]) * 0x100000001B3ull;
  return h;
}

// --- UDP -----------------------------------------------------------------------

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kNoSocket = INVALID_SOCKET;
void CloseSocket(SocketHandle s) { closesocket(s); }
#else
using SocketHandle = int;
constexpr SocketHandle kNoSocket = -1;
void CloseSocket(SocketHandle s) { close(s); }
#endif

struct Endpoint {
  sockaddr_in addr{};
  bool operator==(const Endpoint& o) const {
    return addr.sin_addr.s_addr == o.addr.sin_addr.s_addr && addr.sin_port == o.addr.sin_port;
  }
  std::string ToString() const {
    char text[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &addr.sin_addr, text, sizeof(text));
    return std::string(text) + ":" + std::to_string(ntohs(addr.sin_port));
  }
};

// A non-blocking UDP socket. Outgoing packets can be delayed and dropped on
// purpose (netplay_sim_latency / netplay_sim_loss) for testing on one machine.
class Udp {
 public:
  ~Udp() { Close(); }

  bool Open(uint16_t port) {
#ifdef _WIN32
    static bool wsa = [] {
      WSADATA data;
      return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    if (!wsa) return false;
#endif
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == kNoSocket) return false;
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);
    if (bind(socket_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
      Close();
      return false;
    }
#ifdef _WIN32
    u_long non_blocking = 1;
    ioctlsocket(socket_, FIONBIO, &non_blocking);
#else
    fcntl(socket_, F_SETFL, fcntl(socket_, F_GETFL, 0) | O_NONBLOCK);
#endif
    return true;
  }

  void Close() {
    if (socket_ != kNoSocket) CloseSocket(socket_);
    socket_ = kNoSocket;
  }

  static bool Resolve(const std::string& host_port, Endpoint* out) {
    const size_t colon = host_port.rfind(':');
    if (colon == std::string::npos) return false;
    const std::string host = host_port.substr(0, colon), port = host_port.substr(colon + 1);
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0 || !result) return false;
    std::memcpy(&out->addr, result->ai_addr, sizeof(out->addr));
    freeaddrinfo(result);
    return true;
  }

  void Send(const Endpoint& to, const std::vector<uint8_t>& packet) {
    if (REXCVAR_GET(netplay_sim_loss) > 0 &&
        int(random_() % 100) < REXCVAR_GET(netplay_sim_loss)) {
      return;
    }
    const int latency = REXCVAR_GET(netplay_sim_latency);
    if (latency > 0) {
      delayed_.push_back({Clock::now() + Ms(latency), to, packet});
      return;
    }
    SendNow(to, packet);
  }

  // Sends delayed packets that are due.
  void Flush() {
    const auto now = Clock::now();
    while (!delayed_.empty() && delayed_.front().due <= now) {
      SendNow(delayed_.front().to, delayed_.front().bytes);
      delayed_.pop_front();
    }
  }

  // One waiting packet, or false.
  bool Receive(std::vector<uint8_t>* packet, Endpoint* from) {
    packet->resize(2048);
    socklen_t len = sizeof(from->addr);
    const auto got = recvfrom(socket_, reinterpret_cast<char*>(packet->data()), int(packet->size()),
                              0, reinterpret_cast<sockaddr*>(&from->addr), &len);
    if (got <= 0) return false;
    packet->resize(size_t(got));
    return true;
  }

  // Waits up to `ms` for a packet to arrive (returns early when one does).
  void Wait(int ms) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(socket_, &read_set);
    timeval timeout{0, ms * 1000};
    select(int(socket_) + 1, &read_set, nullptr, nullptr, &timeout);
  }

 private:
  struct Delayed {
    Clock::time_point due;
    Endpoint to;
    std::vector<uint8_t> bytes;
  };

  void SendNow(const Endpoint& to, const std::vector<uint8_t>& packet) {
    sendto(socket_, reinterpret_cast<const char*>(packet.data()), int(packet.size()), 0,
           reinterpret_cast<const sockaddr*>(&to.addr), sizeof(to.addr));
  }

  SocketHandle socket_ = kNoSocket;
  std::deque<Delayed> delayed_;
  std::minstd_rand random_{12345};
};

// --- Packets -------------------------------------------------------------------
// Little-endian fields (both machines are x86-64 PCs):
//   all:      u32 magic, u16 protocol, u8 type, u8 0
//   Hello:    u64 game hash                              (joiner -> host, repeated)
//   Welcome:  u64 game hash, u8 accepted, u8 delay       (host -> joiner)
//   Input:    u32 ack, u32 first, u8 count, count x 60 bytes,
//             u32 hash tick (0 = none), u64 hash[kStateRegions]
//   Bye:      (nothing)

class Writer {
 public:
  explicit Writer(Message type) {
    U32(kMagic);
    U16(kProtocol);
    U8(type);
    U8(0);
  }
  void U8(uint8_t v) { bytes_.push_back(v); }
  void U16(uint16_t v) { Raw(&v, 2); }
  void U32(uint32_t v) { Raw(&v, 4); }
  void U64(uint64_t v) { Raw(&v, 8); }
  void Raw(const void* data, size_t size) {
    const auto* p = static_cast<const uint8_t*>(data);
    bytes_.insert(bytes_.end(), p, p + size);
  }
  const std::vector<uint8_t>& bytes() const { return bytes_; }

 private:
  std::vector<uint8_t> bytes_;
};

class Reader {
 public:
  explicit Reader(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}
  // Reads the common header; false if it isn't one of our packets.
  bool Header(Message* type) {
    uint32_t magic = 0;
    uint16_t protocol = 0;
    uint8_t t = 0, pad = 0;
    if (!U32(&magic) || !U16(&protocol) || !U8(&t) || !U8(&pad)) return false;
    *type = Message(t);
    return magic == kMagic && protocol == kProtocol;
  }
  bool U8(uint8_t* v) { return Raw(v, 1); }
  bool U16(uint16_t* v) { return Raw(v, 2); }
  bool U32(uint32_t* v) { return Raw(v, 4); }
  bool U64(uint64_t* v) { return Raw(v, 8); }
  bool Raw(void* out, size_t size) {
    if (at_ + size > bytes_.size()) return false;
    std::memcpy(out, bytes_.data() + at_, size);
    at_ += size;
    return true;
  }

 private:
  const std::vector<uint8_t>& bytes_;
  size_t at_ = 0;
};

// --- Session -------------------------------------------------------------------

enum class Mode { kOff, kRunning, kEnded };

std::atomic<bool> g_active{false};
std::atomic<int32_t> g_local_slot{-1};

struct Session {
  Mode mode = Mode::kOff;
  bool host = false;
  uint32_t delay = 3;
  uint32_t my_slot = 0;
  Udp udp;
  Endpoint peer;
  uint64_t game_hash = 0;

  struct InputEntry {
    uint32_t tick = UINT32_MAX;
    SlotBytes bytes{};
  };
  struct HashEntry {
    uint32_t tick = UINT32_MAX;
    oratan::StateHashes hashes{};
  };
  struct DumpEntry {
    uint32_t tick = UINT32_MAX;
    std::vector<uint8_t> regions;
  };
  std::vector<InputEntry> local = std::vector<InputEntry>(kRing);
  std::vector<InputEntry> remote = std::vector<InputEntry>(kRing);
  std::vector<HashEntry> own_hashes = std::vector<HashEntry>(kRing);
  std::vector<HashEntry> peer_hashes = std::vector<HashEntry>(kRing);
  std::vector<DumpEntry> dumps = std::vector<DumpEntry>(kDumpRing);

  // Ticks up to `delay` have no input from anyone (all zero on both machines).
  uint32_t local_produced = 0;     // last tick we have our own input for
  uint32_t remote_contiguous = 0;  // last tick we have every remote input up to
  uint32_t peer_ack = 0;           // last tick the peer has every input of ours up to
  uint32_t last_own_hash_tick = 0;

  Clock::time_point last_heard, last_sent;
  bool desynced = false;
  uint32_t desync_tick = 0;
  uint32_t stalls = 0;
  double stall_ms = 0;
  uint32_t compared = 0;
  // For the periodic report.
  Clock::time_point report_time;
  uint32_t report_tick = 0, report_stalls = 0;
  double report_stall_ms = 0;

  void Start(uint8_t* base, bool replay_playing) {
    const int32_t host_port = REXCVAR_GET(netplay_host);
    const std::string join = REXCVAR_GET(netplay_join);
    if (host_port == 0 && join.empty()) return;
    if (replay_playing) {
      REXLOG_WARN("netplay: not started while a replay is playing");
      return;
    }
    host = host_port != 0;
    if (host && !join.empty()) REXLOG_WARN("netplay: both netplay_host and netplay_join set; hosting");
    if (!udp.Open(host ? uint16_t(host_port) : 0)) {
      REXLOG_ERROR("netplay: cannot open UDP port {}", host ? host_port : 0);
      return;
    }
    game_hash = Fnv(base + kImageStart, kImageEnd - kImageStart);
    const bool connected = host ? WaitForJoiner() : JoinHost(join);
    if (!connected) {
      udp.Close();
      return;
    }
    my_slot = host ? 0 : 1;
    local_produced = remote_contiguous = peer_ack = delay;
    last_heard = last_sent = report_time = Clock::now();
    mode = Mode::kRunning;
    g_local_slot = int32_t(my_slot);
    g_active = true;
    REXLOG_INFO("netplay: connected to {} as player {}, input delay {} ticks", peer.ToString(),
                my_slot + 1, delay);
  }

  bool WaitForJoiner() {
    delay = uint32_t(REXCVAR_GET(netplay_delay));
    REXLOG_INFO("netplay: waiting for player 2 on UDP port {}", REXCVAR_GET(netplay_host));
    auto next_note = Clock::now() + std::chrono::seconds(10);
    std::vector<uint8_t> packet;
    Endpoint from;
    for (;;) {
      udp.Wait(100);
      while (udp.Receive(&packet, &from)) {
        Reader in(packet);
        Message type;
        uint64_t their_hash = 0;
        if (!in.Header(&type) || type != kHello || !in.U64(&their_hash)) continue;
        const bool same_game = their_hash == game_hash;
        Writer out(kWelcome);
        out.U64(game_hash);
        out.U8(same_game ? 1 : 0);
        out.U8(uint8_t(delay));
        udp.Send(from, out.bytes());
        udp.Flush();
        if (!same_game) {
          REXLOG_ERROR("netplay: {} has different game files; refused", from.ToString());
          continue;
        }
        peer = from;
        return true;
      }
      if (Clock::now() > next_note) {
        REXLOG_INFO("netplay: still waiting for player 2");
        next_note = Clock::now() + std::chrono::seconds(10);
      }
    }
  }

  bool JoinHost(const std::string& host_port) {
    if (!Udp::Resolve(host_port, &peer)) {
      REXLOG_ERROR("netplay: cannot resolve {} (expected ADDRESS:PORT)", host_port);
      return false;
    }
    REXLOG_INFO("netplay: joining {}", peer.ToString());
    const auto give_up = Clock::now() + std::chrono::seconds(30);
    std::vector<uint8_t> packet;
    Endpoint from;
    while (Clock::now() < give_up) {
      Writer hello(kHello);
      hello.U64(game_hash);
      udp.Send(peer, hello.bytes());
      udp.Flush();
      udp.Wait(250);
      while (udp.Receive(&packet, &from)) {
        if (!(from == peer)) continue;
        Reader in(packet);
        Message type;
        uint64_t their_hash = 0;
        uint8_t accepted = 0, host_delay = 0;
        if (!in.Header(&type) || type != kWelcome || !in.U64(&their_hash) || !in.U8(&accepted) ||
            !in.U8(&host_delay)) {
          continue;
        }
        if (!accepted) {
          REXLOG_ERROR("netplay: the host has different game files; not connected");
          return false;
        }
        delay = host_delay;
        return true;
      }
    }
    REXLOG_ERROR("netplay: no answer from {} after 30 s; playing offline", peer.ToString());
    return false;
  }

  void End(const char* why) {
    if (mode != Mode::kRunning) return;
    for (int i = 0; i < 3; ++i) udp.Send(peer, Writer(kBye).bytes());
    udp.Flush();
    mode = Mode::kEnded;
    g_active = false;
    g_local_slot = -1;
    REXLOG_INFO(
        "netplay: ended at tick {} ({}): {} fingerprints compared, {}, {} stalls ({:.0f} ms)",
        oratan::GameTick(), why, compared,
        desynced ? "desync from tick " + std::to_string(desync_tick) : std::string("in sync"),
        stalls, stall_ms);
  }

  // Every 30 s: how often the game had to wait for the other player. Frequent
  // stalls mean netplay_delay is lower than the connection needs.
  void Report() {
    const auto now = Clock::now();
    if (mode != Mode::kRunning || now - report_time < std::chrono::seconds(30)) return;
    const uint32_t tick = oratan::GameTick();
    REXLOG_INFO("netplay: ticks {}-{}: {} stalls ({:.0f} ms waiting), {} fingerprints compared, {}",
                report_tick, tick, stalls - report_stalls, stall_ms - report_stall_ms, compared,
                desynced ? "desynced" : "in sync");
    report_time = now;
    report_tick = tick;
    report_stalls = stalls;
    report_stall_ms = stall_ms;
  }

  const uint8_t* InputFor(const std::vector<InputEntry>& ring, uint32_t tick) const {
    static const SlotBytes kEmpty{};
    if (tick <= delay) return kEmpty.data();
    const InputEntry& e = ring[tick % kRing];
    return e.tick == tick ? e.bytes.data() : nullptr;
  }

  void SendInputs() {
    Writer out(kInput);
    out.U32(remote_contiguous);
    const uint32_t first = peer_ack + 1;
    uint32_t count = local_produced >= first ? local_produced - first + 1 : 0;
    if (count > kMaxInputsPerPacket) count = kMaxInputsPerPacket;
    out.U32(first);
    out.U8(uint8_t(count));
    for (uint32_t i = 0; i < count; ++i) out.Raw(local[(first + i) % kRing].bytes.data(), kSlotBytes);
    const HashEntry& h = own_hashes[last_own_hash_tick % kRing];
    const bool have_hash = last_own_hash_tick != 0 && h.tick == last_own_hash_tick;
    out.U32(have_hash ? last_own_hash_tick : 0);
    for (uint64_t v : h.hashes) out.U64(have_hash ? v : 0);
    udp.Send(peer, out.bytes());
    last_sent = Clock::now();
  }

  void Pump() {
    udp.Flush();
    std::vector<uint8_t> packet;
    Endpoint from;
    while (udp.Receive(&packet, &from)) {
      if (!(from == peer)) continue;
      Reader in(packet);
      Message type;
      if (!in.Header(&type)) continue;
      last_heard = Clock::now();
      if (type == kBye) {
        End("the other player left");
        return;
      }
      if (type == kHello && host) {
        // The joiner didn't get our Welcome; send it again.
        Writer out(kWelcome);
        out.U64(game_hash);
        out.U8(1);
        out.U8(uint8_t(delay));
        udp.Send(peer, out.bytes());
        continue;
      }
      if (type != kInput) continue;
      uint32_t ack = 0, first = 0, hash_tick = 0;
      uint8_t count = 0;
      if (!in.U32(&ack) || !in.U32(&first) || !in.U8(&count)) continue;
      if (ack > peer_ack) peer_ack = ack;
      for (uint32_t i = 0; i < count; ++i) {
        SlotBytes bytes;
        if (!in.Raw(bytes.data(), kSlotBytes)) break;
        const uint32_t tick = first + i;
        if (tick <= remote_contiguous || tick > remote_contiguous + kRing / 2) continue;
        remote[tick % kRing] = {tick, bytes};
      }
      while (remote[(remote_contiguous + 1) % kRing].tick == remote_contiguous + 1) {
        ++remote_contiguous;
      }
      oratan::StateHashes hashes{};
      if (!in.U32(&hash_tick)) continue;
      bool ok = true;
      for (uint64_t& v : hashes) ok = ok && in.U64(&v);
      if (ok && hash_tick != 0) {
        peer_hashes[hash_tick % kRing] = {hash_tick, hashes};
        Compare(hash_tick);
      }
    }
  }

  void Compare(uint32_t tick) {
    const HashEntry& own = own_hashes[tick % kRing];
    const HashEntry& theirs = peer_hashes[tick % kRing];
    if (own.tick != tick || theirs.tick != tick) return;
    ++compared;
    if (own.hashes == theirs.hashes || desynced) return;
    desynced = true;
    desync_tick = tick;
    REXLOG_ERROR("netplay: desync after tick {}: {} differ", tick,
                 oratan::DifferingRegions(own.hashes, theirs.hashes));
    const DumpEntry& dump = dumps[tick % kDumpRing];
    if (dump.tick == tick) {
      oratan::WriteStateRegions(dump.regions, "netplay_tick" + std::to_string(tick) + "_" +
                                                  (host ? "host" : "join") + ".bin");
    }
  }

  void OnControllerRead(uint8_t* base) {
    if (mode != Mode::kRunning) return;
    const uint32_t tick = oratan::GameTick();
    uint8_t* states = base + game::kControllerStates;

    // This player's input, scheduled `delay` ticks ahead (sampled once per tick).
    if (tick >= 1) {
      const uint32_t target = tick + delay;
      InputEntry& e = local[target % kRing];
      if (e.tick != target) {
        e.tick = target;
        std::memcpy(e.bytes.data(), states + kSlotBytes * uint32_t(REXCVAR_GET(netplay_pad)),
                    kSlotBytes);
        local_produced = target;
      }
    }
    Pump();
    SendInputs();

    // Wait for the other player's input for this tick.
    const uint8_t* theirs = InputFor(remote, tick);
    if (!theirs && mode == Mode::kRunning) {
      const auto start = Clock::now();
      const auto timeout = Ms(REXCVAR_GET(netplay_timeout));
      bool timed_out = false;
      while (!(theirs = InputFor(remote, tick))) {
        udp.Wait(1);
        Pump();
        if (mode != Mode::kRunning) break;
        if (Clock::now() - last_sent > Ms(16)) SendInputs();
        if (Clock::now() - last_heard > timeout) {
          timed_out = true;
          break;
        }
      }
      const double waited =
          std::chrono::duration<double, std::milli>(Clock::now() - start).count();
      if (waited > 2.0) {
        ++stalls;
        stall_ms += waited;
      }
      if (waited > 250.0) {
        REXLOG_WARN("netplay: waited {:.0f} ms for player {} at tick {}", waited,
                    2 - my_slot, tick);
      }
      if (timed_out) End("the other player stopped answering; continuing offline");
    }
    Report();
    if (mode != Mode::kRunning || !theirs) return;  // offline now: leave the real controllers

    std::memset(states, 0, kSlotBytes * kSlots);
    std::memcpy(states + kSlotBytes * my_slot, InputFor(local, tick), kSlotBytes);
    std::memcpy(states + kSlotBytes * (1 - my_slot), theirs, kSlotBytes);
  }

  void OnTickEnd(uint8_t* base) {
    if (mode != Mode::kRunning) return;
    const uint32_t tick = oratan::GameTick();
    own_hashes[tick % kRing] = {tick, oratan::HashState(base, kHashBytes)};
    last_own_hash_tick = tick;
    DumpEntry& dump = dumps[tick % kDumpRing];
    dump.tick = tick;
    oratan::CaptureStateRegions(base, kHashBytes, &dump.regions);
    Compare(tick);
  }
};

Session g_session;

}  // namespace

namespace oratan {

bool NetplayActive() { return g_active; }

int32_t NetplayLocalSlot() { return g_local_slot; }

namespace netplay {

void Start(uint8_t* base, bool replay_playing) { g_session.Start(base, replay_playing); }

void OnControllerRead(uint8_t* base) { g_session.OnControllerRead(base); }

void OnTickEnd(uint8_t* base) { g_session.OnTickEnd(base); }

}  // namespace netplay
}  // namespace oratan
