// Oratan-trace: logs every read of Fei-Yen's loaded motion file (FEI.mt) and every write to her robot record, by function,
// plus a snapshot of both robot records after each tick. The reads/writes come from instrumented REX_LOAD/REX_STORE macros
// in generated/default/oratan_pch.h; this file owns the range globals, the log buffers and the per-tick snapshot.
//
// Output (folder "trace_out" in the working directory):
//   trace_events.bin  records of 20 bytes: u32 tick, u32 ea, u64 value, u16 fn, u8 kind (0 read, 1 write), u8 size
//   trace_names.txt   fn id -> guest function name
//   trace_bones.bin   per tick: u32 tick, then the two robot records (2 x 4496 bytes, big-endian as in guest memory)
//   trace_info.txt    motion file range, robot record address
#include "tick_hooks.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

volatile uint32_t g_trace_lo = 0, g_trace_len = 0, g_trace_slo = 0, g_trace_slen = 0;

namespace {

#pragma pack(push, 1)
struct Rec { uint32_t tick; uint32_t ea; uint64_t value; uint16_t fn; uint8_t kind; uint8_t size; };
#pragma pack(pop)
static_assert(sizeof(Rec) == 20);

std::atomic_flag g_lock = ATOMIC_FLAG_INIT;
std::vector<Rec> g_buf;
std::unordered_map<const char*, uint16_t> g_fn;
std::vector<std::string> g_names;
FILE* g_events = nullptr;
FILE* g_bones = nullptr;
FILE* g_parts = nullptr;      // per tick: draw-list world matrices of Fei-Yen's parts
FILE* g_objs = nullptr;       // once per object: header + first vertices (to identify the part)
std::unordered_map<uint32_t, bool> g_seen_obj;
bool g_started = false, g_found = false;
uint64_t g_total = 0, g_cap = 0;

inline void Lock() { while (g_lock.test_and_set(std::memory_order_acquire)) {} }
inline void Unlock() { g_lock.clear(std::memory_order_release); }

void FlushLocked() {
  if (g_events && !g_buf.empty()) {
    fwrite(g_buf.data(), sizeof(Rec), g_buf.size(), g_events);
    fflush(g_events);
    g_buf.clear();
  }
}

void WriteNames() {
  FILE* f = fopen("trace_out/trace_names.txt", "w");
  if (!f) return;
  for (size_t i = 0; i < g_names.size(); i++) fprintf(f, "%zu %s\n", i, g_names[i].c_str());
  fclose(f);
}

inline uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

}  // namespace

void oratan_trace_event(uint8_t kind, uint32_t ea, uint8_t size, uint64_t value, const char* fn) {
  Lock();
  if (g_total < g_cap) {
    auto it = g_fn.find(fn);
    uint16_t id;
    if (it == g_fn.end()) { id = (uint16_t)g_names.size(); g_fn[fn] = id; g_names.push_back(fn); }
    else id = it->second;
    g_buf.push_back(Rec{oratan::GameTick(), ea, value, id, kind, size});
    ++g_total;
    if (g_buf.size() >= (1u << 20)) FlushLocked();
  }
  Unlock();
}

namespace oratan::trace {

void OnTickEnd(uint8_t* base) {
  if (!g_started) {
    g_started = true;
    std::filesystem::create_directories("trace_out");
    g_events = fopen("trace_out/trace_events.bin", "wb");
    g_bones = fopen("trace_out/trace_bones.bin", "wb");
    g_parts = fopen("trace_out/trace_parts.bin", "wb");
    g_objs = fopen("trace_out/trace_objs.bin", "wb");
    const char* cap = getenv("ORATAN_TRACE_MAXMB");
    g_cap = (uint64_t)(cap ? atoi(cap) : 3000) * 1024 * 1024 / sizeof(Rec);
    g_buf.reserve(1u << 20);
  }
  // game struct -> robot record 0 (the player): +0x00 robot number (8 = Fei-Yen), +0xC8 pointer to the loaded .mt file
  uint32_t game = Be32(base + 0x839EEA98);
  if (!game) { g_trace_len = 0; g_trace_slen = 0; g_found = false; return; }
  uint32_t rec = game + 0x6150;
  uint8_t robot = base[rec];
  uint32_t mt = Be32(base + rec + 0xC8);
  if (robot == 8 && mt >= 0x40000000u) {
    if (!g_found || g_trace_lo != mt) {
      g_found = true; g_trace_lo = mt; g_trace_slo = rec;
      // default: log only the animation-table reads (which animation plays each tick); ORATAN_TRACE_FULL=1 logs every key read and record write
      if (getenv("ORATAN_TRACE_FULL")) { g_trace_len = 0xC4320; g_trace_slen = 4496; } else { g_trace_len = 0xF74; g_trace_slen = 0; }
      FILE* f = fopen("trace_out/trace_info.txt", "a");
      if (f) { fprintf(f, "tick %u: motion file at 0x%08X len 0x%X, robot record at 0x%08X\n", GameTick(), mt, 0xC4320u, rec); fclose(f); }
    }
  } else {
    g_trace_len = 0; g_trace_slen = 0; g_found = false;
  }

  // draw list: base pointer at 0x839EFB78, count at 0x839EFB7C, 0x15C-byte records (+0x4C = object + 0x2C, +0x60 = world matrix)
  if (g_found && g_parts) {
    uint32_t list = Be32(base + 0x839EFB78), cnt = Be32(base + 0x839EFB7C);
    if (list && cnt && cnt < 20000) {
      std::vector<uint8_t> out; uint32_t t = GameTick(); uint16_t kept = 0;
      out.resize(6);
      for (uint32_t i = 0; i < cnt; i++) {
        const uint8_t* r = base + list + i * 0x15C;
        uint32_t obj = Be32(r + 0x4C) - 0x2C;
        if (obj < 0x40000000u || obj > 0xB0000000u) continue;
        const uint8_t* o = base + obj;
        uint32_t nv = Be32(o + 0x34), mat = Be32(o + 0x28);
        if (nv == 0 || nv > 5000 || mat >= 47) continue;                 // Fei-Yen's part objects (material index < 47)
        if (!g_seen_obj.count(obj)) {
          g_seen_obj[obj] = true;
          uint32_t hdr[2] = {obj, 56 + 16 * nv}; fwrite(hdr, 4, 2, g_objs); fwrite(o, 1, hdr[1], g_objs);
        }
        size_t at = out.size(); out.resize(at + 4 + 64);
        memcpy(&out[at], &obj, 4); memcpy(&out[at + 4], r + 0x60, 64);
        kept++;
      }
      memcpy(&out[0], &t, 4); memcpy(&out[4], &kept, 2);
      fwrite(out.data(), 1, out.size(), g_parts);
    }
  }
  if (g_found && g_bones) {
    uint32_t t = GameTick();
    fwrite(&t, 4, 1, g_bones);
    fwrite(base + rec, 4496, 2, g_bones);
    if ((t % 120) == 0) { fflush(g_bones); if (g_parts) fflush(g_parts); if (g_objs) fflush(g_objs); Lock(); FlushLocked(); Unlock(); WriteNames(); }
  }
}

}  // namespace oratan::trace
