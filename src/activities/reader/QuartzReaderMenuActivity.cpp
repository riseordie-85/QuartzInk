#include "QuartzReaderMenuActivity.h"

#include <FreeInkUIIcon.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <SdCardFontSystem.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "CrossPointSettings.h"
#include "EpubReaderMenuModel.h"
#include "MappedInputManager.h"
#include "ReaderFontSizeStep.h"
#include "activities/ActivityResult.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/listIcons.h"

namespace fui = freeink::ui;

namespace {
// Layout, in screen pixels. These follow the Quartz mockup: rounded white
// cards with a 2px black border, options as boxed cells, the selected option
// filled black.
constexpr int16_t EDGE = 12;
constexpr int16_t CARD_RADIUS = 16;
constexpr int16_t CARD_BORDER = 2;
constexpr int16_t CARD_PAD = 14;
constexpr int16_t TOOL_HEIGHT = 56;
constexpr int16_t CELL_RADIUS = 10;
constexpr int16_t CELL_GAP = 8;
constexpr int16_t ROW_GAP = 12;
constexpr int16_t FOCUS_BORDER = 3;

// Quick presets. Anything set to another value in Advanced simply shows no
// selected cell here until the user picks one of these again.
constexpr uint8_t MARGIN_PRESETS[] = {5, 18, 36};
constexpr uint8_t ALIGN_PRESETS[] = {CrossPointSettings::JUSTIFIED, CrossPointSettings::LEFT_ALIGN,
                                     CrossPointSettings::BOOK_STYLE};
constexpr uint8_t SPACING_LEGACY[] = {CrossPointSettings::TIGHT, CrossPointSettings::NORMAL, CrossPointSettings::WIDE};
// Extra word spacing (0-4) that goes with each line-spacing preset. Picking a
// preset sets both, so a leftover Word Spacing value from Advanced cannot leave
// wide gaps between words once the text is no longer justified.
constexpr uint8_t SPACING_WORD[] = {0, 0, 1};

const fui::Paint BLACK = fui::Paint::solid(fui::Color::Black);
const fui::Paint WHITE = fui::Paint::solid(fui::Color::White);

fui::TextStyle textStyle(const fui::FontId font, const fui::TextAlign align, const bool inverted = false,
                         const bool bold = false) {
  fui::TextStyle style;
  style.font = font;
  style.align = align;
  style.color = inverted ? fui::Color::White : fui::Color::Black;
  style.bold = bold;
  return style;
}

void card(fui::DrawTarget& target, const fui::Rect rect) {
  target.fill(rect, WHITE, CARD_RADIUS);
  target.stroke(rect, BLACK, CARD_BORDER, CARD_RADIUS);
}

fui::Rect insetRect(const fui::Rect rect, const int16_t dx, const int16_t dy) {
  return fui::Rect{static_cast<int16_t>(rect.x + dx), static_cast<int16_t>(rect.y + dy),
                   static_cast<int16_t>(std::max<int>(0, rect.width - 2 * dx)),
                   static_cast<int16_t>(std::max<int>(0, rect.height - 2 * dy))};
}
}  // namespace

QuartzReaderMenuActivity::QuartzReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                   BookInfo book, SaveSettingsCallback saveSettings,
                                                   void* saveSettingsContext, SaveSettingsCallback applyToAllBooks)
    : Activity("QuartzReaderMenu", renderer, mappedInput),
      book(std::move(book)),
      saveSettings(saveSettings),
      saveSettingsContext(saveSettingsContext),
      applyToAllBooks(applyToAllBooks),
      initial(captureSettings()),
      current(initial),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void QuartzReaderMenuActivity::onEnter() {
  Activity::onEnter();
  buttonDevice = !mappedInput.hasTouchHardware();
  // Touch opens on the reading chrome and Aa opens the sheet. Button devices go
  // straight to the sheet, with the toolbar reachable as its first row.
  sheetOpen = buttonDevice;
  focusRow = Row::Size;
  focusTool = static_cast<uint8_t>(toolCount() - 1);
  loadFontSizes();
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_DISMISS, &QuartzReaderMenuActivity::onDismiss, this);
  app.on(ACTION_PANEL, &QuartzReaderMenuActivity::onPanel, this);
  app.on(ACTION_TOOL, &QuartzReaderMenuActivity::onTool, this);
  app.on(ACTION_OPTION, &QuartzReaderMenuActivity::onOption, this);
  app.on(ACTION_ADVANCED, &QuartzReaderMenuActivity::onAdvanced, this);
  app.on(ACTION_DONE, &QuartzReaderMenuActivity::onDone, this);
  app.on(ACTION_SEEK, &QuartzReaderMenuActivity::onSeek, this);
  app.on(ACTION_ADV, &QuartzReaderMenuActivity::onAdv, this);
  app.on(ACTION_ADV_BACK, &QuartzReaderMenuActivity::onAdvBack, this);
  app.on(ACTION_APPLY_ALL, &QuartzReaderMenuActivity::onApplyAll, this);
  if (app.handlerOverflowed()) LOG_ERR("QZM", "Quartz menu handler table overflowed");
  app.setScreen(&QuartzReaderMenuActivity::menuScreen, this);
  requestUpdate();
}

// ---------------------------------------------------------------------------
// Settings

QuartzReaderMenuActivity::QuickSettings QuartzReaderMenuActivity::captureSettings() {
  QuickSettings value;
  value.fontFamily = SETTINGS.fontFamily;
  std::strncpy(value.sdFontFamilyName, SETTINGS.sdFontFamilyName, sizeof(value.sdFontFamilyName) - 1);
  value.pointSize = SETTINGS.readerFontPointSize;
  value.lineHeightPercent = SETTINGS.lineHeightPercent;
  value.wordSpacing = SETTINGS.wordSpacing;
  value.marginHorizontal = SETTINGS.screenMarginHorizontal;
  value.alignment = SETTINGS.paragraphAlignment;
  value.marginVertical = SETTINGS.screenMarginVertical;
  value.orientation = SETTINGS.orientation;
  value.textAntiAliasing = SETTINGS.textAntiAliasing;
  value.focusReading = SETTINGS.focusReadingEnabled;
  value.guideReading = SETTINGS.guideReadingEnabled;
  value.hyphenation = SETTINGS.hyphenationEnabled;
  value.publisherPages = SETTINGS.publisherPageNumbers;
  value.extraSpacing = SETTINGS.extraParagraphSpacing;
  value.forceIndents = SETTINGS.forceParagraphIndents;
  value.embeddedStyle = SETTINGS.embeddedStyle;
  value.imageRendering = SETTINGS.imageRendering;
  return value;
}

void QuartzReaderMenuActivity::applySettings() const {
  SETTINGS.fontFamily = current.fontFamily;
  std::strncpy(SETTINGS.sdFontFamilyName, current.sdFontFamilyName, sizeof(SETTINGS.sdFontFamilyName) - 1);
  SETTINGS.sdFontFamilyName[sizeof(SETTINGS.sdFontFamilyName) - 1] = '\0';
  SETTINGS.readerFontPointSize = current.pointSize;
  SETTINGS.lineHeightPercent = current.lineHeightPercent;
  SETTINGS.wordSpacing = current.wordSpacing;
  SETTINGS.screenMarginHorizontal = current.marginHorizontal;
  SETTINGS.paragraphAlignment = current.alignment;
  SETTINGS.screenMarginVertical = current.marginVertical;
  SETTINGS.orientation = current.orientation;
  SETTINGS.textAntiAliasing = current.textAntiAliasing;
  SETTINGS.focusReadingEnabled = current.focusReading;
  SETTINGS.guideReadingEnabled = current.guideReading;
  SETTINGS.hyphenationEnabled = current.hyphenation;
  SETTINGS.publisherPageNumbers = current.publisherPages;
  SETTINGS.extraParagraphSpacing = current.extraSpacing;
  SETTINGS.forceParagraphIndents = current.forceIndents;
  SETTINGS.embeddedStyle = current.embeddedStyle;
  SETTINGS.imageRendering = current.imageRendering;
}

bool QuartzReaderMenuActivity::settingsChanged() const {
  const QuickSettings& a = current;
  const QuickSettings& b = initial;
  return a.fontFamily != b.fontFamily || std::strcmp(a.sdFontFamilyName, b.sdFontFamilyName) != 0 ||
         a.pointSize != b.pointSize || a.lineHeightPercent != b.lineHeightPercent || a.wordSpacing != b.wordSpacing ||
         a.marginHorizontal != b.marginHorizontal || a.alignment != b.alignment ||
         a.marginVertical != b.marginVertical || a.orientation != b.orientation ||
         a.textAntiAliasing != b.textAntiAliasing || a.focusReading != b.focusReading ||
         a.guideReading != b.guideReading || a.hyphenation != b.hyphenation || a.publisherPages != b.publisherPages ||
         a.extraSpacing != b.extraSpacing || a.forceIndents != b.forceIndents || a.embeddedStyle != b.embeddedStyle ||
         a.imageRendering != b.imageRendering;
}

void QuartzReaderMenuActivity::loadFontSizes() {
  // A fixed array: at most a couple dozen sizes, so no heap allocation is held
  // for the menu's lifetime.
  fontSizeCount = 0;
  if (current.sdFontFamilyName[0] != '\0') {
    RenderLock lock;
    sdFontSystem.refreshIfDirty();
    if (const auto* family = sdFontSystem.registry().findFamily(current.sdFontFamilyName)) {
      for (const uint8_t size : family->availableSizes()) {
        if (fontSizeCount >= MAX_FONT_SIZES) break;
        fontSizes[fontSizeCount++] = size;
      }
    }
  }
  if (fontSizeCount == 0) {
    for (const uint8_t size : BUILTIN_READER_FONT_SIZES) {
      if (fontSizeCount >= MAX_FONT_SIZES) break;
      fontSizes[fontSizeCount++] = size;
    }
  }
}

bool QuartzReaderMenuActivity::changeFontSize(const bool larger) {
  return changeReaderFontSizeStep(fontSizes.data(), fontSizeCount, current.pointSize, larger, FontSizeStepMode::Clamp);
}

uint8_t QuartzReaderMenuActivity::spacingPreset(const uint8_t option) const {
  return CrossPointSettings::legacyLineSpacingToPercent(SPACING_LEGACY[option], current.fontFamily,
                                                        current.sdFontFamilyName[0] != '\0');
}

uint8_t QuartzReaderMenuActivity::optionCount(const Row row) const {
  switch (row) {
    case Row::Size:
    case Row::Font:
    case Row::Spacing:
    case Row::Margins:
    case Row::Align:
      return 3;
    case Row::Toolbar:
      return toolCount();
    default:
      return 0;
  }
}

int QuartzReaderMenuActivity::selectedOption(const Row row) const {
  switch (row) {
    case Row::Font:
      if (current.sdFontFamilyName[0] != '\0') return 2;
      return current.fontFamily == CrossPointSettings::BITTER ? 1 : 0;
    case Row::Spacing:
      for (uint8_t i = 0; i < 3; ++i) {
        if (spacingPreset(i) == current.lineHeightPercent && SPACING_WORD[i] == current.wordSpacing) return i;
      }
      return -1;
    case Row::Margins:
      for (uint8_t i = 0; i < 3; ++i) {
        if (MARGIN_PRESETS[i] == current.marginHorizontal) return i;
      }
      return -1;
    case Row::Align:
      for (uint8_t i = 0; i < 3; ++i) {
        if (ALIGN_PRESETS[i] == current.alignment) return i;
      }
      return -1;
    default:
      return -1;
  }
}

void QuartzReaderMenuActivity::chooseOption(const Row row, const uint8_t option) {
  switch (row) {
    case Row::Size:
      // Size is a stepper: 0 = smaller, 2 = bigger. The middle cell is a readout.
      if (option == 0) changeFontSize(false);
      if (option == 2) changeFontSize(true);
      break;
    case Row::Font:
      if (option == 2) {
        // SD card fonts need the full picker (discovery, loading, previews).
        closeMenu(false, static_cast<int>(EpubReaderMenuAction::READER_OPTIONS));
        return;
      }
      current.fontFamily = option == 1 ? CrossPointSettings::BITTER : CrossPointSettings::LEXENDDECA;
      current.sdFontFamilyName[0] = '\0';
      loadFontSizes();
      current.pointSize = closestBuiltinReaderPointSize(current.pointSize);
      break;
    case Row::Spacing:
      current.lineHeightPercent = spacingPreset(option);
      current.wordSpacing = SPACING_WORD[option];
      break;
    case Row::Margins:
      current.marginHorizontal = MARGIN_PRESETS[option];
      break;
    case Row::Align:
      current.alignment = ALIGN_PRESETS[option];
      break;
    default:
      return;
  }
  requestUpdate();
}

// Button devices: Left/Right on a settings row moves its selection one step.
void QuartzReaderMenuActivity::stepOption(const Row row, const bool forward) {
  if (row == Row::Size) {
    if (changeFontSize(forward)) requestUpdate();
    return;
  }
  const uint8_t count = optionCount(row);
  if (count == 0) return;
  // The Font row's third option opens the full picker; keep Left/Right on the
  // two built-in faces so stepping never leaves the menu by surprise.
  const uint8_t steppable = row == Row::Font ? 2 : count;
  const int selected = selectedOption(row);
  int next;
  if (selected < 0 || selected >= steppable) {
    next = forward ? 0 : steppable - 1;
  } else {
    next = (selected + (forward ? 1 : steppable - 1)) % steppable;
  }
  chooseOption(row, static_cast<uint8_t>(next));
}

// ---------------------------------------------------------------------------
// Toolbar

uint8_t QuartzReaderMenuActivity::toolCount() const { return 4; }

QuartzReaderMenuActivity::Tool QuartzReaderMenuActivity::toolAt(const uint8_t index) const {
  // Look up sits where Home was: Home stays on the X4 Pro's Home key and swipe,
  // and on Back from the reader.
  static constexpr Tool TOOLS[] = {Tool::Lookup, Tool::Contents, Tool::Bookmark, Tool::PageSettings};
  return TOOLS[std::min<uint8_t>(index, 3)];
}

void QuartzReaderMenuActivity::activateTool(const Tool tool) {
  switch (tool) {
    case Tool::Home:
      closeMenu(false, static_cast<int>(EpubReaderMenuAction::GO_HOME));
      return;
    case Tool::Contents:
      closeMenu(false, static_cast<int>(EpubReaderMenuAction::SELECT_CHAPTER));
      return;
    case Tool::Bookmark:
      closeMenu(false, static_cast<int>(EpubReaderMenuAction::BOOKMARK_TOGGLE));
      return;
    case Tool::Lookup:
      // Without a dictionary for this book, Look up opens the dictionary picker
      // so there is always something to choose instead of a dead button.
      closeMenu(false, static_cast<int>(book.hasDictionary ? EpubReaderMenuAction::LOOKUP
                                                           : EpubReaderMenuAction::SET_BOOK_DICTIONARY));
      return;
    case Tool::PageSettings:
      if (sheetOpen) {
        // Aa again closes the menu, like Done.
        closeMenu(true, -1);
        return;
      }
      sheetOpen = true;
      requestUpdate();
      return;
  }
}

// ---------------------------------------------------------------------------
// Closing

void QuartzReaderMenuActivity::closeMenu(const bool cancelled, const int action, const int16_t value,
                                         const bool forAllBooks) {
  const bool changed = settingsChanged();
  if (forAllBooks && applyToAllBooks) {
    // Even unchanged settings are pushed out: the point is to make every book
    // look like this one.
    applySettings();
    applyToAllBooks(saveSettingsContext);
  } else if (changed) {
    applySettings();
    if (saveSettings) {
      saveSettings(saveSettingsContext);
    } else if (!SETTINGS.saveToFile()) {
      LOG_ERR("QZM", "Failed to save reader settings");
    }
  }
  MenuResult menu;
  menu.action = cancelled ? -1 : action;
  menu.drawerValue = value;
  menu.orientation = SETTINGS.orientation;
  menu.settingsChanged = changed;
  menu.changeMask = changed ? (ReaderSettingsChangeMask::Relayout | ReaderSettingsChangeMask::Preview)
                            : ReaderSettingsChangeMask::None;
  ActivityResult result;
  result.isCancelled = cancelled;
  result.data = menu;
  setResult(std::move(result));
  finish();
}

// ---------------------------------------------------------------------------
// Input

void QuartzReaderMenuActivity::onDismiss(const fui::ActionEvent&, void* user) {
  static_cast<QuartzReaderMenuActivity*>(user)->closeMenu(true, -1);
}

void QuartzReaderMenuActivity::onPanel(const fui::ActionEvent&, void*) {}

void QuartzReaderMenuActivity::onTool(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<QuartzReaderMenuActivity*>(user);
  if (event.value < 0 || event.value >= self->toolCount()) return;
  const Tool tool = self->toolAt(static_cast<uint8_t>(event.value));
  if (tool == Tool::Bookmark && event.longPress) {
    // Hold the bookmark icon for the bookmark list (Quartz Reader's gesture).
    self->closeMenu(false, static_cast<int>(EpubReaderMenuAction::VIEW_BOOKMARKS));
    return;
  }
  self->activateTool(tool);
}

void QuartzReaderMenuActivity::onOption(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<QuartzReaderMenuActivity*>(user);
  const auto row = static_cast<Row>(event.value / 8);
  const auto option = static_cast<uint8_t>(event.value % 8);
  if (row <= Row::Toolbar || row >= Row::ApplyAll || option >= self->optionCount(row)) return;
  self->chooseOption(row, option);
}

void QuartzReaderMenuActivity::onAdvanced(const fui::ActionEvent&, void* user) {
  static_cast<QuartzReaderMenuActivity*>(user)->openAdvanced();
}

void QuartzReaderMenuActivity::onApplyAll(const fui::ActionEvent&, void* user) {
  static_cast<QuartzReaderMenuActivity*>(user)->closeMenu(true, -1, -1, true);
}

void QuartzReaderMenuActivity::onDone(const fui::ActionEvent&, void* user) {
  static_cast<QuartzReaderMenuActivity*>(user)->closeMenu(true, -1);
}

void QuartzReaderMenuActivity::onSeek(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<QuartzReaderMenuActivity*>(user);
  if (event.dragPermille < 0) return;
  if (self->touchReleasing) {
    // Centipercent, as the reader's percent jump expects (10000 = end of book).
    self->closeMenu(false, static_cast<int>(EpubReaderMenuAction::GO_TO_PERCENT),
                    static_cast<int16_t>(std::clamp<int>(event.dragPermille, 0, 1000) * 10));
    return;
  }
  self->pendingSeekPermille = event.dragPermille;
  // Each repaint is an e-ink refresh. Redraw the thumb only for a visible move
  // (about 1% of the book) and at most a few times a second, so the panel keeps
  // up with the finger instead of queueing refreshes behind it.
  const unsigned long now = millis();
  const bool firstMove = self->seekPermille < 0;
  if (firstMove || (std::abs(event.dragPermille - self->seekPermille) >= 10 &&
                    now - self->lastSeekRenderMs >= SEEK_RENDER_INTERVAL_MS)) {
    self->seekPermille = event.dragPermille;
    self->lastSeekRenderMs = now;
    self->requestUpdate();
  } else {
    // Keep the latest position for the jump even when the thumb is not redrawn.
    self->pendingSeekPermille = event.dragPermille;
  }
}

void QuartzReaderMenuActivity::loop() {
  if (uiReady) {
    fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    // Everything here acts on release. Holding only matters for the timeline
    // scrubber, which binds on the first held frame; the press edge is dropped
    // so a touch does not trigger an extra e-ink refresh for a pressed state.
    snap.touchPressed = false;
    // A horizontal drag is also classified as a swipe partway through, and the
    // snapshot reports that as a release. While the finger is still down it is
    // not one: ignoring it keeps the scrubber from jumping mid-drag.
    if (snap.touchHeld) snap.touchReleased = false;
    const bool scrubbing = !sheetOpen && snap.touchHeld;
    if (snap.touchReleased || scrubbing) {
      touchReleasing = snap.touchReleased;
      const auto event = app.route(snap);
      touchReleasing = false;
      if (app.invalidated()) requestUpdate();
      if (event) return;
      if (snap.touchReleased && seekPermille >= 0) {
        // Finger lifted without a routed release: jump to the last position.
        const int16_t target = pendingSeekPermille >= 0 ? pendingSeekPermille : seekPermille;
        closeMenu(false, static_cast<int>(EpubReaderMenuAction::GO_TO_PERCENT),
                  static_cast<int16_t>(std::clamp<int>(target, 0, 1000) * 10));
        return;
      }
    }
  }

  if (advancedOpen) {
    advancedLoop();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    closeMenu(true, -1);
    return;
  }

  if (!buttonDevice) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) closeMenu(true, -1);
    return;
  }

  const auto rowCount = static_cast<uint8_t>(Row::Count);
  const bool up = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                  mappedInput.wasReleased(MappedInputManager::Button::PageBack);
  const bool down = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                    mappedInput.wasReleased(MappedInputManager::Button::PageForward);
  if (up || down) {
    const uint8_t index = static_cast<uint8_t>(focusRow);
    focusRow = static_cast<Row>((index + (down ? 1 : rowCount - 1)) % rowCount);
    if (focusRow == Row::ApplyAll && !applyToAllBooks) {
      focusRow = static_cast<Row>((static_cast<uint8_t>(focusRow) + (down ? 1 : rowCount - 1)) % rowCount);
    }
    requestUpdate();
    return;
  }

  const bool left = mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool right = mappedInput.wasReleased(MappedInputManager::Button::Right);
  if (left || right) {
    if (focusRow == Row::Toolbar) {
      const uint8_t count = toolCount();
      focusTool = static_cast<uint8_t>((focusTool + (right ? 1 : count - 1)) % count);
      requestUpdate();
    } else if (focusRow < Row::ApplyAll) {
      stepOption(focusRow, right);
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (focusRow) {
      case Row::Toolbar:
        if (toolAt(focusTool) == Tool::Bookmark && mappedInput.getHeldTime() >= BOOKMARK_LIST_HOLD_MS) {
          closeMenu(false, static_cast<int>(EpubReaderMenuAction::VIEW_BOOKMARKS));
          return;
        }
        activateTool(toolAt(focusTool));
        return;
      case Row::ApplyAll:
        closeMenu(true, -1, -1, true);
        return;
      case Row::Advanced:
        openAdvanced();
        return;
      default:
        // Confirm walks down the rows, so a one-handed reader can reach every
        // setting with Confirm plus Left/Right.
        focusRow = static_cast<Row>((static_cast<uint8_t>(focusRow) + 1) % rowCount);
        requestUpdate();
        return;
    }
  }
}

// ---------------------------------------------------------------------------
// Drawing

void QuartzReaderMenuActivity::menuScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<QuartzReaderMenuActivity*>(user)->buildScreen(screen);
}

void QuartzReaderMenuActivity::buildScreen(UiApp::ScreenType& screen) {
  const int16_t width = static_cast<int16_t>(renderer.getScreenWidth());
  const int16_t height = static_cast<int16_t>(renderer.getScreenHeight());
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int16_t hintsHeight = buttonDevice ? static_cast<int16_t>(metrics.buttonHintsHeight) : 0;

  // Taps on the page around the cards close the menu.
  screen.frame().hit(fui::Rect{0, 0, width, height}, ACTION_DISMISS, 0);

  const fui::Rect area{EDGE, EDGE, static_cast<int16_t>(width - 2 * EDGE),
                       static_cast<int16_t>(height - 2 * EDGE - hintsHeight)};
  const int16_t topBottom = drawTopBar(screen, area);

  if (!sheetOpen) {
    drawTimeline(screen, area);
    return;
  }
  {
    const fui::Rect sheet{area.x, static_cast<int16_t>(topBottom + 10), area.width,
                          static_cast<int16_t>(area.bottom() - topBottom - 10)};
    if (advancedOpen) {
      drawAdvanced(screen, sheet);
    } else {
      drawSheet(screen, sheet);
    }
  }
}

int16_t QuartzReaderMenuActivity::drawTopBar(UiApp::ScreenType& screen, const fui::Rect area) {
  auto& target = screen.target();
  const int16_t titleH = target.lineHeight(fui::GfxRendererTarget::FONT_BODY);
  const int16_t chapterH = book.chapter.empty() ? 0 : target.lineHeight(fui::GfxRendererTarget::FONT_SMALL);
  const int16_t barH = static_cast<int16_t>(12 + titleH + chapterH + 6 + TOOL_HEIGHT + 6);
  const fui::Rect bar{area.x, area.y, area.width, barH};
  card(target, bar);
  screen.frame().hit(bar, ACTION_PANEL, 0);

  const int16_t textX = static_cast<int16_t>(bar.x + CARD_PAD + 6);
  const int16_t textW = static_cast<int16_t>(bar.width - 2 * (CARD_PAD + 6));
  int16_t y = static_cast<int16_t>(bar.y + 12);
  target.text(fui::Rect{textX, y, textW, titleH}, book.title.c_str(),
              textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Left, false, true));
  y = static_cast<int16_t>(y + titleH);
  if (chapterH > 0) {
    target.text(fui::Rect{textX, y, textW, chapterH}, book.chapter.c_str(),
                textStyle(fui::GfxRendererTarget::FONT_SMALL, fui::TextAlign::Left));
    y = static_cast<int16_t>(y + chapterH);
  }
  y = static_cast<int16_t>(y + 6);

  const uint8_t count = toolCount();
  const int16_t rowX = static_cast<int16_t>(bar.x + CARD_PAD);
  const int16_t rowW = static_cast<int16_t>(bar.width - 2 * CARD_PAD);
  const int16_t slotW = static_cast<int16_t>(rowW / count);
  const int16_t toolW = std::min<int16_t>(slotW - 6, 72);
  for (uint8_t i = 0; i < count; ++i) {
    const Tool tool = toolAt(i);
    const int16_t slotX = static_cast<int16_t>(rowX + i * slotW);
    const fui::Rect toolRect{static_cast<int16_t>(slotX + (slotW - toolW) / 2), y, toolW, TOOL_HEIGHT};
    const bool active = tool == Tool::PageSettings && sheetOpen;
    const bool focused = buttonDevice && focusRow == Row::Toolbar && focusTool == i;
    const bool filled = active || focused;
    if (filled) target.fill(toolRect, BLACK, 12);
    if (focused && !active) target.stroke(insetRect(toolRect, -4, -4), BLACK, FOCUS_BORDER, 15);
    screen.frame().hit(toolRect, ACTION_TOOL, i,
                       tool == Tool::Bookmark ? static_cast<uint16_t>(fui::InputDefault | fui::InputLongPress)
                                              : static_cast<uint16_t>(fui::InputDefault));

    const fui::Paint ink = filled ? WHITE : BLACK;
    fui::BitmapRef icon;
    switch (tool) {
      case Tool::Home:
        icon = fui::bitmapFromIcon(icon_home_32);
        break;
      case Tool::Contents:
        icon = fui::bitmapFromIcon(icon_menu_32);
        break;
      case Tool::Bookmark:
        icon = fui::bitmapFromIcon(icon_bookmark_32);
        break;
      case Tool::Lookup:
        icon = fui::bitmapFromIcon(icon_search_32);
        break;
      case Tool::PageSettings:
        icon = fui::bitmapFromIcon(icon_case_sensitive_32);
        break;
    }
    target.bitmap(toolRect, icon, fui::BitmapMode::Center, ink);
    if (tool == Tool::Bookmark && book.pageBookmarked) {
      // A small filled corner marks a bookmarked page (the icon set has no
      // filled bookmark at this size).
      const int16_t mark = 8;
      target.fill(fui::Rect{static_cast<int16_t>(toolRect.right() - mark - 6), static_cast<int16_t>(toolRect.y + 6),
                            mark, mark},
                  ink, 4);
    }
  }
  return bar.bottom();
}

void QuartzReaderMenuActivity::drawTimeline(UiApp::ScreenType& screen, const fui::Rect area) {
  auto& target = screen.target();
  const int16_t bodyH = target.lineHeight(fui::GfxRendererTarget::FONT_BODY);
  const int16_t smallH = target.lineHeight(fui::GfxRendererTarget::FONT_SMALL);
  const int16_t cardH = static_cast<int16_t>(14 + std::max<int16_t>(bodyH, 24) + 8 + smallH + 12);
  const fui::Rect bar{area.x, static_cast<int16_t>(area.bottom() - cardH), area.width, cardH};
  card(target, bar);
  screen.frame().hit(bar, ACTION_PANEL, 0);

  const int16_t innerX = static_cast<int16_t>(bar.x + 20);
  const int16_t innerW = static_cast<int16_t>(bar.width - 40);
  int16_t y = static_cast<int16_t>(bar.y + 14);

  char pageLabel[24];
  if (book.chapterPageCount > 0) {
    std::snprintf(pageLabel, sizeof(pageLabel), "%lu / %lu", static_cast<unsigned long>(book.chapterPage),
                  static_cast<unsigned long>(book.chapterPageCount));
  } else {
    pageLabel[0] = '\0';
  }
  const auto bodyStyle = textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Left);
  const int16_t labelW =
      pageLabel[0] ? static_cast<int16_t>(target.measureText(bodyStyle.font, pageLabel, bodyStyle).width + 14) : 0;
  const int16_t lineH = std::max<int16_t>(bodyH, 24);
  if (labelW > 0) target.text(fui::Rect{innerX, y, labelW, lineH}, pageLabel, bodyStyle);

  // Book progress track: a thin line, a thicker filled part, and a round thumb.
  const int16_t trackX = static_cast<int16_t>(innerX + labelW);
  const int16_t trackW = static_cast<int16_t>(std::max<int>(40, innerW - labelW));
  const int16_t mid = static_cast<int16_t>(y + lineH / 2);
  const float progress = seekPermille >= 0 ? static_cast<float>(seekPermille) / 1000.0f
                                           : std::clamp(book.bookProgressPercent, 0.0f, 100.0f) / 100.0f;
  const int16_t filled = static_cast<int16_t>(std::lround(progress * trackW));
  target.fill(fui::Rect{trackX, static_cast<int16_t>(mid - 1), trackW, 2}, BLACK);
  if (filled > 0) target.fill(fui::Rect{trackX, static_cast<int16_t>(mid - 3), filled, 6}, BLACK, 3);
  const int16_t thumb = 22;
  const fui::Rect thumbRect{static_cast<int16_t>(trackX + filled - thumb / 2), static_cast<int16_t>(mid - thumb / 2),
                            thumb, thumb};
  target.fill(thumbRect, WHITE, thumb / 2);
  target.stroke(thumbRect, BLACK, 3, thumb / 2);
  // Drag anywhere along the track; the hit band is taller than the line so it
  // is easy to grab.
  screen.frame().hit(fui::Rect{trackX, static_cast<int16_t>(mid - 22), trackW, 44}, ACTION_SEEK, 0,
                     fui::InputTouch | fui::InputDrag);
  y = static_cast<int16_t>(y + lineH + 8);

  char percentLabel[24];
  std::snprintf(percentLabel, sizeof(percentLabel), "%d%%", static_cast<int>(std::lround(progress * 100.0f)));
  target.text(fui::Rect{innerX, y, innerW, smallH}, book.chapter.empty() ? "" : book.chapter.c_str(),
              textStyle(fui::GfxRendererTarget::FONT_SMALL, fui::TextAlign::Left));
  target.text(fui::Rect{innerX, y, innerW, smallH}, percentLabel,
              textStyle(fui::GfxRendererTarget::FONT_SMALL, fui::TextAlign::Right));
}

void QuartzReaderMenuActivity::drawSheet(UiApp::ScreenType& screen, const fui::Rect area) {
  auto& target = screen.target();
  card(target, area);
  screen.frame().hit(area, ACTION_PANEL, 0);

  const fui::Rect inner = insetRect(area, 18, 16);
  const int16_t titleH = target.lineHeight(fui::GfxRendererTarget::FONT_TITLE);
  const int16_t headerH = std::max<int16_t>(titleH, 40);
  target.text(fui::Rect{inner.x, inner.y, static_cast<int16_t>(inner.width - 110), headerH}, tr(STR_QZ_PAGE_SETTINGS),
              textStyle(fui::GfxRendererTarget::FONT_TITLE, fui::TextAlign::Left, false, true));
  const fui::Rect done{static_cast<int16_t>(inner.right() - 96), inner.y, 96, headerH};
  target.fill(done, WHITE, static_cast<uint8_t>(headerH / 2));
  target.stroke(done, BLACK, 2, static_cast<uint8_t>(headerH / 2));
  target.text(done, tr(STR_DONE), textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Center, false, true));
  screen.frame().hit(done, ACTION_DONE, 0);

  // Five option rows share the space above the two footer links.
  const int16_t smallH = target.lineHeight(fui::GfxRendererTarget::FONT_SMALL);
  const int16_t linkH = static_cast<int16_t>(14 + target.lineHeight(fui::GfxRendererTarget::FONT_BODY) + smallH + 4);
  const int footerLinks = applyToAllBooks ? 2 : 1;
  const int16_t rowsTop = static_cast<int16_t>(inner.y + headerH + 14);
  const int16_t rowsBottom = static_cast<int16_t>(inner.bottom() - footerLinks * linkH - 8);
  constexpr int ROWS = 5;
  const int16_t rowH =
      static_cast<int16_t>(std::clamp<int>((rowsBottom - rowsTop - (ROWS - 1) * ROW_GAP) / ROWS, 40, 60));
  int16_t y = rowsTop;
  for (uint8_t r = static_cast<uint8_t>(Row::Size); r <= static_cast<uint8_t>(Row::Align); ++r) {
    drawOptionRow(screen, fui::Rect{inner.x, y, inner.width, rowH}, static_cast<Row>(r));
    y = static_cast<int16_t>(y + rowH + ROW_GAP);
  }

  // Footer: Apply to all books, then Advanced, each a full-width link under a rule.
  int16_t linkY = static_cast<int16_t>(inner.bottom() - footerLinks * linkH);
  if (applyToAllBooks) {
    drawFooterLink(screen, fui::Rect{inner.x, linkY, inner.width, linkH}, tr(STR_QZ_APPLY_ALL),
                   tr(STR_QZ_APPLY_ALL_HINT), buttonDevice && focusRow == Row::ApplyAll, ACTION_APPLY_ALL);
    linkY = static_cast<int16_t>(linkY + linkH);
  }
  drawFooterLink(screen, fui::Rect{inner.x, linkY, inner.width, linkH}, tr(STR_QZ_ADVANCED), tr(STR_QZ_ADVANCED_HINT),
                 buttonDevice && focusRow == Row::Advanced, ACTION_ADVANCED);

  if (buttonDevice) {
    const char* backLabel = tr(STR_DONE);
    const char* confirmLabel =
        focusRow == Row::Toolbar || focusRow >= Row::ApplyAll ? tr(STR_SELECT) : tr(STR_NEXT_FIELD);
    const char* prevLabel = focusRow == Row::Size ? tr(STR_QZ_SMALLER) : "<";
    const char* nextLabel = focusRow == Row::Size ? tr(STR_QZ_BIGGER) : ">";
    const auto labels = mappedInput.mapLabels(backLabel, confirmLabel, prevLabel, nextLabel);
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

void QuartzReaderMenuActivity::drawFooterLink(UiApp::ScreenType& screen, const fui::Rect rect, const char* label,
                                              const char* hint, const bool focused, const fui::ActionId action) {
  auto& target = screen.target();
  target.fill(fui::Rect{rect.x, rect.y, rect.width, 2}, BLACK);
  if (focused) target.stroke(insetRect(rect, -6, 0), BLACK, FOCUS_BORDER, 12);
  const int16_t bodyH = target.lineHeight(fui::GfxRendererTarget::FONT_BODY);
  const int16_t smallH = target.lineHeight(fui::GfxRendererTarget::FONT_SMALL);
  const int16_t textW = static_cast<int16_t>(rect.width - 40);
  target.text(fui::Rect{static_cast<int16_t>(rect.x + 4), static_cast<int16_t>(rect.y + 12), textW, bodyH}, label,
              textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Left, false, true));
  target.text(fui::Rect{static_cast<int16_t>(rect.x + 4), static_cast<int16_t>(rect.y + 12 + bodyH), textW, smallH},
              hint, textStyle(fui::GfxRendererTarget::FONT_SMALL, fui::TextAlign::Left));
  const int16_t chevronX = static_cast<int16_t>(rect.right() - 18);
  const int16_t chevronY = static_cast<int16_t>(rect.y + rect.height / 2 + 1);
  target.line(fui::Point{static_cast<int16_t>(chevronX - 6), static_cast<int16_t>(chevronY - 8)},
              fui::Point{chevronX, chevronY}, 3, BLACK);
  target.line(fui::Point{chevronX, chevronY},
              fui::Point{static_cast<int16_t>(chevronX - 6), static_cast<int16_t>(chevronY + 8)}, 3, BLACK);
  screen.frame().hit(rect, action, 0);
}

void QuartzReaderMenuActivity::drawOptionRow(UiApp::ScreenType& screen, const fui::Rect rowRect, const Row row) {
  auto& target = screen.target();
  const bool focused = buttonDevice && focusRow == row;
  if (focused) target.stroke(insetRect(rowRect, -8, -5), BLACK, FOCUS_BORDER, 14);

  StrId label = StrId::STR_QZ_SIZE;
  switch (row) {
    case Row::Font:
      label = StrId::STR_QZ_FONT;
      break;
    case Row::Spacing:
      label = StrId::STR_QZ_SPACING;
      break;
    case Row::Margins:
      label = StrId::STR_QZ_MARGINS;
      break;
    case Row::Align:
      label = StrId::STR_QZ_ALIGN;
      break;
    default:
      break;
  }
  const int16_t labelW = static_cast<int16_t>(std::max<int>(76, rowRect.width / 5));
  target.text(fui::Rect{rowRect.x, rowRect.y, labelW, rowRect.height}, I18N.get(label),
              textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Left, false, true));

  const uint8_t count = optionCount(row);
  const int16_t cellsX = static_cast<int16_t>(rowRect.x + labelW + 8);
  const int16_t cellsW = static_cast<int16_t>(rowRect.right() - cellsX);
  const int16_t cellW = static_cast<int16_t>((cellsW - (count - 1) * CELL_GAP) / count);
  const int selected = selectedOption(row);
  for (uint8_t i = 0; i < count; ++i) {
    const fui::Rect cell{static_cast<int16_t>(cellsX + i * (cellW + CELL_GAP)), rowRect.y, cellW, rowRect.height};
    drawOptionCell(screen, cell, row, i, selected == i);
  }
}

void QuartzReaderMenuActivity::drawOptionCell(UiApp::ScreenType& screen, const fui::Rect cell, const Row row,
                                              const uint8_t option, const bool selected) {
  auto& target = screen.target();
  const fui::Paint ink = selected ? WHITE : BLACK;

  // The Size row's middle cell is a readout, not a button.
  if (row == Row::Size && option == 1) {
    char size[12];
    std::snprintf(size, sizeof(size), "%u pt", static_cast<unsigned>(current.pointSize));
    target.text(cell, size, textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Center));
    return;
  }

  target.fill(cell, selected ? BLACK : WHITE, CELL_RADIUS);
  target.stroke(cell, BLACK, 2, CELL_RADIUS);
  screen.frame().hit(cell, ACTION_OPTION, static_cast<int16_t>(static_cast<uint8_t>(row) * 8 + option));

  const int16_t smallH = target.lineHeight(fui::GfxRendererTarget::FONT_SMALL);
  const auto small = textStyle(fui::GfxRendererTarget::FONT_SMALL, fui::TextAlign::Center, selected);
  switch (row) {
    case Row::Size: {
      const auto font = option == 0 ? fui::GfxRendererTarget::FONT_SMALL : fui::GfxRendererTarget::FONT_TITLE;
      target.text(cell, "A", textStyle(font, fui::TextAlign::Center, selected, true));
      return;
    }
    case Row::Font: {
      const char* names[] = {"Lexend", "Bitter", tr(STR_QZ_MORE_FONTS)};
      target.text(cell, names[option], textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Center, selected));
      return;
    }
    default:
      break;
  }

  // Spacing, Margins and Align: a small line drawing over a label.
  const int16_t iconW = 30;
  const int16_t iconH = 20;
  const bool roomForLabel = cell.height >= iconH + smallH + 8;
  const int16_t contentH = static_cast<int16_t>(iconH + (roomForLabel ? smallH + 2 : 0));
  const int16_t iconX = static_cast<int16_t>(cell.x + (cell.width - iconW) / 2);
  const int16_t iconY = static_cast<int16_t>(cell.y + (cell.height - contentH) / 2);
  const int16_t midY = static_cast<int16_t>(iconY + iconH / 2);
  const auto hline = [&](int16_t x1, int16_t x2, int16_t yy) {
    target.line(fui::Point{static_cast<int16_t>(iconX + x1), yy}, fui::Point{static_cast<int16_t>(iconX + x2), yy}, 2,
                ink);
  };

  StrId label = StrId::STR_QZ_NORMAL;
  if (row == Row::Spacing) {
    static constexpr int16_t GAPS[] = {4, 6, 9};
    hline(3, 27, static_cast<int16_t>(midY - GAPS[option]));
    hline(3, 27, midY);
    hline(3, 27, static_cast<int16_t>(midY + GAPS[option]));
    static constexpr StrId LABELS[] = {StrId::STR_TIGHT, StrId::STR_QZ_NORMAL, StrId::STR_QZ_LOOSE};
    label = LABELS[option];
  } else if (row == Row::Margins) {
    static constexpr int16_t INSETS[] = {7, 10, 13};
    target.line(fui::Point{static_cast<int16_t>(iconX + 2), static_cast<int16_t>(midY - 9)},
                fui::Point{static_cast<int16_t>(iconX + 2), static_cast<int16_t>(midY + 9)}, 2, ink);
    target.line(fui::Point{static_cast<int16_t>(iconX + 28), static_cast<int16_t>(midY - 9)},
                fui::Point{static_cast<int16_t>(iconX + 28), static_cast<int16_t>(midY + 9)}, 2, ink);
    for (const int16_t dy : {-5, 0, 5}) {
      hline(INSETS[option], static_cast<int16_t>(30 - INSETS[option]), static_cast<int16_t>(midY + dy));
    }
    static constexpr StrId LABELS[] = {StrId::STR_QZ_NARROW, StrId::STR_QZ_NORMAL, StrId::STR_WIDE};
    label = LABELS[option];
  } else if (row == Row::Align) {
    static constexpr int16_t FIRST_START[] = {3, 3, 9};
    static constexpr int16_t LAST_END[] = {27, 18, 20};
    hline(FIRST_START[option], 27, static_cast<int16_t>(midY - 6));
    hline(3, 27, midY);
    hline(3, LAST_END[option], static_cast<int16_t>(midY + 6));
    static constexpr StrId LABELS[] = {StrId::STR_JUSTIFY, StrId::STR_ALIGN_LEFT, StrId::STR_QZ_BOOK};
    label = LABELS[option];
  }
  if (roomForLabel) {
    target.text(fui::Rect{cell.x, static_cast<int16_t>(iconY + iconH + 2), cell.width, smallH}, I18N.get(label), small);
  }
}

void QuartzReaderMenuActivity::render(RenderLock&&) {
  uiReady = false;
  app.setDevice(uiTarget.deviceContext());
  app.render();
  uiReady = true;
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
