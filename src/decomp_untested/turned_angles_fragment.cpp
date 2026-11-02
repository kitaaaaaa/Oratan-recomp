// sub_821C01D8(control, turn): the robot's angles (+8 / +6 / +10) turned by
// `turn` about the vertical axis, given back as the three angles +16 / +14
// / +18. It builds the matrix on the guest matrix stack, transforms the two
// unit vectors at 0x8201ED5C and reads the angles back one axis at a time
// (sub_822A7398, picking the better-conditioned pair; the value at
// 0x820750EC times the float at 0x8200137C is the quarter turn).
void TurnedAngles(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  const uint32_t frame = ctx.r1.u32 - 208;
  k.k.r1.u64 = frame;
  const int64_t turn = int16_t(ctx.r4.u32);
  const auto mem = [&](uint32_t address) {
    return std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, address)));
  };
  const auto atan = [&](float a, float b) {
    k.k.f1.f64 = double(a);
    k.k.f2.f64 = double(b);
    k.Call(__imp__sub_822A7398);
    return float(k.k.f1.f64);
  };
  const auto transform = [&](uint32_t out, uint32_t in) {
    k.k.r4.u64 = in;
    k.Call(__imp__sub_822B61B0, out);
  };
  const auto rotate = [&](void (*fn)(PPCContext&, uint8_t*), int64_t angle) {
    k.k.r3.s64 = angle;
    k.Call(fn);
  };
  constexpr uint32_t kUnitVectors = 0x8201ED5C;
  const float half = mem(0x8200137C);
  const float quarter = mem(0x820750EC);

  k.Call(__imp__sub_822B5A18);
  k.Call(__imp__sub_822B5908);
  rotate(__imp__sub_822A74F8, c.S16(8));
  rotate(__imp__sub_822A74E8, c.S16(6));
  rotate(__imp__sub_822A7508, c.S16(10));
  rotate(__imp__sub_822A74F8, turn);
  transform(frame + 128, kUnitVectors);
  transform(frame + 80, kUnitVectors + 12);

  // About the vertical axis.
  float yaw;
  {
    const float x = mem(frame + 80), z = mem(frame + 88);
    if (std::fabs(x) < std::fabs(z)) {
      yaw = atan(-x, z);
    } else {
      const float r = atan(z, x);
      yaw = float(-std::fma(double(quarter), double(half), -double(r)));
    }
  }
  k.Call(__imp__sub_822B5908);
  k.k.f1.f64 = -double(yaw);
  k.Call(__imp__sub_822A76A8);
  transform(frame + 144, frame + 128);
  transform(frame + 96, frame + 80);
  // About the sideways axis.
  float pitch;
  {
    const float y = mem(frame + 100), z = mem(frame + 104);
    if (std::fabs(y) < std::fabs(z)) {
      pitch = atan(y, z);
    } else {
      const float q = quarter * half;
      pitch = q - atan(z, y);
    }
  }
  k.Call(__imp__sub_822B5908);
  k.k.f1.f64 = -double(pitch);
  k.Call(__imp__sub_822A7668);
  transform(frame + 112, frame + 144);
  // About the forward axis.
  float roll;
  {
    const float x = mem(frame + 112), y = mem(frame + 116);
    if (std::fabs(x) < std::fabs(y)) {
      roll = atan(x, y);
    } else {
      const float q = quarter * half;
      roll = q - atan(y, x);
    }
  }
  k.Call(__imp__sub_822B5A78);
  const double scale = GuestDouble(base, 0x8201B9C8);
  const auto to_angle = [&](float v) {
    const double d = (double(v) * scale) / double(quarter);
    if (std::isnan(d)) return uint32_t(0);
    if (d >= double(INT_MAX)) return uint32_t(0xFFFF);
    return uint32_t(int32_t(d)) & 0xFFFF;
  };
  c.SetH(18, to_angle(roll));
  c.SetH(14, to_angle(pitch));
  c.SetH(16, to_angle(yaw));
  ctx.r3.u64 = k.k.r3.u64;
}
