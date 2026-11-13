#include "SleepImages.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <strings.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "CrossPointState.h"

namespace sleepimages {

const uint8_t IMAGE_MODES[IMAGE_MODE_COUNT] = {
    CrossPointSettings::CUSTOM,
    CrossPointSettings::COVER_CUSTOM,
    CrossPointSettings::TRANSPARENT_CUSTOM,
    CrossPointSettings::COVER_TRANSPARENT,
};

namespace {
constexpr size_t NAME_BUFFER = 256;

template <size_t N>
bool directlyInside(const std::string_view path, const char* const (&dirs)[N]) {
  for (const char* dir : dirs) {
    const size_t n = std::strlen(dir);
    if (path.size() > n + 1 && strncasecmp(path.data(), dir, n) == 0 && path[n] == '/' &&
        path.find('/', n + 1) == std::string_view::npos) {
      return true;
    }
  }
  return false;
}

const char* const* dirsFor(const Kind kind, size_t& count) {
  count = 2;
  return kind == Kind::Overlay ? OVERLAY_DIRS : WALLPAPER_DIRS;
}
}  // namespace

bool kindOf(const std::string_view path, Kind& out) {
  if (directlyInside(path, OVERLAY_DIRS)) {
    out = Kind::Overlay;
    return true;
  }
  if (directlyInside(path, WALLPAPER_DIRS)) {
    out = Kind::Wallpaper;
    return true;
  }
  return false;
}

bool usableName(const std::string_view name, const Kind kind) {
  if (name.empty() || name[0] == '.') return false;
  if (FsHelpers::hasBmpExtension(name)) return true;
  return kind == Kind::Overlay && FsHelpers::hasPngExtension(name);
}

bool modeShows(const Kind kind, const uint8_t mode) {
  if (kind == Kind::Overlay) {
    return mode == CrossPointSettings::TRANSPARENT_CUSTOM || mode == CrossPointSettings::COVER_TRANSPARENT;
  }
  return mode == CrossPointSettings::CUSTOM || mode == CrossPointSettings::COVER_CUSTOM;
}

PinResult pin(const std::string& path) {
  uint8_t mode = SETTINGS.sleepScreen;
  if (path.empty()) {
    APP_STATE.pinnedSleepImage.clear();
  } else {
    Kind kind;
    const size_t slash = path.find_last_of('/');
    const std::string_view name = std::string_view{path}.substr(slash == std::string::npos ? 0 : slash + 1);
    if (!kindOf(path, kind) || !usableName(name, kind) || !Storage.exists(path.c_str())) {
      LOG_ERR("SLPIMG", "Not a usable sleep image: %s", path.c_str());
      return PinResult::NotUsable;
    }
    APP_STATE.pinnedSleepImage = path;
    // The pin only shows in a mode that reads its folder.
    if (!modeShows(kind, mode))
      mode = kind == Kind::Overlay ? CrossPointSettings::TRANSPARENT_CUSTOM : CrossPointSettings::CUSTOM;
  }
  bool ok = true;
  if (mode != SETTINGS.sleepScreen) {
    SETTINGS.sleepScreen = mode;
    if (!SETTINGS.saveToFile()) {
      LOG_ERR("SLPIMG", "Failed to save sleep screen %u", mode);
      ok = false;
    }
  }
  if (!APP_STATE.saveToFile()) {
    LOG_ERR("SLPIMG", "Failed to save pinned sleep image %s", path.c_str());
    ok = false;
  }
  LOG_INF("SLPIMG", "Pinned sleep image: %s", path.empty() ? "(random)" : path.c_str());
  return ok ? PinResult::Ok : PinResult::SaveFailed;
}

bool isPinned(const std::string& path) {
  const std::string& pinned = APP_STATE.pinnedSleepImage;
  return !pinned.empty() && pinned.size() == path.size() && strcasecmp(pinned.c_str(), path.c_str()) == 0;
}

void list(const Kind kind, std::vector<std::string>& out, const size_t maxCount) {
  out.clear();
  auto name = makeUniqueNoThrow<char[]>(NAME_BUFFER);
  if (!name) {
    LOG_ERR("SLPIMG", "OOM: file name buffer");
    return;
  }
  size_t dirCount = 0;
  const char* const* dirs = dirsFor(kind, dirCount);

  // Count first so the vector is sized once (no regrowth on the C3's heap).
  size_t total = 0;
  for (size_t d = 0; d < dirCount; ++d) {
    HalFile dir = Storage.open(dirs[d]);
    if (!dir || !dir.isDirectory()) {
      if (dir) dir.close();
      continue;
    }
    for (HalFile file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (!file.isDirectory()) {
        file.getName(name.get(), NAME_BUFFER);
        if (usableName(name.get(), kind)) ++total;
      }
      file.close();
    }
    dir.close();
  }
  out.reserve(total < maxCount ? total : maxCount);

  for (size_t d = 0; d < dirCount && out.size() < maxCount; ++d) {
    HalFile dir = Storage.open(dirs[d]);
    if (!dir || !dir.isDirectory()) {
      if (dir) dir.close();
      continue;
    }
    for (HalFile file = dir.openNextFile(); file && out.size() < maxCount; file = dir.openNextFile()) {
      if (!file.isDirectory()) {
        file.getName(name.get(), NAME_BUFFER);
        if (usableName(name.get(), kind)) out.push_back(std::string(dirs[d]) + '/' + name.get());
      }
      file.close();
    }
    dir.close();
  }
  FsHelpers::sortFileList(out);
}

}  // namespace sleepimages
