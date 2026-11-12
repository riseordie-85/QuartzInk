#include "QuartzChrome.h"

#include <FreeInkUIIcon.h>
#include <HalClock.h>
#include <HalPowerManager.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "UITheme.h"
#include "UIThemeTokens.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UiAppHelpers.h"
#include "components/icons/touchHeaderIcons.h"
#include "fontIds.h"
#include "images/QuartzLogo.h"

namespace fui = freeink::ui;

namespace QuartzChrome {

bool active() { return SETTINGS.uiTheme == CrossPointSettings::UI_THEME::QUARTZ; }

void drawTitleBar(const GfxRenderer& renderer, const Rect& header, const char* title, const bool backArrow,
                  const char* rightLabel) {
  auto target = makeUiTarget(renderer);
  constexpr int16_t SIDE = 12;
  constexpr int16_t VERTICAL = 4;
  const fui::Rect bar{static_cast<int16_t>(header.x + SIDE), static_cast<int16_t>(header.y + VERTICAL),
                      static_cast<int16_t>(std::max(0, header.width - 2 * SIDE)),
                      static_cast<int16_t>(std::max(0, header.height - 2 * VERTICAL))};
  target.fill(bar, fui::Paint::solid(fui::Color::Black), RADIUS);

  int16_t titleX = static_cast<int16_t>(bar.x + 18);
  if (backArrow) {
    // Same spot as the stock back button, so its tap target still lines up.
    const auto back = TouchHeaderBackButton::layout(header);
    const int16_t size = TouchHeaderBackButton::ICON_SIZE;
    const int16_t iconX =
        static_cast<int16_t>(std::max<int>(bar.x + 8, back.iconRect.x + (back.iconRect.width - size) / 2));
    const fui::Rect icon{iconX, static_cast<int16_t>(bar.y + (bar.height - size) / 2), size, size};
    target.bitmap(icon, fui::bitmapFromIcon(icon_back_32), fui::BitmapMode::Center,
                  fui::Paint::solid(fui::Color::White));
    titleX = static_cast<int16_t>(icon.right() + 8);
  }

  const auto tokens = uiThemeTokens(target);
  // Far right: the battery, white on the bar.
  const int batteryW = drawBatteryOnBlack(renderer, bar.right() - 18, bar.y + bar.height / 2, true);
  const int16_t rightEdge = static_cast<int16_t>(bar.right() - 18 - (batteryW > 0 ? batteryW + 14 : 0));
  // Then the caller's label, or the clock like the Quartz Home bar.
  char clock[16] = "";
  const char* right = rightLabel;
  if (right == nullptr && halClock.isAvailable() &&
      halClock.formatTime(clock, sizeof(clock), SETTINGS.clockUtcOffsetQ, SETTINGS.clockFormat == 1)) {
    right = clock;
  }
  int16_t rightW = 0;
  if (right != nullptr && right[0] != '\0') {
    fui::TextStyle rightStyle = tokens.bodyText;
    rightStyle.align = fui::TextAlign::Right;
    rightStyle.color = fui::Color::White;
    rightStyle.maxLines = 1;
    rightW = static_cast<int16_t>(
        std::min<int>(bar.width / 2, target.measureText(rightStyle.font, right, rightStyle).width + 4));
    target.text(fui::Rect{static_cast<int16_t>(rightEdge - rightW), bar.y, rightW, bar.height}, right, rightStyle);
  }

  fui::TextStyle style = tokens.titleText;
  style.align = fui::TextAlign::Left;
  style.color = fui::Color::White;
  style.bold = true;
  style.maxLines = 1;
  const int titleRight = rightEdge - (rightW > 0 ? rightW + 12 : 0);
  target.text(fui::Rect{titleX, bar.y, static_cast<int16_t>(std::max(0, titleRight - titleX)), bar.height},
              title != nullptr ? title : "", style);
}

int drawLogo(const GfxRenderer& renderer, const int y) {
  auto target = makeUiTarget(renderer);
  fui::BitmapRef logo;
  logo.data = QuartzLogo;
  logo.width = QUARTZ_LOGO_WIDTH;
  logo.height = QUARTZ_LOGO_HEIGHT;
  logo.format = fui::BitmapFormat::BW1;
  // Drawn per pixel through the renderer, so it follows the screen orientation.
  const int x = (renderer.getScreenWidth() - QUARTZ_LOGO_WIDTH) / 2;
  target.bitmap(fui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(QUARTZ_LOGO_WIDTH),
                          static_cast<int16_t>(QUARTZ_LOGO_HEIGHT)},
                logo, fui::BitmapMode::Center);
  return QUARTZ_LOGO_HEIGHT;
}

int drawBatteryOnBlack(const GfxRenderer& renderer, const int x, const int centerY, const bool alignRight) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const bool showPercent = SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_ALWAYS;
  int textW = 0;
  if (showPercent) {
    char percent[8];
    std::snprintf(percent, sizeof(percent), "%u%%", static_cast<unsigned>(powerManager.getBatteryPercentage()));
    textW = renderer.getTextWidth(SMALL_FONT_ID, percent);
  }
  constexpr int GAP = 4;  // matches the theme's space between percentage and icon
  const int totalW = metrics.batteryWidth + (showPercent ? textW + GAP : 0);
  // drawBatteryRight takes the icon's left edge and puts the percentage before it.
  const int iconX = alignRight ? x - metrics.batteryWidth : x + totalW - metrics.batteryWidth;
  const int top = centerY - renderer.getLineHeight(SMALL_FONT_ID) / 2;
  GUI.drawBatteryRight(renderer, Rect{iconX, top, metrics.batteryWidth, metrics.batteryHeight}, showPercent,
                       /*foregroundBlack=*/false);
  return totalW;
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

void applyListStyle(fui::ListProps& props, const fui::ThemeTokens& tokens) {
  if (props.rowStyles.explicitlySet) return;
  props.rowStyles = rowStyles();
  props.rowRadius = RADIUS;
  if (props.rowGap < 0) props.rowGap = 6;
  if (props.rowInset < 0) props.rowInset = 12;
  props.separatorPaint = fui::Paint::none();
  props.headerText = tokens.smallText;
  props.headerText.bold = true;
  props.headerUnderline = false;
  props.toggleRadius = 9;
  props.toggleKnobRadius = 6;
  props.toggleBorderWidth = 2;
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
