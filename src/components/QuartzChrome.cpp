#include "QuartzChrome.h"

#include <FreeInkUIIcon.h>

#include <algorithm>

#include "UIThemeTokens.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UiAppHelpers.h"
#include "components/icons/touchHeaderIcons.h"

namespace fui = freeink::ui;

namespace QuartzChrome {

void drawTitleBar(const GfxRenderer& renderer, const Rect& header, const char* title, const bool backArrow) {
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

  fui::TextStyle style = uiThemeTokens(target).titleText;
  style.align = fui::TextAlign::Left;
  style.color = fui::Color::White;
  style.bold = true;
  style.maxLines = 1;
  target.text(fui::Rect{titleX, bar.y, static_cast<int16_t>(std::max(0, bar.right() - 18 - titleX)), bar.height}, title,
              style);
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

}  // namespace QuartzChrome
