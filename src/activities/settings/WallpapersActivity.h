#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/UiTabListActivity.h"
#include "util/SleepImages.h"

// QuartzInk Settings > Wallpapers: the sleep images on the card, as on the
// web Wallpapers page. Tabs split wallpapers (/.sleep) from overlays
// (/.sleep-overlay). Each tab lists the sleep mode, Shuffle (no pin: a random
// image each time) and the images; opening one shows it in the image viewer,
// whose Confirm pins it.
class WallpapersActivity final : public UiTabListActivity {
 public:
  WallpapersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  enum class Tab : uint8_t { Wallpapers, Overlays, Count };
  // Rows above the images.
  enum FixedRow : int { ROW_MODE = 0, ROW_SHUFFLE = 1, FIXED_ROWS = 2 };
  // Images listed per tab; a card with more still works, it just lists these.
  static constexpr size_t MAX_IMAGES = 150;

  int listCount() const override;
  int tabCount() const override { return static_cast<int>(Tab::Count); }
  int activeTab() const override { return static_cast<int>(tab_); }
  const char* tabLabel(int index) const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onTabAction(int index) override;
  void stepTab(int direction) override { switchTab(direction); }
  void drawChrome() override;
  void drawFooter() override;
  bool handleButtons() override;

  sleepimages::Kind kind() const;
  void switchTab(int direction);
  void loadImages();
  void rebuildRowItems();
  void activateRow(int row);
  void cycleMode();
  void openImage(int imageIndex);
  const char* confirmLabelText() const;

  Tab tab_ = Tab::Wallpapers;
  std::vector<std::string> images_;
  std::vector<std::string> names_;  // file names shown in the rows
  std::vector<freeink::ui::ListItem> rowItems_;
};
