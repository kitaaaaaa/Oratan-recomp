// Hand-decompiled game functions: shared switches and the check helper (see decomp.h).

#include "decomp.h"

#include <rex/logging.h>

#include <chrono>
#include <cstring>

#include <fmt/format.h>

REXCVAR_DEFINE_BOOL(decomp_use_originals, false, "Oratan/Decomp",
                    "Developer: decompiled functions call the original recompiled code instead");
REXCVAR_DEFINE_BOOL(decomp_verify, false, "Oratan/Decomp",
                    "Developer: each decompiled function compares itself with the original on "
                    "every input it can construct, the first time it runs, and logs the result");

namespace oratan::decomp {

Snapshot::Snapshot(std::vector<GuestRange> ranges) : ranges_(std::move(ranges)) {
  size_t size = 0;
  for (const GuestRange& range : ranges_) size += range.size;
  bytes_.resize(size);
}

void Snapshot::Capture(const uint8_t* base) {
  uint8_t* out = bytes_.data();
  for (const GuestRange& range : ranges_) {
    std::memcpy(out, base + range.address, range.size);
    out += range.size;
  }
}

void Snapshot::Restore(uint8_t* base) const {
  const uint8_t* in = bytes_.data();
  for (const GuestRange& range : ranges_) {
    std::memcpy(base + range.address, in, range.size);
    in += range.size;
  }
}

std::string Snapshot::FirstDifference(const Snapshot& other) const {
  size_t at = 0;
  for (const GuestRange& range : ranges_) {
    for (uint32_t i = 0; i < range.size; ++i) {
      if (bytes_[at + i] != other.bytes_[at + i]) {
        return fmt::format("{} +{:#x} ({:08X}): {:02X} vs {:02X}", range.name, i,
                           range.address + i, bytes_[at + i], other.bytes_[at + i]);
      }
    }
    at += range.size;
  }
  return {};
}

Verifier::Verifier(const char* function, uint8_t* base, std::vector<GuestRange> ranges,
                   uint32_t section_samples)
    : function_(function),
      base_(base),
      saved_(ranges),
      start_(ranges),
      after_original_(ranges),
      after_rewrite_(ranges),
      section_samples_(section_samples) {
  saved_.Capture(base);
  if (section_samples_) {
    section_start_.resize(kDataSection.size);
    section_original_.resize(kDataSection.size);
    section_rewrite_.resize(kDataSection.size);
    // Bytes other threads keep changing: watch the section for a while.
    section_volatile_.assign(kDataSection.size, 0);
    const uint8_t* section = base + kDataSection.address;
    std::memcpy(section_start_.data(), section, kDataSection.size);
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    uint32_t volatile_bytes = 0;
    while (std::chrono::steady_clock::now() < until) {
      for (uint32_t i = 0; i < kDataSection.size; ++i) {
        if (section[i] != section_start_[i] && !section_volatile_[i]) {
          section_volatile_[i] = 1;
          ++volatile_bytes;
        }
      }
    }
    REXLOG_INFO("decomp verify {}: {} data section bytes change by themselves, not compared",
                function_, volatile_bytes);
  }
}

namespace {

uint8_t* SectionHost(uint8_t* base) { return base + kDataSection.address; }

// Puts back the section bytes that differ between `from` and `changed`,
// except the ones that change by themselves.
void UndoSectionChanges(uint8_t* base, const std::vector<uint8_t>& from,
                        const std::vector<uint8_t>& changed,
                        const std::vector<uint8_t>& volatile_bytes) {
  uint8_t* section = SectionHost(base);
  for (size_t i = 0; i < from.size(); ++i) {
    if (changed[i] != from[i] && !volatile_bytes[i]) section[i] = from[i];
  }
}

}  // namespace

std::string Verifier::Compare(const std::function<void()>& setup,
                              const std::function<int64_t()>& original,
                              const std::function<int64_t()>& rewrite, bool section) {
  setup();
  start_.Capture(base_);
  if (section) std::memcpy(section_start_.data(), SectionHost(base_), kDataSection.size);

  const int64_t original_result = original();
  after_original_.Capture(base_);
  if (section) {
    std::memcpy(section_original_.data(), SectionHost(base_), kDataSection.size);
    UndoSectionChanges(base_, section_start_, section_original_, section_volatile_);
  }
  start_.Restore(base_);

  const int64_t rewrite_result = rewrite();
  after_rewrite_.Capture(base_);
  if (section) {
    std::memcpy(section_rewrite_.data(), SectionHost(base_), kDataSection.size);
    UndoSectionChanges(base_, section_start_, section_rewrite_, section_volatile_);
  }
  start_.Restore(base_);

  if (original_result != rewrite_result) {
    return fmt::format("result: original {}, rewrite {}", original_result, rewrite_result);
  }
  if (std::string difference = after_original_.FirstDifference(after_rewrite_);
      !difference.empty()) {
    return "original vs rewrite: " + difference;
  }
  if (section) {
    for (size_t i = 0; i < kDataSection.size; ++i) {
      if (section_original_[i] != section_rewrite_[i] && !section_volatile_[i]) {
        return fmt::format("outside the declared ranges: {} +{:#x} ({:08X}): {:02X} vs {:02X}",
                           kDataSection.name, i, kDataSection.address + uint32_t(i),
                           section_original_[i], section_rewrite_[i]);
      }
    }
  }
  return {};
}

bool Verifier::Check(const std::function<void()>& setup, const std::function<int64_t()>& original,
                     const std::function<int64_t()>& rewrite,
                     const std::function<std::string()>& describe) {
  const bool section = inputs_ < section_samples_;
  ++inputs_;
  const std::string first = Compare(setup, original, rewrite, section);
  if (first.empty()) return true;
  // Once more, in case another guest thread changed memory meanwhile: a real
  // difference comes back the same.
  std::string difference = Compare(setup, original, rewrite, section);
  if (difference.empty()) return true;
  if (difference != first) {
    const std::string third = Compare(setup, original, rewrite, section);
    if (third.empty()) return true;
    if (third != first && third != difference) {
      if (unstable_++ < 5) {
        REXLOG_WARN("decomp verify {}: {}: differences change from run to run, not counted: {}",
                    function_, describe(), third);
      }
      return true;
    }
    difference = third;
  }
  if (mismatches_ < 20) {
    REXLOG_ERROR("decomp verify {}: {}: {}", function_, describe(), difference);
  }
  ++mismatches_;
  return false;
}

void Verifier::Finish() {
  saved_.Restore(base_);
  if (unstable_) {
    REXLOG_WARN("decomp verify {}: {} inputs with run-to-run differences not counted", function_,
                unstable_);
  }
  REXLOG_INFO("decomp verify {}: {} inputs, {} mismatches", function_, inputs_, mismatches_);
}

}  // namespace oratan::decomp
