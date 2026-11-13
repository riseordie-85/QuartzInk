#include "QuartzChrome.h"

#include <FreeInkUIIcon.h>
#include <HalClock.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "HeaderBackTapTarget.h"
#include "UITheme.h"
#include "UIThemeTokens.h"
#include "UiAppHelpers.h"
#include "fontIds.h"
#include "icons/headerIcons.h"

namespace fui = freeink::ui;

namespace QuartzChrome {

bool active() { return SETTINGS.uiTheme == CrossPointSettings::UI_THEME::QUARTZ; }

int drawBatteryOnBlack(const GfxRenderer& renderer, const int right, const int centerY) {
  const uint16_t percent = powerManager.getBatteryPercentage();
  const bool showPercent = SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_ALWAYS;
  constexpr int ICON_W = 22;
  constexpr int ICON_H = 12;
  constexpr int GAP = 5;

  // Icon: white outline with a nub, filled to the charge level.
  const int iconX = right - ICON_W;
  const int iconY = centerY - ICON_H / 2;
  renderer.drawRect(iconX, iconY, ICON_W - 2, ICON_H, false);
  renderer.fillRect(iconX + ICON_W - 2, iconY + 3, 2, ICON_H - 6, false);
  const int fillW = std::clamp<int>((ICON_W - 6) * percent / 100 + 1, 1, ICON_W - 6);
  renderer.fillRect(iconX + 2, iconY + 2, fillW, ICON_H - 4, false);

  int width = ICON_W;
  if (showPercent) {
    char text[8];
    std::snprintf(text, sizeof(text), "%u%%", static_cast<unsigned>(percent));
    const int textW = renderer.getTextWidth(SMALL_FONT_ID, text);
    renderer.drawText(SMALL_FONT_ID, iconX - GAP - textW, centerY - renderer.getLineHeight(SMALL_FONT_ID) / 2, text,
                      false);
    width += GAP + textW;
  }
  return width;
}

void drawTitleBar(const GfxRenderer& renderer, const Rect& band, const char* title, const bool backArrow,
                  const char* rightLabel) {
  auto target = makeUiTarget(renderer);
  int top = 0, right = 0, bottom = 0, left = 0;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  constexpr int16_t SIDE = 10;
  // The bar sits at the band's bottom edge, so content below keeps its place
  // and the band's top keeps clear of the bezel.
  const int barH = std::min<int>(BAR_HEIGHT, band.height - 4);
  const int barX = std::max(band.x, left) + SIDE;
  const int barW = std::min(band.x + band.width, renderer.getScreenWidth() - right) - SIDE - barX;
  const fui::Rect bar{static_cast<int16_t>(barX), static_cast<int16_t>(band.y + band.height - barH - 2),
                      static_cast<int16_t>(std::max(0, barW)), static_cast<int16_t>(barH)};
  target.fill(bar, fui::Paint::solid(fui::Color::Black), RADIUS);

  int16_t titleX = static_cast<int16_t>(bar.x + 16);
  if (backArrow) {
    constexpr int16_t SIZE = 32;
    const fui::Rect icon{static_cast<int16_t>(bar.x + 8), static_cast<int16_t>(bar.y + (bar.height - SIZE) / 2), SIZE,
                         SIZE};
    target.bitmap(icon, fui::bitmapFromIcon(icon_header_back_32), fui::BitmapMode::Center,
                  fui::Paint::solid(fui::Color::White));
    HeaderBackTapTarget::set(icon.x - 4, bar.y, SIZE + 8, bar.height);
    titleX = static_cast<int16_t>(icon.right() + 6);
  } else {
    HeaderBackTapTarget::clear();
  }

  const auto tokens = uiThemeTokens(target);
  const int batteryW = drawBatteryOnBlack(renderer, bar.right() - 16, bar.y + bar.height / 2);
  const int16_t rightEdge = static_cast<int16_t>(bar.right() - 16 - batteryW - 14);

  char clock[12] = "";
  const char* label = rightLabel;
  if (label == nullptr && halClock.isAvailable() &&
      halClock.formatTime(clock, sizeof(clock), SETTINGS.clockFormat == 1)) {
    label = clock;
  }
  int16_t labelW = 0;
  if (label != nullptr && label[0] != '\0') {
    fui::TextStyle style = tokens.smallText;
    style.align = fui::TextAlign::Right;
    style.color = fui::Color::White;
    style.maxLines = 1;
    labelW = static_cast<int16_t>(std::min<int>(bar.width / 2, target.measureText(style.font, label, style).width + 4));
    target.text(fui::Rect{static_cast<int16_t>(rightEdge - labelW), bar.y, labelW, bar.height}, label, style);
  }

  fui::TextStyle style = tokens.titleText;
  style.align = fui::TextAlign::Left;
  style.color = fui::Color::White;
  style.bold = true;
  style.maxLines = 1;
  const int titleRight = rightEdge - (labelW > 0 ? labelW + 12 : 0);
  target.text(fui::Rect{titleX, bar.y, static_cast<int16_t>(std::max(0, titleRight - titleX)), bar.height},
              title != nullptr ? title : "", style);
}

fui::StyleSet rowStyles() {
  fui::StyleSet styles;
  styles.explicitlySet = true;
  styles.normal.background = fui::Paint::solid(fui::Color::White);
  styles.normal.foreground = fui::Paint::solid(fui::Color::Black);
  styles.normal.border = fui::Paint::solid(fui::Color::Black);
  styles.normal.borderWidth = BORDER;
  styles.normal.radius = RADIUS;
  styles.selected = styles.normal;
  styles.selected.background = fui::Paint::solid(fui::Color::Black);
  styles.selected.foreground = fui::Paint::solid(fui::Color::White);
  styles.focused = styles.selected;
  styles.active = styles.selected;
  styles.disabled = styles.normal;
  styles.disabled.foreground = fui::Paint::dither(fui::Color::DarkGray);
  return styles;
}

fui::Rect segmentRect(const fui::Rect rect, const int index, const int count) {
  constexpr int16_t INNER = 4;
  const int16_t segW = static_cast<int16_t>((rect.width - 2 * INNER) / std::max(1, count));
  const int16_t x = static_cast<int16_t>(rect.x + INNER + index * segW);
  const int16_t w = static_cast<int16_t>(index == count - 1 ? rect.right() - INNER - x : segW);
  return fui::Rect{x, static_cast<int16_t>(rect.y + INNER), w, static_cast<int16_t>(rect.height - 2 * INNER)};
}

void paintSegmentTabs(fui::DrawTarget& target, const fui::Rect rect, const char* const* labels, const int count,
                      const int selected, const bool focused, const fui::FontId font) {
  if (count <= 0 || rect.width <= 0) return;
  const fui::Paint black = fui::Paint::solid(fui::Color::Black);
  target.fill(rect, fui::Paint::solid(fui::Color::White), RADIUS);
  for (int i = 0; i < count; ++i) {
    const fui::Rect seg = segmentRect(rect, i, count);
    const bool isSelected = i == selected;
    if (isSelected) target.fill(seg, black, static_cast<uint8_t>(RADIUS - 4));
    fui::TextStyle style;
    style.font = font;
    style.align = fui::TextAlign::Center;
    style.color = isSelected ? fui::Color::White : fui::Color::Black;
    style.bold = isSelected;
    style.maxLines = 1;
    target.text(fui::Rect{static_cast<int16_t>(seg.x + 2), seg.y, static_cast<int16_t>(seg.width - 4), seg.height},
                labels[i] != nullptr ? labels[i] : "", style);
  }
  target.stroke(rect, black, static_cast<uint8_t>(focused ? BORDER + 2 : BORDER), RADIUS);
}

}  // namespace QuartzChrome
