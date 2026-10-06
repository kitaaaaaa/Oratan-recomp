// Compatibility shims for older MSVC STL versions.
//
// The prebuilt ReXGlue SDK libraries (spdlog.lib) are compiled against a
// recent MSVC STL, which calls vectorised helpers that only exist in the
// static part of newer msvcprt.lib. Building with an older Visual Studio 2022
// toolset (e.g. 14.34) leaves them undefined at link time.
//
// They are defined weak, so a toolset that ships the real ones wins.

#include <cstddef>

namespace {

bool InSet(unsigned char c, const unsigned char* set, size_t set_len) {
  for (size_t i = 0; i < set_len; ++i) {
    if (set[i] == c) return true;
  }
  return false;
}

}  // namespace

extern "C" {

__attribute__((weak)) size_t __stdcall __std_find_first_of_trivial_pos_1(
    const void* haystack, size_t haystack_len, const void* needle,
    size_t needle_len) noexcept {
  auto h = static_cast<const unsigned char*>(haystack);
  auto n = static_cast<const unsigned char*>(needle);
  for (size_t i = 0; i < haystack_len; ++i) {
    if (InSet(h[i], n, needle_len)) return i;
  }
  return static_cast<size_t>(-1);
}

__attribute__((weak)) size_t __stdcall __std_find_last_of_trivial_pos_1(
    const void* haystack, size_t haystack_len, const void* needle,
    size_t needle_len) noexcept {
  auto h = static_cast<const unsigned char*>(haystack);
  auto n = static_cast<const unsigned char*>(needle);
  for (size_t i = haystack_len; i-- > 0;) {
    if (InSet(h[i], n, needle_len)) return i;
  }
  return static_cast<size_t>(-1);
}

}  // extern "C"
