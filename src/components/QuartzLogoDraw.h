#pragma once

class GfxRenderer;

// QuartzChrome::drawLogo on its own, for files that cannot include FreeInkUI
// headers (SleepActivity includes PNGdec, whose zutil.h defines `local`).
namespace QuartzChrome {
// The QuartzInk gem logo and wordmark, centered horizontally with its top at
// `y`. Returns the logo's height.
int drawLogo(const GfxRenderer& renderer, int y);
// The full-screen QuartzInk boot art, covering the screen (cropped from the
// center when the panel's shape differs).
void drawBootArt(const GfxRenderer& renderer);
}  // namespace QuartzChrome
