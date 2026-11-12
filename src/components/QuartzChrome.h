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

// True when the Quartz UI theme is selected; other themes keep their own look.
bool active();

// Black rounded bar across `header` with the title in white. `backArrow` adds a
// white back chevron where TouchHeaderBackButton's tap target already sits.
// `rightLabel` is drawn on the right; when null, the clock (if set) is.
void drawTitleBar(const GfxRenderer& renderer, const Rect& header, const char* title, bool backArrow,
                  const char* rightLabel = nullptr);

// The QuartzInk gem logo and wordmark, centered horizontally with its top at `y`.
// Returns the logo's height.
int drawLogo(const GfxRenderer& renderer, int y);

// The battery icon and percentage in white, for drawing on a black bar.
// Anchored by its right edge (alignRight) or its left edge at `x`, vertically
// centered on `centerY`. Honors Hide Battery %. Returns the width it used.
int drawBatteryOnBlack(const GfxRenderer& renderer, int x, int centerY, bool alignRight);

// Outlined, rounded list rows; the selected row fills black with white text.
freeink::ui::StyleSet rowStyles();

// Applies the Quartz row look (outlined rounded rows with a gap, bold section
// labels) to a FreeInkUI list that has not chosen its own row styles.
void applyListStyle(freeink::ui::ListProps& props, const freeink::ui::ThemeTokens& tokens);

// Segment rectangle `index` of `count` inside a segment control at `rect`.
freeink::ui::Rect segmentRect(freeink::ui::Rect rect, int index, int count);

// Draws the segment control without registering touch targets.
void paintSegmentTabs(freeink::ui::DrawTarget& target, freeink::ui::Rect rect, const char* const* labels, int count,
                      int selected, bool focused, freeink::ui::FontId font);

// Outlined segment control; the selected segment fills black. `focused` thickens
// the outline so button devices can see the tabs hold the focus. Each segment
// registers `action` with its index as the value.
template <typename FrameT>
void drawSegmentTabs(FrameT& frame, freeink::ui::DrawTarget& target, const freeink::ui::Rect rect,
                     const char* const* labels, const int count, const int selected, const bool focused,
                     const freeink::ui::FontId font, const freeink::ui::ActionId action,
                     const int16_t* values = nullptr) {
  paintSegmentTabs(target, rect, labels, count, selected, focused, font);
  for (int i = 0; i < count; ++i) {
    frame.hit(segmentRect(rect, i, count), action, values != nullptr ? values[i] : static_cast<int16_t>(i));
  }
}

}  // namespace QuartzChrome
