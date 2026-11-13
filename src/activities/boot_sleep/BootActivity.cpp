#include "BootActivity.h"

#include <GfxRenderer.h>

#include "components/QuartzLogoDraw.h"
#include "fontIds.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  // QuartzInk: the full-screen boot art, with the version in white inside its
  // black circle (centered on the screen, below "A CrossPoint Project").
  renderer.clearScreen();
  QuartzChrome::drawBootArt(renderer);
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() / 2 + 120, CROSSPOINT_VERSION, false);
  renderer.displayBuffer();
}
