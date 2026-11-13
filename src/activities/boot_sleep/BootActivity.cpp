#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/QuartzLogoDraw.h"
#include "fontIds.h"
#include "images/QuartzLogo.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  (void)pageWidth;
  // QuartzInk: the gem logo and wordmark, with the fork credit and version along the bottom.
  const int logoTop = (pageHeight - QUARTZ_LOGO_HEIGHT) / 2 - 50;
  QuartzChrome::drawLogo(renderer, logoTop);
  renderer.drawCenteredText(SMALL_FONT_ID, logoTop + QUARTZ_LOGO_HEIGHT + 24, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 56, tr(STR_QZ_FORK_LINE));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
