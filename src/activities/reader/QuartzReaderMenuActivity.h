#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include "activities/Activity.h"

// QuartzInk's in-book menu: a title bar with quick actions, a page timeline, and
// a "Page settings" sheet holding the everyday reading settings (size, font,
// spacing, margins, alignment). Everything else stays one step away under
// Advanced, which reopens CrossInk's full reader drawer.
//
// Settings are edited on a small local copy and written to SETTINGS only when
// the menu closes, so the reader below never sees a half-applied change. The
// reader's existing per-book save callback then records them for this book.
class QuartzReaderMenuActivity final : public Activity {
 public:
  using SaveSettingsCallback = void (*)(void* ctx);

  struct BookInfo {
    std::string title;
    std::string chapter;
    uint32_t chapterPage = 0;
    uint32_t chapterPageCount = 0;
    float bookProgressPercent = 0.0f;
    bool pageBookmarked = false;
    bool hasDictionary = false;
  };

  QuartzReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, BookInfo book,
                           SaveSettingsCallback saveSettings, void* saveSettingsContext);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool requiresFreshBackdrop() const override { return true; }
  bool allowPowerAsConfirmInReaderMode() const override { return true; }

 private:
  // The few reader settings this sheet edits.
  struct QuickSettings {
    uint8_t fontFamily = 0;
    char sdFontFamilyName[64] = {};
    uint8_t pointSize = 0;
    uint8_t lineHeightPercent = 0;
    uint8_t wordSpacing = 0;
    uint8_t marginHorizontal = 0;
    uint8_t alignment = 0;
  };

  enum class Row : uint8_t { Toolbar, Size, Font, Spacing, Margins, Align, Advanced, Count };
  enum class Tool : uint8_t { Home, Contents, Bookmark, Lookup, PageSettings };

  using UiApp = freeink::ui::FreeInkApp<40, 8>;  // 7 actions below; keep MaxHandlers >= that count
  static constexpr freeink::ui::ActionId ACTION_DISMISS = 1;
  static constexpr freeink::ui::ActionId ACTION_PANEL = 2;  // swallows taps on blank panel space
  static constexpr freeink::ui::ActionId ACTION_TOOL = 3;
  static constexpr freeink::ui::ActionId ACTION_OPTION = 4;  // value = row * 8 + option
  static constexpr freeink::ui::ActionId ACTION_ADVANCED = 5;
  static constexpr freeink::ui::ActionId ACTION_DONE = 6;
  static constexpr freeink::ui::ActionId ACTION_SEEK = 7;  // timeline drag; dragPermille = position
  static constexpr size_t MAX_FONT_SIZES = 24;

  BookInfo book;
  SaveSettingsCallback saveSettings = nullptr;
  void* saveSettingsContext = nullptr;
  QuickSettings initial{};
  QuickSettings current{};
  std::array<uint8_t, MAX_FONT_SIZES> fontSizes{};
  uint8_t fontSizeCount = 0;

  bool sheetOpen = false;
  // Timeline scrubbing: the thumb follows the finger (0-1000) and the book jumps
  // there on release. -1 when not scrubbing.
  int16_t seekPermille = -1;         // where the thumb is drawn
  int16_t pendingSeekPermille = -1;  // latest finger position, drawn or not
  unsigned long lastSeekRenderMs = 0;
  static constexpr unsigned long SEEK_RENDER_INTERVAL_MS = 250;
  bool touchReleasing = false;
  bool buttonDevice = false;
  Row focusRow = Row::Size;
  uint8_t focusTool = static_cast<uint8_t>(Tool::PageSettings);

  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};

  static void menuScreen(UiApp::ScreenType& screen, void* user);
  static void onDismiss(const freeink::ui::ActionEvent& event, void* user);
  static void onPanel(const freeink::ui::ActionEvent& event, void* user);
  static void onTool(const freeink::ui::ActionEvent& event, void* user);
  static void onOption(const freeink::ui::ActionEvent& event, void* user);
  static void onAdvanced(const freeink::ui::ActionEvent& event, void* user);
  static void onDone(const freeink::ui::ActionEvent& event, void* user);
  static void onSeek(const freeink::ui::ActionEvent& event, void* user);

  void buildScreen(UiApp::ScreenType& screen);
  int16_t drawTopBar(UiApp::ScreenType& screen, freeink::ui::Rect area);
  void drawTimeline(UiApp::ScreenType& screen, freeink::ui::Rect area);
  void drawSheet(UiApp::ScreenType& screen, freeink::ui::Rect area);
  void drawOptionRow(UiApp::ScreenType& screen, freeink::ui::Rect rowRect, Row row);
  void drawOptionCell(UiApp::ScreenType& screen, freeink::ui::Rect cell, Row row, uint8_t option, bool selected);

  uint8_t toolCount() const;
  Tool toolAt(uint8_t index) const;
  void activateTool(Tool tool);
  void chooseOption(Row row, uint8_t option);
  void stepOption(Row row, bool forward);
  int selectedOption(Row row) const;
  uint8_t optionCount(Row row) const;
  bool changeFontSize(bool larger);
  uint8_t spacingPreset(uint8_t option) const;

  void loadFontSizes();
  static QuickSettings captureSettings();
  void applySettings() const;
  bool settingsChanged() const;
  void closeMenu(bool cancelled, int action, int16_t value = -1);
};
