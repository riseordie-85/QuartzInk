// The Advanced page of QuartzReaderMenuActivity: the less common reader
// settings as one scrolling list of steppers, switches and tap-to-change values,
// plus a link to CrossInk's full reader menu for everything else.

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "EpubReaderMenuModel.h"
#include "MappedInputManager.h"
#include "QuartzReaderMenuActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
constexpr int16_t CARD_RADIUS = 16;
constexpr int16_t HEADER_ROW_H = 30;
constexpr int16_t ITEM_ROW_H = 44;
constexpr int16_t STEP_BOX = 32;
constexpr int16_t STEP_VALUE_W = 58;
constexpr uint8_t MARGIN_STEP = CrossPointSettings::SCREEN_MARGIN_LARGE_STEP;
constexpr uint8_t LINE_HEIGHT_STEP = 5;

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

StrId orientationLabel(const uint8_t orientation) {
  switch (orientation) {
    case CrossPointSettings::LANDSCAPE_CW:
      return StrId::STR_LANDSCAPE_CW;
    case CrossPointSettings::INVERTED:
      return StrId::STR_ORIENTATION_INVERTED;
    case CrossPointSettings::LANDSCAPE_CCW:
      return StrId::STR_LANDSCAPE_CCW;
    case CrossPointSettings::PORTRAIT:
    default:
      return StrId::STR_PORTRAIT;
  }
}

uint8_t stepClamped(const int value, const int delta, const int minimum, const int maximum) {
  return static_cast<uint8_t>(std::clamp(value + delta, minimum, maximum));
}
}  // namespace

QuartzReaderMenuActivity::AdvKind QuartzReaderMenuActivity::advKind(const Adv row) {
  switch (row) {
    case Adv::HeaderText:
    case Adv::HeaderLayout:
    case Adv::HeaderMore:
    case Adv::Count:
      return AdvKind::Header;
    case Adv::LineHeight:
    case Adv::WordSpacing:
    case Adv::MarginVertical:
    case Adv::MarginHorizontal:
      return AdvKind::Stepper;
    case Adv::Orientation:
    case Adv::Images:
      return AdvKind::Cycle;
    case Adv::AllOptions:
      return AdvKind::Link;
    default:
      return AdvKind::Toggle;
  }
}

StrId QuartzReaderMenuActivity::advLabel(const Adv row) {
  switch (row) {
    case Adv::HeaderText:
      return StrId::STR_QZ_TEXT;
    case Adv::LineHeight:
      return StrId::STR_LINE_SPACING;
    case Adv::WordSpacing:
      return StrId::STR_WORD_SPACING;
    case Adv::Hyphenation:
      return StrId::STR_HYPHENATION;
    case Adv::TextAa:
      return StrId::STR_TEXT_AA;
    case Adv::Focus:
      return StrId::STR_FOCUS_READING;
    case Adv::GuideDots:
      return StrId::STR_GUIDE_READING;
    case Adv::HeaderLayout:
      return StrId::STR_QZ_LAYOUT;
    case Adv::MarginVertical:
      return StrId::STR_QZ_TOP_BOTTOM_MARGIN;
    case Adv::MarginHorizontal:
      return StrId::STR_QZ_SIDE_MARGIN;
    case Adv::ExtraSpacing:
      return StrId::STR_EXTRA_SPACING;
    case Adv::ForceIndents:
      return StrId::STR_FORCE_PARAGRAPH_INDENTS;
    case Adv::Orientation:
      return StrId::STR_ORIENTATION;
    case Adv::Images:
      return StrId::STR_IMAGES;
    case Adv::PublisherPages:
      return StrId::STR_PUBLISHER_PAGE_NUMBERS;
    case Adv::EmbeddedStyle:
      return StrId::STR_QZ_BOOK_STYLES;
    case Adv::HeaderMore:
      return StrId::STR_QZ_MORE_SECTION;
    case Adv::AllOptions:
      return StrId::STR_QZ_ALL_OPTIONS;
    case Adv::Count:
      break;
  }
  return StrId::STR_NONE_OPT;
}

const char* QuartzReaderMenuActivity::advValue(const Adv row, char* buffer, const size_t size) const {
  switch (row) {
    case Adv::LineHeight:
      std::snprintf(buffer, size, "%u%%", static_cast<unsigned>(current.lineHeightPercent));
      return buffer;
    case Adv::WordSpacing:
      std::snprintf(buffer, size, "%u", static_cast<unsigned>(current.wordSpacing));
      return buffer;
    case Adv::MarginVertical:
      std::snprintf(buffer, size, "%u", static_cast<unsigned>(current.marginVertical));
      return buffer;
    case Adv::MarginHorizontal:
      std::snprintf(buffer, size, "%u", static_cast<unsigned>(current.marginHorizontal));
      return buffer;
    case Adv::Orientation:
      return I18N.get(orientationLabel(current.orientation));
    case Adv::Images: {
      static constexpr StrId LABELS[] = {StrId::STR_IMAGES_DISPLAY, StrId::STR_IMAGES_PLACEHOLDER,
                                         StrId::STR_IMAGES_SUPPRESS};
      return I18N.get(LABELS[std::min<uint8_t>(current.imageRendering, 2)]);
    }
    default:
      return nullptr;
  }
}

bool QuartzReaderMenuActivity::advToggleValue(const Adv row) const {
  switch (row) {
    case Adv::Hyphenation:
      return current.hyphenation;
    case Adv::TextAa:
      return current.textAntiAliasing;
    case Adv::Focus:
      return current.focusReading;
    case Adv::GuideDots:
      return current.guideReading;
    case Adv::ExtraSpacing:
      return current.extraSpacing;
    case Adv::ForceIndents:
      return current.forceIndents;
    case Adv::PublisherPages:
      return current.publisherPages;
    case Adv::EmbeddedStyle:
      return current.embeddedStyle;
    default:
      return false;
  }
}

void QuartzReaderMenuActivity::openAdvanced() {
  advancedOpen = true;
  advTop = 0;
  advFocus = Adv::LineHeight;
  requestUpdate();
}

void QuartzReaderMenuActivity::advChange(const Adv row, const int direction) {
  const auto flip = [](uint8_t& value) { value = value ? 0 : 1; };
  switch (row) {
    case Adv::LineHeight:
      if (direction == 0) return;
      current.lineHeightPercent = CrossPointSettings::clampedLineHeightPercent(
          stepClamped(current.lineHeightPercent, direction * LINE_HEIGHT_STEP,
                      CrossPointSettings::MIN_LINE_HEIGHT_PERCENT, CrossPointSettings::MAX_LINE_HEIGHT_PERCENT));
      break;
    case Adv::WordSpacing:
      if (direction == 0) return;
      current.wordSpacing = stepClamped(current.wordSpacing, direction, 0, CrossPointSettings::MAX_WORD_SPACING);
      break;
    case Adv::MarginVertical:
      if (direction == 0) return;
      current.marginVertical =
          stepClamped(current.marginVertical, direction * MARGIN_STEP, CrossPointSettings::MIN_SCREEN_MARGIN,
                      CrossPointSettings::MAX_SCREEN_MARGIN);
      break;
    case Adv::MarginHorizontal:
      if (direction == 0) return;
      current.marginHorizontal =
          stepClamped(current.marginHorizontal, direction * MARGIN_STEP, CrossPointSettings::MIN_SCREEN_MARGIN,
                      CrossPointSettings::MAX_SCREEN_MARGIN);
      break;
    case Adv::Hyphenation:
      flip(current.hyphenation);
      break;
    case Adv::TextAa:
      flip(current.textAntiAliasing);
      break;
    case Adv::Focus:
      flip(current.focusReading);
      break;
    case Adv::GuideDots:
      flip(current.guideReading);
      break;
    case Adv::ExtraSpacing:
      flip(current.extraSpacing);
      break;
    case Adv::ForceIndents:
      flip(current.forceIndents);
      break;
    case Adv::PublisherPages:
      flip(current.publisherPages);
      break;
    case Adv::EmbeddedStyle:
      flip(current.embeddedStyle);
      break;
    case Adv::Orientation: {
      // Cycle in turning order: Portrait, Landscape CW, Portrait 180°, Landscape CCW.
      const int count = CrossPointSettings::ORIENTATION_COUNT;
      const int step = direction < 0 ? count - 1 : 1;
      current.orientation = static_cast<uint8_t>((current.orientation + step) % count);
      break;
    }
    case Adv::Images: {
      const int count = CrossPointSettings::IMAGE_RENDERING_COUNT;
      const int step = direction < 0 ? count - 1 : 1;
      current.imageRendering = static_cast<uint8_t>((current.imageRendering + step) % count);
      break;
    }
    case Adv::AllOptions:
      closeMenu(false, static_cast<int>(EpubReaderMenuAction::READER_OPTIONS));
      return;
    default:
      return;
  }
  requestUpdate();
}

void QuartzReaderMenuActivity::advKeepFocusVisible() {
  const uint8_t focus = static_cast<uint8_t>(advFocus);
  if (focus < advTop) {
    // Show the section header above the first item of a section too.
    advTop = focus > 0 && advKind(static_cast<Adv>(focus - 1)) == AdvKind::Header ? focus - 1 : focus;
  } else if (advShown > 0 && focus >= advTop + advShown) {
    advTop = static_cast<uint8_t>(focus - advShown + 1);
  }
}

void QuartzReaderMenuActivity::advMoveFocus(const bool down) {
  uint8_t index = static_cast<uint8_t>(advFocus);
  for (uint8_t tries = 0; tries < ADV_ROW_COUNT; ++tries) {
    index = static_cast<uint8_t>((index + (down ? 1 : ADV_ROW_COUNT - 1)) % ADV_ROW_COUNT);
    if (advKind(static_cast<Adv>(index)) != AdvKind::Header) break;
  }
  advFocus = static_cast<Adv>(index);
  if (index == 1) advTop = 0;  // wrapped to the top
  advKeepFocusVisible();
  requestUpdate();
}

void QuartzReaderMenuActivity::advScroll(const int rows) {
  const int maxTop = std::max(0, ADV_ROW_COUNT - std::max<int>(1, advShown));
  const int next = std::clamp<int>(advTop + rows, 0, maxTop);
  if (next == advTop) return;
  advTop = static_cast<uint8_t>(next);
  requestUpdate();
}

void QuartzReaderMenuActivity::onAdv(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<QuartzReaderMenuActivity*>(user);
  const int row = event.value / 4;
  const int part = event.value % 4;
  if (row < 0 || row >= ADV_ROW_COUNT) return;
  const auto adv = static_cast<Adv>(row);
  const AdvKind kind = advKind(adv);
  if (kind == AdvKind::Header) return;
  if (kind == AdvKind::Stepper) {
    if (part == 1) self->advChange(adv, -1);
    if (part == 2) self->advChange(adv, +1);
    return;
  }
  self->advChange(adv, kind == AdvKind::Cycle ? 1 : 0);
}

void QuartzReaderMenuActivity::onAdvBack(const fui::ActionEvent&, void* user) {
  auto* self = static_cast<QuartzReaderMenuActivity*>(user);
  self->advancedOpen = false;
  self->requestUpdate();
}

// Button and swipe input while the Advanced page is open. Touch taps were
// already routed by loop().
bool QuartzReaderMenuActivity::advancedLoop() {
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    const int page = std::max<int>(1, advShown - 1);
    advScroll(swipe == MappedInputManager::SwipeDir::Up ? page : -page);
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    advancedOpen = false;
    requestUpdate();
    return true;
  }
  if (!buttonDevice) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) closeMenu(true, -1);
    return true;
  }
  const bool up = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                  mappedInput.wasReleased(MappedInputManager::Button::PageBack);
  const bool down = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                    mappedInput.wasReleased(MappedInputManager::Button::PageForward);
  if (up || down) {
    advMoveFocus(down);
    return true;
  }
  const bool left = mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool right = mappedInput.wasReleased(MappedInputManager::Button::Right);
  const AdvKind kind = advKind(advFocus);
  if (left || right) {
    if (kind == AdvKind::Stepper || kind == AdvKind::Cycle) {
      advChange(advFocus, right ? 1 : -1);
    } else if (kind == AdvKind::Toggle) {
      advChange(advFocus, 0);
    }
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (kind == AdvKind::Stepper) {
      advMoveFocus(true);  // like the quick sheet: Confirm walks down
    } else {
      advChange(advFocus, kind == AdvKind::Cycle ? 1 : 0);
    }
    return true;
  }
  return false;
}

void QuartzReaderMenuActivity::drawAdvanced(UiApp::ScreenType& screen, const fui::Rect area) {
  auto& target = screen.target();
  target.fill(area, WHITE, CARD_RADIUS);
  target.stroke(area, BLACK, 2, CARD_RADIUS);
  screen.frame().hit(area, ACTION_PANEL, 0);

  const fui::Rect inner{static_cast<int16_t>(area.x + 16), static_cast<int16_t>(area.y + 14),
                        static_cast<int16_t>(area.width - 32), static_cast<int16_t>(area.height - 28)};
  const int16_t titleH = target.lineHeight(fui::GfxRendererTarget::FONT_TITLE);
  const int16_t headerH = std::max<int16_t>(titleH, 40);

  // Header: back chevron, title, Done.
  const fui::Rect back{inner.x, inner.y, 40, headerH};
  const int16_t cx = static_cast<int16_t>(back.x + 14);
  const int16_t cy = static_cast<int16_t>(back.y + headerH / 2);
  target.line(fui::Point{static_cast<int16_t>(cx + 7), static_cast<int16_t>(cy - 9)}, fui::Point{cx, cy}, 3, BLACK);
  target.line(fui::Point{cx, cy}, fui::Point{static_cast<int16_t>(cx + 7), static_cast<int16_t>(cy + 9)}, 3, BLACK);
  screen.frame().hit(back, ACTION_ADV_BACK, 0);
  target.text(fui::Rect{static_cast<int16_t>(inner.x + 44), inner.y, static_cast<int16_t>(inner.width - 150), headerH},
              tr(STR_QZ_ADVANCED), textStyle(fui::GfxRendererTarget::FONT_TITLE, fui::TextAlign::Left, false, true));
  const fui::Rect done{static_cast<int16_t>(inner.right() - 96), inner.y, 96, headerH};
  target.stroke(done, BLACK, 2, static_cast<uint8_t>(headerH / 2));
  target.text(done, tr(STR_DONE), textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Center, false, true));
  screen.frame().hit(done, ACTION_DONE, 0);

  // List.
  const int16_t listTop = static_cast<int16_t>(inner.y + headerH + 8);
  const int16_t listBottom = inner.bottom();
  const int16_t smallH = target.lineHeight(fui::GfxRendererTarget::FONT_SMALL);
  int16_t y = listTop;
  uint8_t shown = 0;
  char value[24];
  for (uint8_t i = advTop; i < ADV_ROW_COUNT; ++i) {
    const auto row = static_cast<Adv>(i);
    const AdvKind kind = advKind(row);
    const int16_t rowH = kind == AdvKind::Header ? HEADER_ROW_H : ITEM_ROW_H;
    if (y + rowH > listBottom) break;
    const fui::Rect rect{inner.x, y, inner.width, rowH};
    ++shown;

    if (kind == AdvKind::Header) {
      target.text(fui::Rect{rect.x, static_cast<int16_t>(rect.bottom() - smallH - 2), rect.width, smallH},
                  I18N.get(advLabel(row)),
                  textStyle(fui::GfxRendererTarget::FONT_SMALL, fui::TextAlign::Left, false, true));
      y = static_cast<int16_t>(y + rowH);
      continue;
    }

    const bool focused = buttonDevice && advFocus == row;
    if (focused) {
      target.stroke(fui::Rect{static_cast<int16_t>(rect.x - 6), static_cast<int16_t>(rect.y + 1),
                              static_cast<int16_t>(rect.width + 12), static_cast<int16_t>(rect.height - 2)},
                    BLACK, 3, 10);
    }
    // Rule under every item that is followed by another item.
    if (i + 1 < ADV_ROW_COUNT && advKind(static_cast<Adv>(i + 1)) != AdvKind::Header) {
      target.fill(fui::Rect{rect.x, static_cast<int16_t>(rect.bottom() - 1), rect.width, 1}, BLACK);
    }

    const int16_t controlW = kind == AdvKind::Stepper  ? static_cast<int16_t>(2 * STEP_BOX + STEP_VALUE_W + 8)
                             : kind == AdvKind::Toggle ? 48
                             : kind == AdvKind::Link   ? 24
                                                       : static_cast<int16_t>(rect.width / 2);
    const fui::Rect labelRect{rect.x, rect.y, static_cast<int16_t>(rect.width - controlW - 8), rect.height};
    target.text(labelRect, I18N.get(advLabel(row)),
                textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Left, false, kind == AdvKind::Link));
    if (kind != AdvKind::Stepper) screen.frame().hit(rect, ACTION_ADV, static_cast<int16_t>(i * 4));

    switch (kind) {
      case AdvKind::Stepper: {
        const int16_t boxY = static_cast<int16_t>(rect.y + (rect.height - STEP_BOX) / 2);
        const fui::Rect plus{static_cast<int16_t>(rect.right() - STEP_BOX), boxY, STEP_BOX, STEP_BOX};
        const fui::Rect minus{static_cast<int16_t>(plus.x - STEP_VALUE_W - STEP_BOX), boxY, STEP_BOX, STEP_BOX};
        for (const fui::Rect& box : {minus, plus}) target.stroke(box, BLACK, 2, 8);
        const int16_t midY = static_cast<int16_t>(boxY + STEP_BOX / 2);
        target.fill(fui::Rect{static_cast<int16_t>(minus.x + 9), static_cast<int16_t>(midY - 1), 14, 3}, BLACK);
        target.fill(fui::Rect{static_cast<int16_t>(plus.x + 9), static_cast<int16_t>(midY - 1), 14, 3}, BLACK);
        target.fill(fui::Rect{static_cast<int16_t>(plus.x + 15), static_cast<int16_t>(midY - 7), 3, 14}, BLACK);
        target.text(fui::Rect{static_cast<int16_t>(minus.right()), rect.y, STEP_VALUE_W, rect.height},
                    advValue(row, value, sizeof(value)),
                    textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Center));
        // Tap bands split the row height so the small boxes are easy to hit.
        screen.frame().hit(
            fui::Rect{static_cast<int16_t>(minus.x - 6), rect.y, static_cast<int16_t>(STEP_BOX + 12), rect.height},
            ACTION_ADV, static_cast<int16_t>(i * 4 + 1));
        screen.frame().hit(
            fui::Rect{static_cast<int16_t>(plus.x - 6), rect.y, static_cast<int16_t>(STEP_BOX + 12), rect.height},
            ACTION_ADV, static_cast<int16_t>(i * 4 + 2));
        break;
      }
      case AdvKind::Toggle: {
        const bool on = advToggleValue(row);
        const fui::Rect pill{static_cast<int16_t>(rect.right() - 46),
                             static_cast<int16_t>(rect.y + (rect.height - 26) / 2), 46, 26};
        target.fill(pill, on ? BLACK : WHITE, 13);
        target.stroke(pill, BLACK, 2, 13);
        const fui::Rect knob{static_cast<int16_t>(on ? pill.right() - 21 : pill.x + 5),
                             static_cast<int16_t>(pill.y + 5), 16, 16};
        target.fill(knob, on ? WHITE : BLACK, 8);
        break;
      }
      case AdvKind::Cycle: {
        char label[40];
        std::snprintf(label, sizeof(label), "%s  >", advValue(row, value, sizeof(value)));
        target.text(fui::Rect{static_cast<int16_t>(rect.right() - controlW), rect.y, controlW, rect.height}, label,
                    textStyle(fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Right));
        break;
      }
      case AdvKind::Link: {
        const int16_t chevronX = static_cast<int16_t>(rect.right() - 8);
        const int16_t chevronY = static_cast<int16_t>(rect.y + rect.height / 2);
        target.line(fui::Point{static_cast<int16_t>(chevronX - 6), static_cast<int16_t>(chevronY - 8)},
                    fui::Point{chevronX, chevronY}, 3, BLACK);
        target.line(fui::Point{chevronX, chevronY},
                    fui::Point{static_cast<int16_t>(chevronX - 6), static_cast<int16_t>(chevronY + 8)}, 3, BLACK);
        break;
      }
      case AdvKind::Header:
        break;
    }
    y = static_cast<int16_t>(y + rowH);
  }
  advShown = shown;

  // Scroll cues: small arrows on the right edge when rows are hidden.
  const int16_t arrowX = static_cast<int16_t>(area.right() - 10);
  if (advTop > 0) {
    target.triangle(fui::Point{arrowX, static_cast<int16_t>(listTop - 6)},
                    fui::Point{static_cast<int16_t>(arrowX - 5), static_cast<int16_t>(listTop + 2)},
                    fui::Point{static_cast<int16_t>(arrowX + 5), static_cast<int16_t>(listTop + 2)}, BLACK);
  }
  if (advTop + shown < ADV_ROW_COUNT) {
    target.triangle(fui::Point{arrowX, static_cast<int16_t>(listBottom + 6)},
                    fui::Point{static_cast<int16_t>(arrowX - 5), static_cast<int16_t>(listBottom - 2)},
                    fui::Point{static_cast<int16_t>(arrowX + 5), static_cast<int16_t>(listBottom - 2)}, BLACK);
  }

  if (buttonDevice) {
    const AdvKind kind = advKind(advFocus);
    const char* confirmLabel = kind == AdvKind::Stepper ? tr(STR_NEXT_FIELD) : tr(STR_SELECT);
    const char* prevLabel = kind == AdvKind::Stepper ? "-" : "<";
    const char* nextLabel = kind == AdvKind::Stepper ? "+" : ">";
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, prevLabel, nextLabel);
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}
