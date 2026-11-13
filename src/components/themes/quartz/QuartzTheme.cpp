#include "QuartzTheme.h"

#include <HalGPIO.h>

#include "components/QuartzChrome.h"

void QuartzTheme::drawHeader(const GfxRenderer& renderer, const Rect rect, const char* title, const char* subtitle,
                             const bool backButton) const {
  // Untitled bands (status-only headers) keep Lyra's drawing.
  if (title == nullptr || title[0] == '\0') {
    LyraTheme::drawHeader(renderer, rect, title, subtitle, backButton);
    return;
  }
  QuartzChrome::drawTitleBar(renderer, rect, title, backButton && gpio.hasTouch(), subtitle);
}
