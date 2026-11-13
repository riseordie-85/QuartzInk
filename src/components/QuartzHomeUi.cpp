#include "QuartzHomeUi.h"

#include <BoardConfig.h>
#include <FreeInkUIIcon.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "QuartzChrome.h"
#include "UITheme.h"
#include "components/icons/listIcons.h"
#include "components/icons/quartzIcons.h"
#include "stats/BookReadingStats.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId SELECT = 1;
constexpr int16_t GAP = 12;
constexpr int16_t BAR_HEIGHT = 56;
constexpr int16_t SHELF_ROW_HEIGHT = 54;
constexpr int16_t SHELF_GAP = 8;
constexpr int16_t RADIUS = 12;
constexpr int16_t BORDER = 3;
constexpr size_t MAX_SHELVES = 60;

const fui::Paint BLACK = fui::Paint::solid(fui::Color::Black);
const fui::Paint WHITE = fui::Paint::solid(fui::Color::White);

fui::TextStyle textStyle(const fui::FontId font, const fui::TextAlign align, const bool white = false,
                         const bool bold = false, const uint8_t maxLines = 1) {
  fui::TextStyle style;
  style.font = font;
  style.align = align;
  style.color = white ? fui::Color::White : fui::Color::Black;
  style.bold = bold;
  style.maxLines = maxLines;
  return style;
}

fui::Rect inset(const fui::Rect rect, const int16_t dx, const int16_t dy) {
  return fui::Rect{static_cast<int16_t>(rect.x + dx), static_cast<int16_t>(rect.y + dy),
                   static_cast<int16_t>(std::max<int>(0, rect.width - 2 * dx)),
                   static_cast<int16_t>(std::max<int>(0, rect.height - 2 * dy))};
}

bool lessIgnoringCase(const std::string& a, const std::string& b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](const char x, const char y) {
    return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y));
  });
}
}  // namespace

QuartzHomeUi::QuartzHomeUi(GfxRenderer& renderer) : UiAppHost(renderer), coverCache(renderer), renderer(renderer) {}

void QuartzHomeUi::begin(const std::vector<RecentBook>& recent, CurrentBook book, std::vector<std::string> shelfNames) {
  books = &recent;
  current = std::move(book);
  shelves = std::move(shelfNames);
  shelfPage = 0;
  // Button devices start on the current book, or on the shelves when nothing has been read.
  focus = !recent.empty() ? 0 : (!shelves.empty() ? SHELF_BASE : ACTION_LIBRARY);
  if (!recent.empty()) coverCache.begin();
  resetUi();
  app.on(SELECT, &QuartzHomeUi::onAction, this);
  app.setScreen(&QuartzHomeUi::screenFn, this);
  refreshCoverPaths();
}

// ---------------------------------------------------------------------------
// Shelves

std::vector<std::string> QuartzHomeUi::scanShelves() {
  std::vector<std::string> names;
  auto dir = Storage.open(SHELF_ROOT);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return names;
  }
  names.reserve(16);
  char name[128];
  for (auto entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      entry.getName(name, sizeof(name));
      if (name[0] != '\0' && name[0] != '.' && names.size() < MAX_SHELVES) names.emplace_back(name);
    }
    entry.close();
  }
  dir.close();
  std::sort(names.begin(), names.end(), lessIgnoringCase);
  return names;
}

std::string QuartzHomeUi::shelfForPath(const std::string& path) {
  const std::string root = std::string(SHELF_ROOT) + "/";
  const bool underRoot =
      path.size() > root.size() && std::equal(root.begin(), root.end(), path.begin(), [](const char a, const char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
      });
  if (underRoot) {
    const size_t end = path.find('/', root.size());
    return end == std::string::npos ? std::string{} : path.substr(root.size(), end - root.size());
  }
  // Elsewhere on the card: the folder the book is in.
  const size_t slash = path.rfind('/');
  if (slash == std::string::npos || slash == 0) return {};
  const size_t start = path.rfind('/', slash - 1);
  return path.substr(start == std::string::npos ? 0 : start + 1, slash - (start == std::string::npos ? 0 : start + 1));
}

int QuartzHomeUi::shelfPageCount() const {
  return std::max<int>(1, (static_cast<int>(shelves.size()) + SHELVES_PER_PAGE - 1) / SHELVES_PER_PAGE);
}

// ---------------------------------------------------------------------------
// Covers

void QuartzHomeUi::refreshCoverPaths() {
  coverCache.invalidate();
  if (!books) return;
  for (size_t i = 0; i < books->size() && i < coverPaths.size(); ++i) refreshCoverPath(i);
}

void QuartzHomeUi::refreshCoverPath(const size_t index) {
  if (!books || index >= books->size() || index >= coverPaths.size()) return;
  coverCache.invalidate(index);
  coverPaths[index] = thumbHeights[index] > 0
                          ? UITheme::getCoverThumbPath((*books)[index].coverBmpPath, thumbHeights[index])
                          : std::string();
}

int QuartzHomeUi::thumbHeightFor(const size_t index) const {
  return index < thumbHeights.size() && thumbHeights[index] > 0 ? thumbHeights[index] : 180;
}

bool QuartzHomeUi::takeThumbSizesChanged() { return std::exchange(thumbSizesChanged, false); }

void QuartzHomeUi::noteThumbHeight(const size_t index, const int height) {
  if (index >= thumbHeights.size()) return;
  const int h = std::max(1, height);
  if (thumbHeights[index] != h) {
    thumbHeights[index] = h;
    thumbSizesChanged = true;
    refreshCoverPath(index);
  }
}

void QuartzHomeUi::paintCover(fui::DrawTarget& target, const fui::Rect rect, const size_t index) {
  const bool drawn =
      books && index < books->size() && index < coverPaths.size() && coverCache.paint(rect, index, coverPaths[index]);
  if (!drawn) {
    // No cover image (yet): the title in a frame, so the slot still says which book it is.
    target.fill(rect, WHITE);
    if (books && index < books->size()) {
      target.text(inset(rect, 6, 6), (*books)[index].title.c_str(),
                  textStyle(fui::FONT_SLOT_SMALL, fui::TextAlign::Center, false, true, 5));
    }
  }
  target.stroke(rect, BLACK, 1, 0);
}

// ---------------------------------------------------------------------------
// Input

void QuartzHomeUi::onAction(const fui::ActionEvent& event, void* user) {
  auto& self = *static_cast<QuartzHomeUi*>(user);
  self.pending = event.value;
  self.app.clearTapFlash();
}

int QuartzHomeUi::buildRows(std::array<FocusRow, 8>& rows) const {
  int count = 0;
  const auto add = [&](std::initializer_list<int> items) {
    FocusRow row;
    for (const int item : items) row.items[row.count++] = item;
    rows[count++] = row;
  };
  add({ACTION_STATS, ACTION_SETTINGS});
  if (books && !books->empty()) add({0});
  if (books && books->size() > 1) {
    FocusRow row;
    for (int i = 1; i < static_cast<int>(books->size()) && i < MAX_BOOKS; ++i) row.items[row.count++] = i;
    rows[count++] = row;
  }
  const int first = shelfPage * SHELVES_PER_PAGE;
  const int last = std::min<int>(static_cast<int>(shelves.size()), first + SHELVES_PER_PAGE);
  for (int i = first; i < last; i += 2) {
    FocusRow row;
    row.items[row.count++] = SHELF_BASE + i;
    if (i + 1 < last) row.items[row.count++] = SHELF_BASE + i + 1;
    rows[count++] = row;
  }
  add({ACTION_FILES, ACTION_LIBRARY, ACTION_TRANSFER});
  return count;
}

void QuartzHomeUi::moveFocus(const int rowDelta, const int colDelta) {
  std::array<FocusRow, 8> rows;
  int rowCount = buildRows(rows);
  int r = -1;
  int c = 0;
  for (int i = 0; i < rowCount && r < 0; ++i) {
    for (int j = 0; j < rows[i].count; ++j) {
      if (rows[i].items[j] == focus) {
        r = i;
        c = j;
        break;
      }
    }
  }
  if (r < 0) {
    focus = rows[0].items[0];
    return;
  }
  if (colDelta != 0) {
    const int n = rows[r].count;
    focus = rows[r].items[(c + colDelta + n) % n];
    return;
  }
  const bool onShelf = focus >= SHELF_BASE;
  const int target = r + rowDelta;
  const bool leavingShelves = onShelf && (target < 0 || target >= rowCount || rows[target].items[0] < SHELF_BASE);
  // Moving past the last shelf row turns to the next page of shelves, and back.
  if (leavingShelves && rowDelta > 0 && shelfPage + 1 < shelfPageCount()) {
    ++shelfPage;
    focus = SHELF_BASE + shelfPage * SHELVES_PER_PAGE + std::min(c, 1);
    if (focus - SHELF_BASE >= static_cast<int>(shelves.size())) focus = SHELF_BASE + shelfPage * SHELVES_PER_PAGE;
    return;
  }
  if (leavingShelves && rowDelta < 0 && shelfPage > 0) {
    --shelfPage;
    rowCount = buildRows(rows);
    for (int i = rowCount - 1; i >= 0; --i) {
      if (rows[i].items[0] >= SHELF_BASE) {
        focus = rows[i].items[std::min(c, rows[i].count - 1)];
        return;
      }
    }
  }
  const int next = (target + rowCount) % rowCount;
  // Keep roughly the same horizontal position across rows of different widths.
  const int fromCount = rows[r].count;
  const int toCount = rows[next].count;
  const int col = fromCount <= 1 ? 0 : std::min(toCount - 1, (c * toCount + fromCount / 2) / fromCount);
  focus = rows[next].items[std::max(0, col)];
}

int QuartzHomeUi::handleInput(MappedInputManager& input, bool& redraw) {
  redraw = false;
  pending = -1;
  const auto touch = routeTouch(input);
  if (touch.snap.touchReleased && pending >= 0) return pending;

  // Swipe sideways to page through shelves.
  const auto swipe = input.wasSwipe();
  if ((swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right) &&
      shelfPageCount() > 1) {
    const int pages = shelfPageCount();
    shelfPage = (shelfPage + (swipe == MappedInputManager::SwipeDir::Left ? 1 : pages - 1)) % pages;
    if (focus >= SHELF_BASE) focus = SHELF_BASE + shelfPage * SHELVES_PER_PAGE;
    redraw = true;
    return -1;
  }

  const bool up = input.wasReleased(MappedInputManager::Button::Up);
  const bool down = input.wasReleased(MappedInputManager::Button::Down);
  const bool left = input.wasReleased(MappedInputManager::Button::Left);
  const bool right = input.wasReleased(MappedInputManager::Button::Right);
  if (up || down) {
    moveFocus(down ? 1 : -1, 0);
    redraw = true;
  } else if (left || right) {
    moveFocus(0, right ? 1 : -1);
    redraw = true;
  } else if (input.wasReleased(MappedInputManager::Button::Confirm)) {
    return focus;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Drawing

void QuartzHomeUi::screenFn(UiScreen& screen, void* user) { static_cast<QuartzHomeUi*>(user)->draw(screen); }

void QuartzHomeUi::draw(UiScreen& screen) {
  coverCache.prepare();
  const auto safe = UITheme::getInstance().getScreenSafeArea(renderer, !BoardConfig::hasTouch());
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y), static_cast<int16_t>(renderer.getScreenWidth() - safe.x - safe.width),
      static_cast<int16_t>(renderer.getScreenHeight() - safe.y - safe.height), static_cast<int16_t>(safe.x)});
  screen.insetContent(fui::Insets{GAP, GAP, GAP, GAP});

  const fui::Rect top = screen.takeTop(BAR_HEIGHT, GAP);
  const fui::Rect bottom = screen.takeBottom(BAR_HEIGHT, GAP);
  const int16_t pageLine = shelfPageCount() > 1 ? 22 : 0;
  const int16_t shelvesH = static_cast<int16_t>(3 * SHELF_ROW_HEIGHT + 2 * SHELF_GAP + pageLine);
  const fui::Rect shelfRect = screen.takeBottom(shelvesH, GAP);

  const fui::Rect body = screen.body();
  const bool hasCovers = books && books->size() > 1;
  const int16_t currentH = hasCovers ? static_cast<int16_t>(std::clamp<int>(body.height * 56 / 100, 150, 260))
                                     : static_cast<int16_t>(std::min<int>(body.height, 260));
  const fui::Rect currentRect{body.x, body.y, body.width, currentH};
  const fui::Rect coversRect{body.x, static_cast<int16_t>(body.y + currentH + GAP), body.width,
                             static_cast<int16_t>(std::max<int>(0, body.height - currentH - GAP))};

  drawTopBar(screen, top);
  if (books && !books->empty()) {
    drawCurrent(screen, currentRect);
  } else {
    drawEmptyBook(screen, body);
  }
  if (hasCovers) drawCovers(screen, coversRect);
  drawShelves(screen, shelfRect);
  drawBottomBar(screen, bottom);
}

void QuartzHomeUi::drawTopBar(UiScreen& screen, const fui::Rect rect) {
  auto& target = screen.target();
  target.fill(rect, BLACK, RADIUS);

  char clock[16] = "";
  if (halClock.isAvailable()) {
    halClock.formatTime(clock, sizeof(clock), SETTINGS.clockFormat == 1);
  }
  target.text(fui::Rect{static_cast<int16_t>(rect.x + 20), rect.y, static_cast<int16_t>(rect.width / 2), rect.height},
              clock, textStyle(fui::FONT_SLOT_BODY, fui::TextAlign::Left, true));
  // Battery after the time, white on the bar.
  const auto clockStyle = textStyle(fui::FONT_SLOT_BODY, fui::TextAlign::Left, true);
  const int clockW = clock[0] != '\0' ? target.measureText(clockStyle.font, clock, clockStyle).width + 16 : 0;
  // Battery after the time; drawBatteryOnBlack anchors by its right edge.
  QuartzChrome::drawBatteryOnBlack(renderer, rect.x + 20 + clockW + QuartzChrome::batteryOnBlackWidth(renderer),
                                   rect.y + rect.height / 2);

  const bool showFocus = !BoardConfig::hasTouch();
  const auto icon = [&](const fui::Rect slot, const freeink::Icon& glyph, const int action) {
    target.bitmap(slot, fui::bitmapFromIcon(glyph), fui::BitmapMode::Center, WHITE);
    if (showFocus && focus == action) target.stroke(inset(slot, 2, 2), WHITE, BORDER, 8);
    screen.frame().hit(slot, SELECT, static_cast<int16_t>(action));
  };
  const int16_t slot = 52;
  icon(fui::Rect{static_cast<int16_t>(rect.right() - 2 * slot - 6), static_cast<int16_t>(rect.y + 2), slot,
                 static_cast<int16_t>(rect.height - 4)},
       qz_reading_stats_32, ACTION_STATS);
  icon(fui::Rect{static_cast<int16_t>(rect.right() - slot - 6), static_cast<int16_t>(rect.y + 2), slot,
                 static_cast<int16_t>(rect.height - 4)},
       qz_sliders_horizontal_32, ACTION_SETTINGS);
}

void QuartzHomeUi::drawEmptyBook(UiScreen& screen, const fui::Rect rect) {
  auto& target = screen.target();
  const int16_t titleH = target.lineHeight(fui::FONT_SLOT_BODY);
  const int16_t y = static_cast<int16_t>(rect.y + std::max(0, rect.height / 2 - titleH));
  target.text(fui::Rect{rect.x, y, rect.width, titleH}, tr(STR_NO_OPEN_BOOK),
              textStyle(fui::FONT_SLOT_BODY, fui::TextAlign::Center, false, true));
  target.text(
      fui::Rect{rect.x, static_cast<int16_t>(y + titleH + 4), rect.width, target.lineHeight(fui::FONT_SLOT_SMALL)},
      tr(STR_QZ_START_FROM_SHELF), textStyle(fui::FONT_SLOT_SMALL, fui::TextAlign::Center));
}

void QuartzHomeUi::drawCurrent(UiScreen& screen, const fui::Rect rect) {
  auto& target = screen.target();
  const RecentBook& book = books->front();
  target.fill(rect, WHITE, RADIUS);
  target.stroke(rect, BLACK, BORDER, RADIUS);
  if (!BoardConfig::hasTouch() && focus == 0) target.stroke(inset(rect, -5, -5), BLACK, BORDER, RADIUS + 4);
  screen.frame().hit(rect, SELECT, 0);

  const fui::Rect inner = inset(rect, 12, 12);
  const int16_t coverH = inner.height;
  const int16_t coverW = static_cast<int16_t>(std::min<int>(coverH * 2 / 3, inner.width * 2 / 5));
  noteThumbHeight(0, coverH);
  paintCover(target, fui::Rect{inner.x, inner.y, coverW, coverH}, 0);

  const int16_t textX = static_cast<int16_t>(inner.x + coverW + 14);
  const int16_t textW = static_cast<int16_t>(inner.right() - textX);
  const int16_t bodyH = target.lineHeight(fui::FONT_SLOT_BODY);
  const int16_t smallH = target.lineHeight(fui::FONT_SLOT_SMALL);
  constexpr int16_t LINE_GAP = 2;
  constexpr int16_t BAR_H = 14;

  // Progress and time sit at the bottom of the card; measure them first so the
  // title block above can never run into them.
  char line[96];
  char duration[32];
  const bool hasProgress = current.progressPercent >= 0.0f;
  const bool hasTaken = current.readingSeconds > 0;
  const bool hasLeft = current.timeLeftSeconds > 0;
  const int smallLines = (hasProgress ? 1 : 0) + (hasTaken ? 1 : 0) + (hasLeft ? 1 : 0);
  const int16_t stackH = static_cast<int16_t>(smallLines * (smallH + LINE_GAP) + (hasProgress ? BAR_H + 6 : 0));
  const int16_t stackTop = static_cast<int16_t>(inner.bottom() - stackH);

  // Title, author and shelf, shrinking until they fit above the stack: a
  // two-line title, then one line, then author and shelf in the small font.
  const bool hasAuthor = !book.author.empty();
  const bool hasShelf = !current.shelf.empty();
  const auto titleStyle = textStyle(fui::FONT_SLOT_BODY, fui::TextAlign::Left, false, true, 2);
  const bool longTitle = target.measureText(fui::FONT_SLOT_BODY, book.title.c_str(), titleStyle).width > textW;
  const int16_t room = static_cast<int16_t>(stackTop - inner.y);
  const int metaLines = (hasAuthor ? 1 : 0) + (hasShelf ? 1 : 0);
  int titleLines = longTitle ? 2 : 1;
  if (titleLines == 2 && (2 + metaLines) * bodyH > room) titleLines = 1;
  const bool smallMeta = (titleLines + metaLines) * bodyH > room;
  const fui::FontId metaFont = smallMeta ? fui::FONT_SLOT_SMALL : fui::FONT_SLOT_BODY;
  const int16_t metaH = smallMeta ? smallH : bodyH;

  int16_t y = inner.y;
  const int16_t titleH = static_cast<int16_t>(bodyH * titleLines);
  target.text(fui::Rect{textX, y, textW, titleH}, book.title.c_str(),
              textStyle(fui::FONT_SLOT_BODY, fui::TextAlign::Left, false, true, static_cast<uint8_t>(titleLines)));
  y = static_cast<int16_t>(y + titleH);
  if (hasAuthor && y + metaH <= stackTop) {
    target.text(fui::Rect{textX, y, textW, metaH}, book.author.c_str(), textStyle(metaFont, fui::TextAlign::Left));
    y = static_cast<int16_t>(y + metaH);
  }
  if (hasShelf && y + metaH <= stackTop) {
    std::snprintf(line, sizeof(line), "%s: %s", tr(STR_QZ_SHELF), current.shelf.c_str());
    target.text(fui::Rect{textX, y, textW, metaH}, line, textStyle(metaFont, fui::TextAlign::Left));
  }

  // Top to bottom: Completion, its bar, Time Taken, Time Left.
  int16_t by = stackTop;
  const auto stackLine = [&](const char* text) {
    target.text(fui::Rect{textX, by, textW, smallH}, text, textStyle(fui::FONT_SLOT_SMALL, fui::TextAlign::Left));
    by = static_cast<int16_t>(by + smallH + LINE_GAP);
  };
  if (hasProgress) {
    const float percent = std::clamp(current.progressPercent, 0.0f, 100.0f);
    std::snprintf(line, sizeof(line), "%s: %d%%", tr(STR_QZ_COMPLETION), static_cast<int>(std::lround(percent)));
    stackLine(line);
    const fui::Rect bar{textX, by, textW, BAR_H};
    target.stroke(bar, BLACK, 2, 0);
    const int16_t filled = static_cast<int16_t>(std::lround((bar.width - 4) * percent / 100.0f));
    if (filled > 0) {
      target.fill(fui::Rect{static_cast<int16_t>(bar.x + 2), static_cast<int16_t>(bar.y + 2), filled,
                            static_cast<int16_t>(BAR_H - 4)},
                  BLACK);
    }
    by = static_cast<int16_t>(by + BAR_H + 6);
  }
  if (hasTaken) {
    BookReadingStats::formatDuration(current.readingSeconds, duration, sizeof(duration));
    std::snprintf(line, sizeof(line), "%s: %s", tr(STR_QZ_TIME_TAKEN), duration);
    stackLine(line);
  }
  if (hasLeft) {
    BookReadingStats::formatDuration(current.timeLeftSeconds, duration, sizeof(duration));
    std::snprintf(line, sizeof(line), "%s: %s", tr(STR_QZ_TIME_LEFT), duration);
    stackLine(line);
  }
}

void QuartzHomeUi::drawCovers(UiScreen& screen, const fui::Rect rect) {
  auto& target = screen.target();
  const int count = std::min<int>(3, static_cast<int>(books->size()) - 1);
  if (count <= 0 || rect.height < 40) return;
  // Three slots across, so a single recent book does not stretch.
  const int16_t slotW = static_cast<int16_t>((rect.width - 2 * GAP) / 3);
  const int16_t coverH = static_cast<int16_t>(std::min<int>(rect.height, slotW * 3 / 2));
  const int16_t coverW = static_cast<int16_t>(std::min<int>(slotW, coverH * 2 / 3));
  for (int i = 0; i < count; ++i) {
    const int index = i + 1;
    const int16_t slotX = static_cast<int16_t>(rect.x + i * (slotW + GAP));
    const fui::Rect cover{static_cast<int16_t>(slotX + (slotW - coverW) / 2),
                          static_cast<int16_t>(rect.y + (rect.height - coverH) / 2), coverW, coverH};
    noteThumbHeight(static_cast<size_t>(index), coverH);
    paintCover(target, cover, static_cast<size_t>(index));
    if (!BoardConfig::hasTouch() && focus == index) target.stroke(inset(cover, -6, -6), BLACK, BORDER + 1, 6);
    screen.frame().hit(cover, SELECT, static_cast<int16_t>(index));
  }
}

void QuartzHomeUi::drawShelves(UiScreen& screen, const fui::Rect rect) {
  auto& target = screen.target();
  if (shelves.empty()) {
    target.text(rect, tr(STR_QZ_NO_SHELVES), textStyle(fui::FONT_SLOT_SMALL, fui::TextAlign::Center, false, false, 3));
    return;
  }
  const bool showFocus = !BoardConfig::hasTouch();
  const int16_t cellW = static_cast<int16_t>((rect.width - SHELF_GAP) / 2);
  const int first = shelfPage * SHELVES_PER_PAGE;
  for (int slot = 0; slot < SHELVES_PER_PAGE; ++slot) {
    const int index = first + slot;
    if (index >= static_cast<int>(shelves.size())) break;
    const fui::Rect cell{static_cast<int16_t>(rect.x + (slot % 2) * (cellW + SHELF_GAP)),
                         static_cast<int16_t>(rect.y + (slot / 2) * (SHELF_ROW_HEIGHT + SHELF_GAP)), cellW,
                         SHELF_ROW_HEIGHT};
    // The X3's focus is the button filled black with white text.
    const bool focused = showFocus && focus == SHELF_BASE + index;
    target.fill(cell, focused ? BLACK : WHITE, 10);
    target.stroke(cell, BLACK, BORDER, 10);
    target.text(inset(cell, 8, 0), shelves[static_cast<size_t>(index)].c_str(),
                textStyle(fui::FONT_SLOT_TITLE, fui::TextAlign::Center, focused));
    screen.frame().hit(cell, SELECT, static_cast<int16_t>(SHELF_BASE + index));
  }
  const int pages = shelfPageCount();
  if (pages > 1) {
    char page[24];
    std::snprintf(page, sizeof(page), "%d / %d", shelfPage + 1, pages);
    target.text(fui::Rect{rect.x, static_cast<int16_t>(rect.bottom() - 20), rect.width, 20}, page,
                textStyle(fui::FONT_SLOT_SMALL, fui::TextAlign::Center));
  }
}

void QuartzHomeUi::drawBottomBar(UiScreen& screen, const fui::Rect rect) {
  auto& target = screen.target();
  const bool showFocus = !BoardConfig::hasTouch();
  const int16_t cellW = static_cast<int16_t>((rect.width - 2 * GAP) / 3);
  static constexpr const freeink::Icon* ICONS[] = {&icon_folder_32, &qz_library_32, &qz_transfer_32};
  static constexpr int ACTIONS[] = {ACTION_FILES, ACTION_LIBRARY, ACTION_TRANSFER};
  for (int i = 0; i < 3; ++i) {
    const fui::Rect cell{static_cast<int16_t>(rect.x + i * (cellW + GAP)), rect.y, cellW, rect.height};
    target.fill(cell, BLACK, 10);
    target.bitmap(cell, fui::bitmapFromIcon(*ICONS[i]), fui::BitmapMode::Center, WHITE);
    if (showFocus && focus == ACTIONS[i]) target.stroke(inset(cell, 5, 5), WHITE, BORDER, 8);
    screen.frame().hit(cell, SELECT, static_cast<int16_t>(ACTIONS[i]));
  }
}
