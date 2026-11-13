#include "BookCacheUtils.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Xtc.h>

#include <functional>

#include "BookmarkUtil.h"
#include "ClippingStore.h"
#include "CrossPointState.h"
#include "RecentBooksStore.h"

bool isBookCacheDirectoryName(const char* name) {
  if (!name) {
    return false;
  }

  constexpr char EPUB_PREFIX[] = "epub_";
  constexpr char TXT_PREFIX[] = "txt_";
  constexpr char XTC_PREFIX[] = "xtc_";

  return strncmp(name, EPUB_PREFIX, std::size(EPUB_PREFIX) - 1) == 0 ||
         strncmp(name, TXT_PREFIX, std::size(TXT_PREFIX) - 1) == 0 ||
         strncmp(name, XTC_PREFIX, std::size(XTC_PREFIX) - 1) == 0;
}

void clearBookCache(const std::string& path) {
  if (FsHelpers::hasReflowableBookExtension(path)) {
    Epub(path, "/.crosspoint").clearCache();
  } else if (FsHelpers::hasXtcExtension(path)) {
    Xtc(path, "/.crosspoint").clearCache();
  } else {
    return;
  }
  LOG_DBG("BookCache", "Done checking metadata cache for: %s", path.c_str());
}

bool removeBookFile(const std::string& path) {
  if (!Storage.remove(path.c_str())) return false;
  clearBookCache(path);
  return ClippingStore::deleteForFilePath(path, "epub");
}

std::string bookCachePath(const std::string& path) {
  const char* prefix = nullptr;
  if (FsHelpers::hasReflowableBookExtension(path)) {
    prefix = "epub_";
  } else if (FsHelpers::hasXtcExtension(path)) {
    prefix = "xtc_";
  } else {
    return "";
  }
  return std::string("/.crosspoint/") + prefix + std::to_string(std::hash<std::string>{}(path));
}

namespace {
// Moves one piece of saved state. A missing source is fine (nothing saved);
// an existing target is not, so another book's state is never overwritten.
bool moveStatePath(const std::string& oldPath, const std::string& newPath, bool& moved) {
  moved = false;
  if (oldPath.empty() || !Storage.exists(oldPath.c_str())) return true;
  if (Storage.exists(newPath.c_str())) {
    LOG_ERR("BookCache", "Book state target already exists: %s", newPath.c_str());
    return false;
  }
  moved = Storage.rename(oldPath.c_str(), newPath.c_str());
  if (!moved) LOG_ERR("BookCache", "Failed to move book state: %s -> %s", oldPath.c_str(), newPath.c_str());
  return moved;
}

void rollBackStatePath(const std::string& oldPath, const std::string& newPath, const bool moved) {
  if (moved && !Storage.rename(newPath.c_str(), oldPath.c_str())) {
    LOG_ERR("BookCache", "Failed to roll back book state: %s -> %s", newPath.c_str(), oldPath.c_str());
  }
}

// Saved state that sits beside the book under its own name + suffix.
constexpr const char* SIDECARS[] = {".key", ".rights"};
}  // namespace

bool moveBookPreservingState(const std::string& oldPath, const std::string& newPath) {
  const std::string oldCachePath = bookCachePath(oldPath);
  const std::string newCachePath = bookCachePath(newPath);
  const bool hasBookmarks = FsHelpers::hasReflowableBookExtension(oldPath);
  const std::string oldBookmarkPath = hasBookmarks ? BookmarkUtil::getBookmarkPath(oldPath) : "";
  const std::string newBookmarkPath = hasBookmarks ? BookmarkUtil::getBookmarkPath(newPath) : "";

  bool cacheMoved = false;
  bool bookmarksMoved = false;
  bool sidecarMoved[std::size(SIDECARS)] = {};
  const auto rollBack = [&]() {
    for (size_t i = 0; i < std::size(SIDECARS); ++i) {
      rollBackStatePath(oldPath + SIDECARS[i], newPath + SIDECARS[i], sidecarMoved[i]);
    }
    rollBackStatePath(oldBookmarkPath, newBookmarkPath, bookmarksMoved);
    rollBackStatePath(oldCachePath, newCachePath, cacheMoved);
  };

  if (!moveStatePath(oldCachePath, newCachePath, cacheMoved)) return false;
  if (!moveStatePath(oldBookmarkPath, newBookmarkPath, bookmarksMoved)) {
    rollBack();
    return false;
  }
  for (size_t i = 0; i < std::size(SIDECARS); ++i) {
    if (!moveStatePath(oldPath + SIDECARS[i], newPath + SIDECARS[i], sidecarMoved[i])) {
      rollBack();
      return false;
    }
  }
  // Moves the book file itself along with its clippings.
  if (!ClippingStore::moveBook(oldPath, newPath)) {
    LOG_ERR("BookCache", "Failed to move book: %s -> %s", oldPath.c_str(), newPath.c_str());
    rollBack();
    return false;
  }

  RECENT_BOOKS.updatePath(oldPath, newPath, oldCachePath, newCachePath);
  if (APP_STATE.openEpubPath == oldPath) {
    APP_STATE.openEpubPath = newPath;
    if (!APP_STATE.saveToFile()) LOG_ERR("BookCache", "Failed to save moved open-book path");
  }
  LOG_INF("BookCache", "Moved book with its state: %s -> %s", oldPath.c_str(), newPath.c_str());
  return true;
}
