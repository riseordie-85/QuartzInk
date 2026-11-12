#include "ScalableBuiltins.h"
#if CROSSINK_SCALABLE_FONTS
#include <GfxRenderer.h>
#include <Logging.h>

#include <optional>

#include "../../src/ReaderFontSizeStep.h"
#include "../../src/fontIds.h"
#include "HalScalableFont.h"
#include "ScalableAssets.generated.h"
namespace {
std::optional<HalScalableFont> faces[8];
bool ready[2] = {};
const uint8_t* const assets[] = {nvjost_regularOutline,    nvjost_boldOutline,      nvjost_italicOutline,
                                 nvjost_bolditalicOutline, libron_regularOutline,   libron_boldOutline,
                                 libron_italicOutline,     libron_bolditalicOutline};
constexpr size_t lengths[] = {sizeof(nvjost_regularOutline), sizeof(nvjost_boldOutline),
                              sizeof(nvjost_italicOutline),  sizeof(nvjost_bolditalicOutline),
                              sizeof(libron_regularOutline), sizeof(libron_boldOutline),
                              sizeof(libron_italicOutline),  sizeof(libron_bolditalicOutline)};
constexpr int ids[2][4] = {{NVJOST_10_FONT_ID, NVJOST_12_FONT_ID, NVJOST_14_FONT_ID, NVJOST_16_FONT_ID},
                           {LIBRON_10_FONT_ID, LIBRON_12_FONT_ID, LIBRON_14_FONT_ID, LIBRON_16_FONT_ID}};
}  // namespace
int scalableBuiltinFontId(int id) {
  for (unsigned f = 0; f < 2; ++f)
    for (unsigned s = 0; s < 4; ++s)
      if (ids[f][s] == id && !ready[f]) return UI_12_FONT_ID;
  const int result = int(uint32_t(id) ^ OutlineFingerprint ^ 0x53544600u ^ HalScalableFont::renderingRevision());
  return result ? result : 1;
}
int scalableBuiltinReaderFontId(unsigned family, unsigned points) {
  if (family >= 2 || !ready[family]) return UI_12_FONT_ID;
  // Preserve existing cache IDs for the original four sizes.
  for (unsigned s = 0; s < 4; ++s)
    if (points == 10 + s * 2) return scalableBuiltinFontId(ids[family][s]);
  return scalableBuiltinFontId(0x54540000 | (family << 8) | points);
}
void ensureScalableBuiltinFamily(GfxRenderer& renderer, unsigned family) {
  ScalableFontAccess access;
  if (family >= 2 || ready[family]) return;
  bool ok = true;
  for (unsigned style = 0; style < 4; ++style) {
    unsigned i = family * 4 + style;
    faces[i].emplace();
    if (!faces[i]->openMemory(assets[i], lengths[i])) {
      ok = false;
      break;
    }
  }
  if (ok) {
    for (uint8_t points = ScalableContentMinPointSize; points <= ScalableContentMaxPointSize && ok; ++points) {
      for (unsigned style = 0; style < 4; ++style) {
        if (!faces[family * 4 + style]->atSize(points)) {
          LOG_ERR("TTF", "Built-in size %u unavailable", unsigned(points));
          ok = false;
          break;
        }
      }
    }
  }
  if (!ok) {
    for (unsigned style = 0; style < 4; ++style) faces[family * 4 + style].reset();
    LOG_ERR("TTF", "Built-in family unavailable; using UI recovery font");
  }
  ready[family] = ok;
  for (uint8_t points = ScalableContentMinPointSize; points <= ScalableContentMaxPointSize; ++points) {
    if (ok) {
      EpdFontFamily font(faces[family * 4]->atSize(points), faces[family * 4 + 1]->atSize(points),
                         faces[family * 4 + 2]->atSize(points), faces[family * 4 + 3]->atSize(points));
      renderer.insertFont(scalableBuiltinReaderFontId(family, points), font);
      for (unsigned size = 0; size < 4; ++size)
        if (points == 10 + size * 2) renderer.insertFont(ids[family][size], font);
    } else {
      const auto it = renderer.getFontMap().find(UI_12_FONT_ID);
      if (it != renderer.getFontMap().end()) {
        for (unsigned size = 0; size < 4; ++size)
          if (points == 10 + size * 2) renderer.insertFont(ids[family][size], it->second);
      }
    }
  }
}
#endif
