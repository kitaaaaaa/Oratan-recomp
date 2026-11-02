// sub_821DD810(control): to state 11 (button state 29), speed from the
// robot's data +1568.
void ToState11(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 96;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  c.SetH(274, 0);
  c.SetB(286, 0);
  c.SetH(272, 11);
  for (uint32_t o : {292u, 12u}) c.SetH(o, 0);
  c.SetW(584, 0);
  for (uint32_t o : {598u, 594u, 276u}) c.SetH(o, 0);
  c.SetB(568, 29);
  for (uint32_t o : {4492u, 592u}) c.SetH(o, 0);
  const uint32_t data = Control{base, game_address}.W(game::kGameRobotData);
  c.SetW(372, Control{base, data}.W(1568));
  ctx.r3.u64 = game_address;
}

