// Locating the player's game files.
//
// The XBLA game ships as one STFS ("LIVE") package. Players can drop that file
// straight into assets/; on first launch it is unpacked next to itself with
// the SDK's STFS reader (the same code Xenia uses), and later launches use
// the unpacked files directly.

#include "game_files.h"

#include <rex/filesystem/devices/stfs_container_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/logging.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace oratan {
namespace {

constexpr uint32_t kTitleId = 0x58410985;      // Virtual-On OT
constexpr uint32_t kStfsTitleIdOffset = 0x360;  // in the package metadata
constexpr int kMaxSearchDepth = 6;

bool HasGame(const fs::path& dir) {
  std::error_code ec;
  return fs::is_regular_file(dir / "default.xex", ec) && fs::is_directory(dir / "media", ec);
}

// Searches `dir` and its subfolders (players sometimes copy a whole Xenia
// content tree) for a folder with default.xex and media/.
fs::path FindExtracted(const fs::path& dir) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return {};
  if (HasGame(dir)) return dir;
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (it.depth() >= kMaxSearchDepth) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory(ec) && HasGame(it->path())) return it->path();
  }
  return {};
}

bool IsGamePackage(const fs::path& file) {
  std::ifstream in(file, std::ios::binary);
  std::array<uint8_t, kStfsTitleIdOffset + 4> header{};
  if (!in.read(reinterpret_cast<char*>(header.data()), header.size())) return false;
  const std::string_view magic(reinterpret_cast<const char*>(header.data()), 4);
  if (magic != "LIVE" && magic != "PIRS" && magic != "CON ") return false;
  const uint8_t* t = header.data() + kStfsTitleIdOffset;
  const uint32_t title_id = (uint32_t(t[0]) << 24) | (t[1] << 16) | (t[2] << 8) | t[3];
  return title_id == kTitleId;
}

fs::path FindPackage(const fs::path& dir) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return {};
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (it.depth() >= kMaxSearchDepth) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file(ec) && it->file_size(ec) > (1 << 20) && IsGamePackage(it->path())) {
      return it->path();
    }
  }
  return {};
}

bool CopyEntry(rex::filesystem::Entry* entry, const fs::path& out_path, std::string* error) {
  rex::filesystem::File* file = nullptr;
  if (entry->Open(0x80000000 /* GENERIC_READ */, &file) != 0 || !file) {
    *error = "cannot read " + entry->path() + " from the package";
    return false;
  }
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  std::vector<uint8_t> buffer(4 << 20);
  size_t offset = 0;
  bool ok = static_cast<bool>(out);
  while (ok && offset < entry->size()) {
    size_t read = 0;
    if (file->ReadSync(buffer, offset, &read) != 0 || read == 0) {
      ok = false;
      break;
    }
    out.write(reinterpret_cast<const char*>(buffer.data()), read);
    ok = static_cast<bool>(out);
    offset += read;
  }
  file->Destroy();
  // The last buffered write is only flushed here (e.g. a full disk).
  out.close();
  ok = ok && !out.fail();
  if (!ok) *error = "cannot write " + out_path.string();
  return ok;
}

// Unpacks the package into `out_dir`. default.xex is written last, so an
// interrupted unpack is not mistaken for a complete one next launch.
bool ExtractPackage(const fs::path& package, const fs::path& out_dir, std::string* error) {
  REXLOG_INFO("Unpacking game package {} into {}", package.string(), out_dir.string());
  rex::filesystem::StfsContainerDevice device("\\Device\\Import", package);
  if (!device.Initialize()) {
    *error = "the game package could not be read (is it damaged?):\n" + package.string();
    return false;
  }
  rex::filesystem::Entry* root = device.ResolvePath("");
  if (!root) {
    *error = "the game package is empty:\n" + package.string();
    return false;
  }

  rex::filesystem::Entry* xex = nullptr;
  std::vector<std::pair<rex::filesystem::Entry*, fs::path>> stack{{root, out_dir}};
  while (!stack.empty()) {
    auto [dir, dir_path] = stack.back();
    stack.pop_back();
    std::error_code ec;
    fs::create_directories(dir_path, ec);
    for (const auto& child : dir->children()) {
      const fs::path child_path = dir_path / child->name();
      if (child->attributes() & rex::filesystem::kFileAttributeDirectory) {
        stack.emplace_back(child.get(), child_path);
      } else if (dir == root && child->name() == "default.xex") {
        xex = child.get();
      } else if (!CopyEntry(child.get(), child_path, error)) {
        return false;
      }
    }
  }
  if (!xex) {
    *error = "the package does not contain default.xex:\n" + package.string();
    return false;
  }
  return CopyEntry(xex, out_dir / "default.xex", error);
}

}  // namespace

fs::path LocateGameFiles(const fs::path& exe_dir, std::string* error) {
  // assets/ next to the exe (release layout), then the repo's assets/ for
  // developer builds in out/build/<preset>/.
  const fs::path candidates[] = {exe_dir / "assets", exe_dir / ".." / ".." / ".." / "assets"};
  for (const auto& dir : candidates) {
    if (auto found = FindExtracted(dir); !found.empty()) return fs::weakly_canonical(found);
  }
  for (const auto& dir : candidates) {
    if (auto package = FindPackage(dir); !package.empty()) {
      if (!ExtractPackage(package, dir, error)) return {};
      return fs::weakly_canonical(dir);
    }
  }
  *error = "Virtual-On OT game files not found.\n\nPut your copy of the game into:\n\n" +
           (exe_dir / "assets").string() +
           "\n\nEither the game package file (2A944528D84678B7C9F0270A564B8E653520EF72) "
           "or the extracted files (default.xex and the media folder) will work.";
  return {};
}

}  // namespace oratan
