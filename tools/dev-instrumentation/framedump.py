"""Analyse a gpu_frame_dump_*.txt (SDK gpu_frame_stats + gpu_frame_dump_every).

Lists render passes (runs of draws to the same targets), resolves, and for each
texture sampled whether it is the output of a resolve in the same frame."""
import re, sys, collections

def parse(path):
    ev = []
    for line in open(path):
        t = line.split()
        if not t: continue
        if t[0] == 'D':
            d = dict(kind='D', pitch=int(t[t.index('pitch')+1]), msaa=int(t[t.index('msaa')+1]),
                     wo=t[t.index('wo')+1], n=int(t[t.index('n')+1]))
            d['c'] = re.findall(r' c(\d) (\d+)/(\d+)', line)
            z = re.search(r' z (\d+)/(\d+)', line); d['z'] = z.groups() if z else None
            d['t'] = [(int(a), int(b, 16), c, int(f)) for a, b, c, f in re.findall(r' t(\d+) ([0-9A-F]{8}) (\d+x\d+) f(\d+)', line)]
            d['vs'] = t[t.index('vs')+1]; d['ps'] = t[t.index('ps')+1]
            ev.append(d)
        elif t[0] == 'R':
            m = re.search(r'src (\d) (\d+)/(z?\d+) pitch (\d+) msaa (\d+) wo (\S+) sample (\d+) cmd (\d+) clr c(\d) z(\d) -> base ([0-9A-F]+) (\d+)x(\d+) fmt (\d+) bias (-?\d+) written ([0-9A-F]+)\+(\d+)', line)
            g = m.groups()
            ev.append(dict(kind='R', src=g[0], edram=int(g[1]), fmt=g[2], pitch=int(g[3]), msaa=int(g[4]), wo=g[5],
                           cmd=int(g[7]), clr=(g[8], g[9]), base=int(g[10], 16), w=int(g[11]), h=int(g[12]),
                           dfmt=int(g[13]), wa=int(g[15], 16), wl=int(g[16])))
        elif t[0] == 'SWAP':
            ev.append(dict(kind='S', fb=int(t[2], 16)))
    return ev

def report(ev):
    written = []                      # (start, end, resolve index)
    passes = []
    ri = 0
    for e in ev:
        if e['kind'] == 'D':
            key = (e['pitch'], e['msaa'], tuple(e['c']), e['z'], e['wo'])
            if passes and passes[-1][0] == key:
                passes[-1][1] += 1; passes[-1][2].update(x[1] for x in e['t'])
            else:
                passes.append([key, 1, set(x[1] for x in e['t'])])
        elif e['kind'] == 'R':
            ri += 1
            def src(addr):
                return [i for s, en, i in written if s <= addr < en]
            print('pass(es) before resolve %d:' % ri)
            for key, n, tex in passes:
                frm = sorted({i for a in tex for i in src(a)})
                print('   %4d draws pitch %4d msaa %d wo %-8s colour %s depth %s  textures from resolves %s, other textures %d'
                      % (n, key[0], key[1], key[4], ' '.join('%s:%s/f%s' % c for c in key[2]) or '-', key[3], frm,
                         sum(1 for a in tex if not src(a))))
            passes = []
            print('  R%-3d edram %4d fmt %-3s pitch %4d msaa %d wo %-8s cmd %d clear c%s z%s -> %08X %4dx%-4d fmt %2d  (%08X+%d)'
                  % (ri, e['edram'], e['fmt'], e['pitch'], e['msaa'], e['wo'], e['cmd'], e['clr'][0], e['clr'][1],
                     e['base'], e['w'], e['h'], e['dfmt'], e['wa'], e['wl']))
            if e['wl']:
                written.append((e['wa'], e['wa'] + e['wl'], ri))
            else:  # native mode: nothing written to memory, use the destination texture
                bpp = {2: 1, 6: 4, 7: 4, 10: 2, 26: 8, 32: 8, 29: 8}.get(e['dfmt'], 4)
                written.append((e['base'], e['base'] + ((e['w'] + 31) // 32 * 32) * ((e['h'] + 31) // 32 * 32) * bpp, ri))
        elif e['kind'] == 'S':
            print('SWAP front buffer %08X  (resolve %s)' % (e['fb'], [i for s, en, i in written if s <= e['fb'] < en]))
    for key, n, tex in passes:
        print('   %4d draws after the last resolve, colour %s' % (n, key[2]))

if __name__ == '__main__':
    report(parse(sys.argv[1]))
