// Hand-decompiled game functions: shared switches and the check helper.
//
// Each file in src/decomp/ replaces recompiled game functions with readable
// C++ that behaves identically. Every replacement hook:
//   - calls the original (__imp__sub_XXXXXXXX) instead when decomp_use_originals
//     is on, as an escape hatch if a rewrite misbehaves;
//   - with decomp_verify on, the first time it runs, compares the rewrite with
//     the original over every input it can construct and logs the result
//     ("decomp verify sub_XXXXXXXX: N inputs, M mismatches"), using Verifier.

#pragma once

#include <rex/cvar.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

REXCVAR_DECLARE(bool, decomp_use_originals);
REXCVAR_DECLARE(bool, decomp_verify);

namespace oratan::decomp {

// A stretch of guest memory a function reads or writes.
struct GuestRange {
  uint32_t address;
  uint32_t size;
  const char* name;
};

// A copy of some guest ranges.
class Snapshot {
 public:
  explicit Snapshot(std::vector<GuestRange> ranges);

  void Capture(const uint8_t* base);
  void Restore(uint8_t* base) const;
  // The first byte where this and `other` (same ranges) differ, as
  // "<range> +<offset>: <this> vs <other>", or "" if they're equal.
  std::string FirstDifference(const Snapshot& other) const;

 private:
  std::vector<GuestRange> ranges_;
  std::vector<uint8_t> bytes_;
};

// The XEX's data section (the range F9 saves), for the whole-section check.
inline constexpr GuestRange kDataSection = {0x82890000, 0x01171DFC, "data section"};

// Compares a rewrite with the original over many inputs.
//
// The declared ranges are everything the function reads or writes in guest
// memory. They are saved when the Verifier is made and restored by Finish.
// For each input, Check writes the starting state (setup), runs the original,
// restores the declared ranges, runs the rewrite, and compares their results
// and the declared ranges afterwards. A mismatch is re-run once before it
// counts, and only if the re-run finds the same difference (a third run
// settles a disagreement); the first 20 are logged with the input.
//
// The first `section_samples` inputs also compare the whole data section, to
// catch writes outside the declared ranges: bytes the original changed are put
// back before the rewrite runs, and only bytes where the two runs end up
// different count. Other guest threads change some bytes all the time (a
// counter at 839F65AA, for one): the Verifier finds them by watching the
// section for a moment before the sweep and leaves them out.
class Verifier {
 public:
  Verifier(const char* function, uint8_t* base, std::vector<GuestRange> ranges,
           uint32_t section_samples = 50);

  // `original` and `rewrite` return the function's result (0 if none).
  // `describe` names the input, for the log. Returns whether they matched.
  bool Check(const std::function<void()>& setup, const std::function<int64_t()>& original,
             const std::function<int64_t()>& rewrite,
             const std::function<std::string()>& describe);

  // Restores the saved state and logs the summary line.
  void Finish();

  uint32_t inputs() const { return inputs_; }
  uint32_t mismatches() const { return mismatches_; }

 private:
  // One comparison; "" if they matched, or what differed.
  std::string Compare(const std::function<void()>& setup,
                      const std::function<int64_t()>& original,
                      const std::function<int64_t()>& rewrite, bool section);

  const char* function_;
  uint8_t* base_;
  Snapshot saved_, start_, after_original_, after_rewrite_;
  uint32_t section_samples_;
  std::vector<uint8_t> section_start_, section_original_, section_rewrite_;
  std::vector<uint8_t> section_volatile_;  // 1 = changes by itself; not compared
  uint32_t inputs_ = 0, mismatches_ = 0, unstable_ = 0;
};

}  // namespace oratan::decomp
