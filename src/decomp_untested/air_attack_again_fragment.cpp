// sub_821DEC58(control): whether an attack in the air (substates 2-10) can
// fire again: +438 below its parameter limit and sub_82257F50 agreeing.
void AirAttackAgain(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 96;
  struct Entry {
    uint32_t limit, weapon, variant;
  };
  Entry e;
  switch (c.H(274)) {
    case 8: e = {3314, 2, 0}; break;
    case 9: e = {3410, 2, 1}; break;
    case 10: e = {3362, 2, 2}; break;
    case 2: e = {3298, 1, 0}; break;
    case 3: e = {3394, 1, 1}; break;
    case 4: e = {3346, 1, 2}; break;
    case 5: e = {3282, 0, 0}; break;
    case 6: e = {3378, 0, 1}; break;
    case 7: e = {3330, 0, 2}; break;
    default: ctx.r3.u64 = 0; return;
  }
  uint32_t r = 0;
  if (c.S16(438) < int32_t(Control{base, c.W(76)}.H(e.limit))) {
    k.k.r6.u64 = 1;
    k.k.r5.u64 = e.variant;
    k.k.r4.u64 = e.weapon;
    if (k.Call(__imp__sub_82257F50, c.a) != 0) r = 1;
  }
  ctx.r3.u64 = r;
}

