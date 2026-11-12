#pragma once

#include "components/themes/lyra/LyraTheme.h"

// QuartzInk's UI theme: Lyra's layout and metrics, drawn in the Quartz style
// used by Quartz Home and the reader's page settings — a black rounded title
// bar with the clock, an outlined segment control for tabs, and black
// selected list rows. Lists built with FreeInkUI pick up outlined rows from
// configureUiList when this theme is active.
namespace QuartzMetrics {
inline constexpr const ThemeMetrics& values = LyraMetrics::values;
}

class QuartzTheme : public LyraTheme {
 public:
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool readerContext = false, bool showStatus = true) const override;
  void drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                  bool selected) const override;
  bool tabIndexFromPoint(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs, int x, int y,
                         int& index) const override;
  void drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                const std::function<std::string(int index)>& rowTitle,
                const std::function<std::string(int index)>& rowSubtitle,
                const std::function<UIIcon(int index)>& rowIcon, const std::function<std::string(int index)>& rowValue,
                bool highlightValue, const std::function<bool(int index)>& rowDimmed = nullptr,
                const std::function<bool(int index)>& isHeader = nullptr, int rowHeightScale = 1,
                bool showSelection = true) const override;
};
