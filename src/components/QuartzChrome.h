#pragma once

#include <FreeInkUI.h>

#include "GfxRenderer.h"
#include "themes/BaseTheme.h"

// Shared pieces of QuartzInk's look, so list screens match Home and the
// reader's page settings: a black rounded title bar, outlined segment tabs,
// and outlined rounded rows that fill black when selected.
namespace QuartzChrome {

constexpr uint8_t RADIUS = 12;
constexpr uint8_t BORDER = 2;
constexpr int16_t ROW_GAP = 8;

// Black rounded bar across `header` with the title in white. `backArrow` adds a
// white back chevron where TouchHeaderBackButton's tap target already sits.
void drawTitleBar(const GfxRenderer& renderer, const Rect& header, const char* title, bool backArrow);

// The QuartzInk gem logo and wordmark, centered horizontally with its top at `y`.
// Returns the logo's height.
int drawLogo(const GfxRenderer& renderer, int y);

// Outlined, rounded list rows; the selected row fills black with white text.
freeink::ui::StyleSet rowStyles();

// Outlined segment control; the selected segment fills black. `focused` thickens
// the outline so button devices can see the tabs hold the focus. Each segment
// registers `action` with its index as the value.
template <typename FrameT>
void drawSegmentTabs(FrameT& frame, freeink::ui::DrawTarget& target, const freeink::ui::Rect rect,
                     const char* const* labels, const int count, const int selected, const bool focused,
                     const freeink::ui::FontId font, const freeink::ui::ActionId action) {
  namespace fui = freeink::ui;
  if (count <= 0 || rect.width <= 0) return;
  const fui::Paint black = fui::Paint::solid(fui::Color::Black);
  target.fill(rect, fui::Paint::solid(fui::Color::White), RADIUS);
  const int16_t inner = 4;
  const int16_t segW = static_cast<int16_t>((rect.width - 2 * inner) / count);
  for (int i = 0; i < count; ++i) {
    const fui::Rect seg{
        static_cast<int16_t>(rect.x + inner + i * segW), static_cast<int16_t>(rect.y + inner),
        static_cast<int16_t>(i == count - 1 ? rect.right() - inner - (rect.x + inner + i * segW) : segW),
        static_cast<int16_t>(rect.height - 2 * inner)};
    const bool isSelected = i == selected;
    if (isSelected) target.fill(seg, black, static_cast<uint8_t>(RADIUS - 4));
    fui::TextStyle style;
    style.font = font;
    style.align = fui::TextAlign::Center;
    style.color = isSelected ? fui::Color::White : fui::Color::Black;
    style.bold = isSelected;
    style.maxLines = 1;
    target.text(fui::Rect{static_cast<int16_t>(seg.x + 2), seg.y, static_cast<int16_t>(seg.width - 4), seg.height},
                labels[i], style);
    frame.hit(seg, action, static_cast<int16_t>(i));
  }
  target.stroke(rect, black, static_cast<uint8_t>(focused ? BORDER + 2 : BORDER), RADIUS);
}

}  // namespace QuartzChrome
