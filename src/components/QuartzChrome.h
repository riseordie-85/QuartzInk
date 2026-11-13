#pragma once

#include <FreeInkUI.h>

#include "GfxRenderer.h"
#include "themes/BaseTheme.h"

// Shared pieces of QuartzInk's look, so list screens match Home and the
// reader's page settings: a black rounded title bar with the clock and
// battery, an outlined segment control for tabs, and outlined rounded rows
// that fill black when selected.
namespace QuartzChrome {

constexpr uint8_t RADIUS = 12;
constexpr uint8_t BORDER = 2;
constexpr int16_t ROW_GAP = 6;
constexpr int16_t BAR_HEIGHT = 50;

// True when the Quartz UI theme is selected; other themes keep their own look.
bool active();

// Black rounded bar across `band` with the title in white. `backArrow` adds a
// white back chevron and records its tap rect in HeaderBackTapTarget.
// `rightLabel` is drawn on the right; when null, the clock (if known) is.
void drawTitleBar(const GfxRenderer& renderer, const Rect& band, const char* title, bool backArrow,
                  const char* rightLabel = nullptr);

// Battery icon and percentage in white for a black bar, right edge at `right`,
// centered on `centerY`. Honors Hide Battery %. Returns the width used.
int drawBatteryOnBlack(const GfxRenderer& renderer, int right, int centerY);

// Outlined, rounded list rows; the selected row fills black with white text.
freeink::ui::StyleSet rowStyles();

// Segment rectangle `index` of `count` inside a segment control at `rect`.
freeink::ui::Rect segmentRect(freeink::ui::Rect rect, int index, int count);

// Outlined segment control; the selected segment fills black. `focused`
// thickens the outline so button devices can see the tabs hold the focus.
void paintSegmentTabs(freeink::ui::DrawTarget& target, freeink::ui::Rect rect, const char* const* labels, int count,
                      int selected, bool focused, freeink::ui::FontId font);

}  // namespace QuartzChrome
