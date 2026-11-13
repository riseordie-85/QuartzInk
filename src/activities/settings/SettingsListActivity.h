#pragma once

#include <array>

#include "SettingsSection.h"
#include "activities/UiListActivity.h"

// Settings root: one row per SettingsSection, each opening a
// SettingsActivity with that group's settings.
class SettingsListActivity final : public UiListActivity {
  // Section opened on the first loop pass (End = none), e.g. returning to
  // Network after the Wi-Fi rows' silent restart.
  SettingsSection pendingSection;

  // QuartzInk: a Wallpapers row sits between Display and Reader.
  static constexpr int WALLPAPERS_ROW = static_cast<int>(SettingsSection::Reader);
  static constexpr int ROW_COUNT = static_cast<int>(SETTINGS_SECTION_COUNT) + 1;
  std::array<freeink::ui::ListItem, ROW_COUNT> rowItems_{};
  static int rowForSection(SettingsSection section);

  void rebuildRows();
  void openSection(SettingsSection section);
  void openWallpapers();

  int listCount() const override { return ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  void drawChrome() override;

 public:
  explicit SettingsListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                SettingsSection openSection = SettingsSection::End);
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
