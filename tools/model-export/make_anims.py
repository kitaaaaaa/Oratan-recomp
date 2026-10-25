"""Turn run2_bones.npz (exact per-tick bone rotations from the game) into animation pose files, one per game animation.

    python make_anims.py <bones.npz> <segments.pkl> <outdir> [--tick-rate 100]

Output per animation: anim_<id>.txt in the poses.txt format with a META line, plus anims.json with timing/speed metadata.
Time: ticks run at `tick-rate` per animation-second (the trace was recorded in the game's slow-motion mode; the idle loop's
measured 160-tick period vs its stored 96-frame length at 60/s gives 100 ticks per second).
Heading: parts_extract removed the pelvis yaw of every tick. It is put back as the swing relative to a reference direction:
the HEAD's yaw (the robot holds an angled stance with the head on its target, so the pelvis is not the facing); loops also lose
their linear yaw drift (circle-strafing turns the whole body steadily). Jump/fall animations (51, 52, 53, 55) keep ROOTY at 0:
their height is the world's altitude, not part of the pose.
"""
import sys, os, json, pickle, math
import numpy as np
from scipy.spatial.transform import Rotation as Rot, Slerp

NPZ, SEGS, OUT = sys.argv[1:4]
RATE = float(sys.argv[sys.argv.index('--tick-rate') + 1]) if '--tick-rate' in sys.argv else 100.0
FPS = 30
os.makedirs(OUT, exist_ok=True)
Z = np.load(NPZ, allow_pickle=True)
ticks = Z['ticks']; rd = Z['root_disp']; yaw = Z['heading']
names = [str(n) for n in Z['names']]
Q = {n: np.array(Z['q_' + n]) for n in names}
seg = pickle.load(open(SEGS, 'rb'))
idx = {int(t): i for i, t in enumerate(ticks)}
UNIT = 1.5 / 21.117                                  # game units -> metres (the avatar is 1.5 m for 21.117 units)

def best_segment(a):
    best = None
    for s_a, t0, t1 in seg:
        if s_a != a: continue
        ii = [idx[t] for t in range(t0, t1 + 1) if t in idx]
        if best is None or len(ii) > len(best): best = ii
    return best

def signal(ii):
    cols = []
    for n in ('thigh_L', 'thigh_R', 'shin_L', 'shin_R', 'torso', 'upper_arm_L', 'upper_arm_R'):
        if n in Q: cols.append(np.nan_to_num(Q[n][ii][:, :3]))
    return np.hstack(cols) if cols else None

def find_period(sig, lo=10, hi=500):
    n = len(sig)
    if n < 2 * lo + 8: return None
    s = sig - sig.mean(0); best = (0, None)
    for k in range(lo, min(hi, n // 2 + 1)):
        a, b = s[:n - k], s[k:]
        c = np.sum(a * b) / (np.sqrt(np.sum(a * a) * np.sum(b * b)) + 1e-12)
        if c > best[0]: best = (c, k)
    return best[1], best[0]

def slerp_series(qs, ts, t_query):
    """qs: (N,4) xyzw at times ts; returns (M,4) at t_query (clamped)"""
    qs = np.array(qs); good = ~np.isnan(qs).any(1)
    if good.sum() < 2: return np.tile(qs[good][0] if good.any() else [0, 0, 0, 1], (len(t_query), 1))
    sl = Slerp(ts[good], Rot.from_quat(qs[good])); return sl(np.clip(t_query, ts[good][0], ts[good][-1])).as_quat()

OVERRIDE = {37: 55}
meta = {}
for a in sorted({s[0] for s in seg}, key=lambda x: (isinstance(x, str), x if not isinstance(x, str) else 0)):
    ii = best_segment(a)
    if ii is None or len(ii) < 3: continue
    ii = ii[min(6, len(ii) // 4):]                       # drop the blend-in from the previous pose
    if len(ii) < 3: continue
    sig = signal(ii); loop = False; per = None
    if sig is not None and len(ii) >= 40:
        k, c = find_period(sig, lo=20)
        vr = np.ptp(rd[ii][:, 1])                      # a jump or fall is not a loop
        if k and c > 0.6 and vr < 6: loop = True; per = k
    if a in OVERRIDE and len(ii) > OVERRIDE[a] * 2: loop = True; per = OVERRIDE[a]       # strafes: right is the mirror of left (period ~55)
    if loop:
        # one period, starting where the signal is closest to its value one period later (cleanest loop point)
        n = len(ii); best = (1e9, 0)
        for s0 in range(0, max(1, n - per - 1)):
            d = np.linalg.norm(sig[s0] - sig[s0 + per]); best = min(best, (d, s0))
        s0 = best[1]; sel = ii[s0:s0 + per + 1]
    else:
        sel = ii
    # parts_extract removed the pelvis yaw of every tick; put the swing back, relative to the segment's mean heading
    yw = np.unwrap(yaw[sel])
    if loop:                                           # strafing circles the target: the heading drifts steadily, which a loop cannot carry
        yw = yw - (yw[-1] - yw[0]) * np.arange(len(yw)) / (len(yw) - 1)
    else:
        yw = yw - yw[0]
    # The robot faces where its HEAD looks (the game holds an angled stance with the head dead on the target), so the heading
    # reference is the head's world yaw, not the pelvis's: the head ends up facing forward and the body keeps its stance.
    hq = Q['head'][sel] if 'head' in Q else None; hy = np.zeros(len(sel))
    if hq is not None and (~np.isnan(hq).any(1)).any():
        okh = ~np.isnan(hq).any(1); fw = Rot.from_quat(hq[okh]).apply([0, 0, 1]); hv = np.unwrap(np.arctan2(fw[:, 0], fw[:, 2]))
        hy = np.interp(np.arange(len(sel)), np.where(okh)[0], hv)
    hd = yw + hy; ref = hd.mean() if loop else hd[0]
    yrel = Rot.from_rotvec(np.outer(yw - ref, [0, 1, 0]))
    def withyaw(n):
        q = Q[n][sel]; bad = np.isnan(q).any(1); q = np.where(bad[:, None], [0, 0, 0, 1], q)
        out = (yrel * Rot.from_quat(q)).as_quat(); out[bad] = np.nan; return out
    tt = (ticks[sel] - ticks[sel[0]]) / RATE
    dur = float(tt[-1]) if not loop else (len(sel) - 1) / RATE
    nfr = max(2, int(round(dur * FPS)) + (0 if loop else 1))
    tq = np.linspace(0, dur, nfr, endpoint=not loop)
    # per-bone slerp resample
    frames = {n: slerp_series(withyaw(n), tt, tq) for n in names}
    AIR = {51, 52, 53, 55}                               # jump rise / fall: the height is the world's, not the animation's
    ry = np.interp(tq, tt, rd[sel][:, 1] - (np.mean(rd[sel][:, 1]) if loop else rd[sel][0, 1])) * (0 if a in AIR else 1)           # hip height change, game units, about its mean
    # travel speed over the selection (forward/side in the pelvis frame), metres per second
    p = rd[sel]; y = yaw[sel]; v = np.diff(p, axis=0) * RATE
    fwd = float(np.mean([v[k, 0] * math.sin(y[k]) + v[k, 2] * math.cos(y[k]) for k in range(len(v))])) * UNIT
    side = float(np.mean([v[k, 0] * math.cos(y[k]) - v[k, 2] * math.sin(y[k]) for k in range(len(v))])) * UNIT
    with open(os.path.join(OUT, 'anim_%s.txt' % a), 'w') as f:
        f.write('META %.4f %s %.3f %.3f\n' % (dur, 'loop' if loop else 'once', fwd, side))
        for j in range(nfr):
            f.write('POSE %d anim%s_%03d\n' % (j, a, j))
            for n in names:
                q = frames[n][j]
                if not np.isnan(q).any(): f.write('%s %.6f %.6f %.6f %.6f\n' % (n, *q))
            f.write('ROOTY %.5f\n' % ry[j])
    meta[str(a)] = dict(loop=loop, duration=dur, frames=nfr, fwd=fwd, side=side, period_ticks=per, src_ticks=len(ii))
    print('anim %-4s %s dur %.2fs frames %3d  fwd %+.2f m/s  side %+.2f m/s' % (a, 'loop' if loop else 'once', dur, nfr, fwd, side))
json.dump(meta, open(os.path.join(OUT, 'anims.json'), 'w'), indent=1)
print('wrote', len(meta), 'animations to', OUT)
