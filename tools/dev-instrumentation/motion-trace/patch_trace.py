"""Instrument a COPY of the project (never the working tree): load/store macros in the generated pch, tick hook, header,
CMake source list, and the codegen step stubbed out so the hand-instrumented pch is not regenerated.

    python patch_trace.py <copy of Oratan-recomp>      (then copy motion_trace.cpp to <copy>/src/ and build)
"""
import os, re, sys
root = sys.argv[1]
os.chdir(root)
p = open('generated/default/oratan_pch.h', encoding='utf-8').read()
assert 'oratan_trace_event' not in p, 'already patched'
a = p.index('#define REX_LOAD_U8(x)'); b = p.index('#define REX_LOAD_STRING')
loads = '''// ---- motion trace instrumentation (Oratan-trace): log loads from the motion file and stores to the robot record ----
extern volatile uint32_t g_trace_lo, g_trace_len, g_trace_slo, g_trace_slen;
void oratan_trace_event(uint8_t kind, uint32_t ea, uint8_t size, uint64_t value, const char* fn);
#define REX_LOAD_U8(x) ({ u32 _a = (u32)(x); u8 _v = *(volatile u8*)(base + _a + REX_PHYS_HOST_OFFSET(_a)); if ((u32)(_a - g_trace_lo) < g_trace_len) oratan_trace_event(0, _a, 1, _v, __func__); _v; })
#define REX_LOAD_U16(x) ({ u32 _a = (u32)(x); u16 _v = __builtin_bswap16(*(volatile u16*)(base + _a + REX_PHYS_HOST_OFFSET(_a))); if ((u32)(_a - g_trace_lo) < g_trace_len) oratan_trace_event(0, _a, 2, _v, __func__); _v; })
#define REX_LOAD_U32(x) ({ u32 _a = (u32)(x); u32 _v = __builtin_bswap32(*(volatile u32*)(base + _a + REX_PHYS_HOST_OFFSET(_a))); if ((u32)(_a - g_trace_lo) < g_trace_len) oratan_trace_event(0, _a, 4, _v, __func__); _v; })
#define REX_LOAD_U64(x) ({ u32 _a = (u32)(x); u64 _v = __builtin_bswap64(*(volatile u64*)(base + _a + REX_PHYS_HOST_OFFSET(_a))); if ((u32)(_a - g_trace_lo) < g_trace_len) oratan_trace_event(0, _a, 8, _v, __func__); _v; })
'''
p = p[:a] + loads + p[b:]
a = p.index('#define REX_STORE_U8(x, y)'); b = p.index('#define REX_MEMORY_SIZE')
stores = '''#define REX_STORE_U8(x, y)  ({ u32 _a = (u32)(x); u8 _y = (u8)(y); if ((u32)(_a - g_trace_slo) < g_trace_slen) oratan_trace_event(1, _a, 1, _y, __func__); *(volatile u8*)(base + _a + REX_PHYS_HOST_OFFSET(_a)) = _y; })
#define REX_STORE_U16(x, y) ({ u32 _a = (u32)(x); u16 _y = (u16)(y); if ((u32)(_a - g_trace_slo) < g_trace_slen) oratan_trace_event(1, _a, 2, _y, __func__); *(volatile u16*)(base + _a + REX_PHYS_HOST_OFFSET(_a)) = __builtin_bswap16(_y); })
#define REX_STORE_U32(x, y) ({ u32 _a = (u32)(x); u32 _y = (u32)(y); if ((u32)(_a - g_trace_slo) < g_trace_slen) oratan_trace_event(1, _a, 4, _y, __func__); *(volatile u32*)(base + _a + REX_PHYS_HOST_OFFSET(_a)) = __builtin_bswap32(_y); })
#define REX_STORE_U64(x, y) ({ u32 _a = (u32)(x); u64 _y = (u64)(y); if ((u32)(_a - g_trace_slo) < g_trace_slen) oratan_trace_event(1, _a, 8, _y, __func__); *(volatile u64*)(base + _a + REX_PHYS_HOST_OFFSET(_a)) = __builtin_bswap64(_y); })

'''
p = p[:a] + stores + p[b:]
open('generated/default/oratan_pch.h', 'w', encoding='utf-8').write(p)
print('pch patched')

t = open('src/tick_hooks.cpp', encoding='utf-8').read()
assert 'trace::OnTickEnd' not in t
t = t.replace('  oratan::replay::OnTickEnd(base);\n}', '  oratan::replay::OnTickEnd(base);\n  oratan::trace::OnTickEnd(base);\n}', 1)
open('src/tick_hooks.cpp', 'w', encoding='utf-8').write(t)
h = open('src/tick_hooks.h', encoding='utf-8').read()
if 'namespace trace' not in h:
    h = h.replace('}  // namespace oratan\n', 'namespace trace {\n// After each Update: finds Fei-Yen\'s motion file and records her robot record (Oratan-trace only).\nvoid OnTickEnd(uint8_t* base);\n}  // namespace trace\n\n}  // namespace oratan\n', 1)
open('src/tick_hooks.h', 'w', encoding='utf-8').write(h)
c = open('CMakeLists.txt', encoding='utf-8').read()
if 'motion_trace' not in c:
    c = c.replace('    src/tick_hooks.cpp\n', '    src/tick_hooks.cpp\n    src/motion_trace.cpp\n', 1)
open('CMakeLists.txt', 'w', encoding='utf-8').write(c)
g = open('generated/rexglue.cmake', encoding='utf-8', newline='').read()
if 'Oratan-trace:' not in g:
    pat = re.compile(r'( *)COMMAND \$<TARGET_FILE:rex::rexglue> codegen [^\r\n]*(\r?\n) *DEPFILE [^\r\n]*\r?\n')
    stub = lambda m: (m.group(1) + '# Oratan-trace: the generated code is hand-instrumented (oratan_pch.h), never regenerate it here.' + m.group(2) +
                      m.group(1) + 'COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.build.stamp"' + m.group(2))
    g, n = pat.subn(stub, g)
    assert n == 1, 'codegen command not found in generated/rexglue.cmake'
    open('generated/rexglue.cmake', 'w', encoding='utf-8', newline='').write(g)
print('hooks, header, cmake and codegen stub patched')
