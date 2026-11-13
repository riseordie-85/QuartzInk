#pragma once

#include <array>
#include <string>
#include <vector>

#include "HomeCoverCache.h"
#include "RecentBooksStore.h"
#include "UiAppHost.h"

class MappedInputManager;

// QuartzInk's Home screen: a clock bar with Reading Stats and Settings, the book
// being read with its progress and time, the next three recent books, one
// button per shelf (each folder inside /Books), and Browse Files, Library and
// File Transfer along the bottom. Shelves page in sixes.
//
// HomeActivity owns the data and the navigation; this class draws, routes
// touch, and moves the button-device focus.
class QuartzHomeUi final : public UiAppHost {
 public:
  // The folder whose subfolders are shelves.
  static constexpr const char* SHELF_ROOT = "/Books";
  static constexpr int MAX_BOOKS = 4;  // the current book plus three recent ones
  static constexpr int SHELVES_PER_PAGE = 6;
  static_assert(MAX_BOOKS <= static_cast<int>(HomeCoverCache::MAX_COVERS));

  // Action values. Books are 0..MAX_BOOKS-1; shelves are SHELF_BASE + index.
  static constexpr int ACTION_STATS = 1000;
  static constexpr int ACTION_SETTINGS = 1001;
  static constexpr int ACTION_FILES = 1002;
  static constexpr int ACTION_LIBRARY = 1003;
  static constexpr int ACTION_TRANSFER = 1004;
  static constexpr int SHELF_BASE = 2000;

  struct CurrentBook {
    std::string shelf;              // shelf name, empty when unknown
    float progressPercent = -1.0f;  // -1 when unknown
    uint32_t readingSeconds = 0;
    uint32_t timeLeftSeconds = 0;  // 0 when the reader has no estimate yet
  };

  explicit QuartzHomeUi(GfxRenderer& renderer);

  void begin(const std::vector<RecentBook>& books, CurrentBook current, std::vector<std::string> shelves);
  void refreshCoverPaths();

  // Touch and buttons. Returns the action to run (-1 for none) and sets
  // `redraw` when the focus or shelf page changed.
  int handleInput(MappedInputManager& input, bool& redraw);

  // CrossPoint cover thumbs are generated per height; slots crop to width.
  int thumbHeightFor(size_t index) const;
  bool takeThumbSizesChanged();

  // Folders inside SHELF_ROOT, sorted case-insensitively. Hidden folders are skipped.
  static std::vector<std::string> scanShelves();
  // The shelf a book sits on: the first folder below SHELF_ROOT, or its parent folder.
  static std::string shelfForPath(const std::string& path);

 private:
  static void screenFn(UiScreen& screen, void* user);
  static void onAction(const freeink::ui::ActionEvent& event, void* user);

  void draw(UiScreen& screen);
  void drawTopBar(UiScreen& screen, freeink::ui::Rect rect);
  void drawCurrent(UiScreen& screen, freeink::ui::Rect rect);
  void drawCovers(UiScreen& screen, freeink::ui::Rect rect);
  void drawShelves(UiScreen& screen, freeink::ui::Rect rect);
  void drawBottomBar(UiScreen& screen, freeink::ui::Rect rect);
  void drawEmptyBook(UiScreen& screen, freeink::ui::Rect rect);
  void paintCover(freeink::ui::DrawTarget& target, freeink::ui::Rect rect, size_t index);
  void noteThumbHeight(size_t index, int height);
  void refreshCoverPath(size_t index);

  // Button focus, as rows of action values.
  struct FocusRow {
    std::array<int, 3> items{};
    int count = 0;
  };
  int buildRows(std::array<FocusRow, 8>& rows) const;
  void moveFocus(int rowDelta, int colDelta);
  int shelfPageCount() const;

  HomeCoverCache coverCache;
  GfxRenderer& renderer;
  const std::vector<RecentBook>* books = nullptr;
  CurrentBook current;
  std::vector<std::string> shelves;
  std::array<std::string, MAX_BOOKS> coverPaths;
  std::array<int, MAX_BOOKS> thumbHeights{};
  bool thumbSizesChanged = false;
  int shelfPage = 0;
  int focus = 0;  // an action value; button devices only
  int pending = -1;
};
