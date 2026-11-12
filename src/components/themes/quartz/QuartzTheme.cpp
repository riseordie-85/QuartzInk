#include "QuartzTheme.h"

#include <GfxRenderer.h>

#include <algorithm>

#include "components/QuartzChrome.h"
#include "components/TouchRegistry.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

namespace {
constexpr int SIDE_INSET = 12;
constexpr int BAR_HEIGHT = 56;

Rect segmentBarRect(const Rect rect) {
  return Rect{rect.x + SIDE_INSET, rect.y, std::max(0, rect.width - 2 * SIDE_INSET), rect.height};
}

fui::Rect toUi(const Rect rect) {
  return fui::Rect{static_cast<int16_t>(rect.x), static_cast<int16_t>(rect.y), static_cast<int16_t>(rect.width),
                   static_cast<int16_t>(rect.height)};
}
}  // namespace

void QuartzTheme::drawHeader(const GfxRenderer& renderer, const Rect rect, const char* title, const char* subtitle,
                             const bool readerContext, const bool showStatus) const {
  // Untitled headers (status-only bands) and in-book headers keep Lyra's drawing.
  if (readerContext || title == nullptr || title[0] == '\0') {
    LyraTheme::drawHeader(renderer, rect, title, subtitle, readerContext, showStatus);
    return;
  }
  // The bar sits at the top of the header band; the clock replaces the
  // separate battery row, matching the Quartz Home top bar.
  const Rect bar{rect.x, rect.y, rect.width, std::min(rect.height, BAR_HEIGHT + 8)};
  QuartzChrome::drawTitleBar(renderer, bar, title, false, subtitle);
}

void QuartzTheme::drawTabBar(const GfxRenderer& renderer, const Rect rect, const std::vector<TabInfo>& tabs,
                             const bool selected) const {
  if (tabs.empty()) return;
  auto target = makeUiTarget(renderer);
  constexpr size_t MAX_TABS = 8;
  const char* labels[MAX_TABS];
  int selectedIndex = -1;
  const int count = static_cast<int>(std::min(tabs.size(), MAX_TABS));
  for (int i = 0; i < count; ++i) {
    labels[i] = tabs[static_cast<size_t>(i)].label;
    if (tabs[static_cast<size_t>(i)].selected) selectedIndex = i;
  }
  const fui::Rect bar = toUi(segmentBarRect(rect));
  QuartzChrome::paintSegmentTabs(target, bar, labels, count, selectedIndex, selected,
                                 uiThemeTokens(target).smallText.font);
  for (int i = 0; i < count; ++i) {
    const fui::Rect seg = QuartzChrome::segmentRect(bar, i, count);
    TouchRegistry::getInstance().add(Rect{seg.x, rect.y, seg.width, rect.height}, i, TouchRegistry::Tab);
  }
}

bool QuartzTheme::tabIndexFromPoint(const GfxRenderer&, const Rect rect, const std::vector<TabInfo>& tabs, const int x,
                                    const int y, int& index) const {
  if (tabs.empty() || y < rect.y || y >= rect.y + rect.height) return false;
  const fui::Rect bar = toUi(segmentBarRect(rect));
  const int count = static_cast<int>(std::min<size_t>(tabs.size(), 8));
  for (int i = 0; i < count; ++i) {
    const fui::Rect seg = QuartzChrome::segmentRect(bar, i, count);
    if (x >= seg.x && x < seg.x + seg.width) {
      index = i;
      return true;
    }
  }
  return false;
}

void QuartzTheme::drawList(const GfxRenderer& renderer, const Rect rect, const int itemCount, const int selectedIndex,
                           const std::function<std::string(int index)>& rowTitle,
                           const std::function<std::string(int index)>& rowSubtitle,
                           const std::function<UIIcon(int index)>& rowIcon,
                           const std::function<std::string(int index)>& rowValue, const bool highlightValue,
                           const std::function<bool(int index)>& rowDimmed,
                           const std::function<bool(int index)>& isHeader, const int rowHeightScale,
                           const bool showSelection) const {
  // Lyra's rows, with the selected row filled black and its text white.
  drawListWithMetrics(renderer, rect, itemCount, selectedIndex, rowTitle, rowSubtitle, rowIcon, rowValue,
                      highlightValue, rowDimmed, isHeader, QuartzMetrics::values, /*invertSelectedRows=*/true,
                      rowHeightScale, showSelection);
}
