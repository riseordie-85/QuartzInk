#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>
#include <I18n.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include "activities/Activity.h"

// QuartzInk's in-book menu (Quartz theme): a top card with the book and quick
// actions (Home, Look up, Contents, Bookmark, Page settings), a book timeline
// to drag through, and a "Page settings" sheet with the everyday text settings
// (size, font, spacing, margins, alignment). Advanced opens CrossPoint's full
// reader menu, which holds everything else.
//
// Settings are edited on a local copy and written to SETTINGS only when the
// menu closes, so the page below never sees a half-applied change. CrossPoint
// keeps one set of reader settings for every book.
class QuartzReaderMenuActivity final : public Activity {
 public:
  // What the reader does after the menu closes (MenuResult::action).
  enum class Action : int8_t {
    None = -1,
    Home,
    Contents,
    ToggleBookmark,
    Bookmarks,
    Lookup,
    FullMenu,   // Advanced: CrossPoint's full reader menu
    MoreFonts,  // Font row's last cell: Text Settings on the Font tab
    Seek,       // MenuResult::value = target in permille of the book
  };

  struct BookInfo {
    std::string title;
    std::string chapter;
    uint32_t chapterPage = 0;
    uint32_t chapterPageCount = 0;
    float bookProgressPercent = 0.0f;
    bool pageBookmarked = false;
  };

  QuartzReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, BookInfo book);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }

 private:
  // The reader settings this sheet edits.
  struct QuickSettings {
    uint8_t fontFamily = 0;
    char sdFontFamilyName[32] = {};
    uint8_t pointSize = 0;
    uint8_t lineSpacing = 0;
    uint8_t wordSpacing = 0;
    uint8_t margin = 0;
    uint8_t alignment = 0;
  };

  enum class Row : uint8_t { Toolbar, Size, Font, Spacing, Margins, Align, Advanced, Count };
  enum class Tool : uint8_t { Home, Lookup, Contents, Bookmark, PageSettings };
  static constexpr uint8_t TOOL_COUNT = 5;

  using UiApp = freeink::ui::FreeInkApp<40, 8>;
  static constexpr freeink::ui::ActionId ACTION_DISMISS = 1;
  static constexpr freeink::ui::ActionId ACTION_PANEL = 2;  // swallows taps on blank card space
  static constexpr freeink::ui::ActionId ACTION_TOOL = 3;
  static constexpr freeink::ui::ActionId ACTION_OPTION = 4;  // value = row * 8 + option
  static constexpr freeink::ui::ActionId ACTION_ADVANCED = 5;
  static constexpr freeink::ui::ActionId ACTION_DONE = 6;
  static constexpr freeink::ui::ActionId ACTION_SEEK = 7;  // timeline drag

  static constexpr size_t MAX_FONT_SIZES = 24;
  // Button devices: holding Confirm this long on the bookmark tool opens the list.
  static constexpr unsigned long BOOKMARK_LIST_HOLD_MS = 600;
  static constexpr unsigned long SEEK_RENDER_INTERVAL_MS = 350;

  BookInfo book;
  QuickSettings initial{};
  QuickSettings current{};
  std::array<uint8_t, MAX_FONT_SIZES> fontSizes{};
  uint8_t fontSizeCount = 0;
  bool sheetOpen = false;
  bool buttonDevice = false;
  Row focusRow = Row::Size;
  uint8_t focusTool = 0;

  // Timeline scrubbing: the thumb follows the finger (0-1000) and the book jumps
  // there on release. -1 when not scrubbing.
  int16_t seekPermille = -1;
  int16_t pendingSeekPermille = -1;
  unsigned long lastSeekRenderMs = 0;
  bool touchReleasing = false;

  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};

  static QuickSettings captureSettings();
  void applySettings() const;
  bool settingsChanged() const;
  void loadFontSizes();
  bool changeFontSize(bool larger);
  uint8_t builtinFontCount() const;
  uint8_t optionCount(Row row) const;
  int selectedOption(Row row) const;
  void chooseOption(Row row, uint8_t option);
  void stepOption(Row row, bool forward);
  Tool toolAt(uint8_t index) const;
  void activateTool(Tool tool);
  void closeMenu(Action action, int16_t value = -1);

  static void onDismiss(const freeink::ui::ActionEvent& event, void* user);
  static void onPanel(const freeink::ui::ActionEvent& event, void* user);
  static void onTool(const freeink::ui::ActionEvent& event, void* user);
  static void onOption(const freeink::ui::ActionEvent& event, void* user);
  static void onAdvanced(const freeink::ui::ActionEvent& event, void* user);
  static void onDone(const freeink::ui::ActionEvent& event, void* user);
  static void onSeek(const freeink::ui::ActionEvent& event, void* user);

  static void menuScreen(UiApp::ScreenType& screen, void* user);
  void buildScreen(UiApp::ScreenType& screen);
  int16_t drawTopBar(UiApp::ScreenType& screen, freeink::ui::Rect area);
  void drawTimeline(UiApp::ScreenType& screen, freeink::ui::Rect area);
  void drawSheet(UiApp::ScreenType& screen, freeink::ui::Rect area);
  void drawFooterLink(UiApp::ScreenType& screen, freeink::ui::Rect rect, const char* label, const char* hint,
                      bool focused, freeink::ui::ActionId action);
  void drawOptionRow(UiApp::ScreenType& screen, freeink::ui::Rect rowRect, Row row);
  void drawOptionCell(UiApp::ScreenType& screen, freeink::ui::Rect cell, Row row, uint8_t option, bool selected);
};
