#pragma once

#include "components/themes/lyra/LyraTheme.h"

// QuartzInk's UI theme: Lyra's layout, drawn in the Quartz style used by Quartz
// Home and the reader's page settings. Headers become a black rounded title bar
// with the clock and battery; lists get outlined rounded rows that fill black
// when selected (tokens.listRow, see UIThemeTokens.h); tabbed lists draw an
// outlined segment control (UiTabListActivity).
namespace QuartzMetrics {
inline constexpr ThemeMetrics values = [] {
  ThemeMetrics m = LyraMetrics::values;
  m.headerHeight = 66;        // the title bar plus a little air; no separate status strip
  m.headerUnderlineSize = 0;  // the bar carries the edge
  m.listRowGap = 6;
  m.listRowRadius = 12;
  m.listInset = 12;
  m.listSelectionStyle = 0;  // invert fill: black row, white text
  m.listTitleBold = false;
  m.tabBarHeight = 52;
  return m;
}();
}  // namespace QuartzMetrics

class QuartzTheme : public LyraTheme {
 public:
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool backButton = true) const override;
};
