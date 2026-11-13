#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "stats/DailyReadingLog.h"
#include "stats/GlobalReadingStats.h"

namespace {
constexpr int MARGIN = 10;
constexpr int GAP = 10;
constexpr int CARD_PAD = 14;
constexpr int CARD_RADIUS = 14;
constexpr int CARD_LINE = 3;
constexpr int BAR_HEIGHT = 46;
constexpr int CELL = 14;
constexpr int CELL_GAP = 4;

constexpr int TITLE_FONT = UI_12_FONT_ID;
constexpr int BODY_FONT = UI_10_FONT_ID;
constexpr int VALUE_FONT = NOTOSANS_18_FONT_ID;

// "42 min", "4h 10m", "128h": short enough for a half-width tile.
void formatDuration(const uint32_t seconds, char* out, const size_t len) {
  const uint32_t minutes = seconds / 60;
  if (minutes < 60) {
    snprintf(out, len, "%lu min", static_cast<unsigned long>(minutes));
  } else if (minutes < 100 * 60) {
    snprintf(out, len, "%luh %02lum", static_cast<unsigned long>(minutes / 60),
             static_cast<unsigned long>(minutes % 60));
  } else {
    snprintf(out, len, "%luh", static_cast<unsigned long>(minutes / 60));
  }
}

// 41380 -> "41,380"
void formatCount(const uint32_t value, char* out, const size_t len) {
  char digits[12];
  const int n = snprintf(digits, sizeof(digits), "%lu", static_cast<unsigned long>(value));
  size_t o = 0;
  for (int i = 0; i < n && o + 1 < len; ++i) {
    if (i > 0 && (n - i) % 3 == 0 && o + 1 < len) out[o++] = ',';
    if (o + 1 < len) out[o++] = digits[i];
  }
  out[o] = '\0';
}

void formatDays(const uint16_t days, char* out, const size_t len) {
  snprintf(out, len, "%u %s", static_cast<unsigned>(days), days == 1 ? tr(STR_QZ_DAY) : tr(STR_QZ_DAYS));
}

ReadingStatsActivity::Shade shadeFor(const uint32_t seconds) {
  const uint32_t minutes = seconds / 60;
  if (minutes == 0) return ReadingStatsActivity::None;
  if (minutes < 15) return ReadingStatsActivity::Light;
  if (minutes < 45) return ReadingStatsActivity::Medium;
  return ReadingStatsActivity::Full;
}

int bodyLine(const GfxRenderer& renderer) { return renderer.getLineHeight(BODY_FONT); }

void drawCard(const GfxRenderer& renderer, const int x, const int y, const int w, const int h) {
  renderer.drawRoundedRect(x, y, w, h, CARD_LINE, CARD_RADIUS, true);
}

// Label left, value right, on one body line.
void drawRow(const GfxRenderer& renderer, const int x, const int y, const int w, const char* label, const char* value) {
  renderer.drawText(BODY_FONT, x, y, label);
  const int vw = renderer.getTextWidth(BODY_FONT, value, EpdFontFamily::BOLD);
  renderer.drawText(BODY_FONT, x + w - vw, y, value, true, EpdFontFamily::BOLD);
}
}  // namespace

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  page = 0;

  const GlobalReadingStats global = GlobalReadingStats::load();
  totalSeconds = global.totalReadingSeconds;
  totalPages = global.totalPagesTurned;
  totalSessions = global.totalSessions;
  timeOfDaySeconds = global.timeOfDaySeconds;
  bestStreakDays = global.displayLongestReadingStreak();

  ReadingStatsDateTime now;
  const bool dated = getCurrentLocalReadingStatsDateTime(now);
  streakDays = global.currentReadingStreak(dated ? &now.date : nullptr);
  currentYear = dated ? now.date.year : 0;

  calendar.fill(Future);
  if (dated) {
    DailyReadingLog daily;
    daily.load();  // an empty log on failure; the calendar then uses history only
    const uint32_t today = readingStatsDayIndex(now.date);
    const uint32_t weekStart = today - readingStatsDayOfWeekIndex(now.date);  // Monday
    todaySeconds = daily.secondsOn(today);
    weekSeconds = daily.secondsBetween(weekStart, today);
    const uint32_t yearStart = readingStatsDayIndex(ReadingStatsDate{now.date.year, 1, 1});
    yearSeconds = daily.secondsBetween(yearStart, today);

    // Last cell column is this week; days CrossInk marked before the daily
    // log existed show as the lightest shade.
    const uint32_t firstDay = weekStart - (MAX_WEEKS - 1) * 7;
    for (int i = 0; i < MAX_WEEKS * 7; ++i) {
      const uint32_t day = firstDay + static_cast<uint32_t>(i);
      if (day > today) break;
      Shade shade = shadeFor(daily.secondsOn(day));
      if (shade == None && readingHistoryHasDay(global.readingHistoryAnchorDay, global.readingHistoryBits, day)) {
        shade = Light;
      }
      calendar[i] = shade;
    }
  }

  finished = finishedbooks::summarize(currentYear, recent.data(), recent.size());
  // CrossInk counted finishes before this log existed.
  finishedAll = std::max<uint32_t>(global.completedBooks, finished.total);

  hasData = totalSeconds > 0 || finishedAll > 0;
  requestUpdate();
}

void ReadingStatsActivity::turnPage(const int delta) {
  const int next = std::clamp(page + delta, 0, PAGE_COUNT - 1);
  if (next == page) return;
  page = next;
  requestUpdate();
}

void ReadingStatsActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back) || mappedInput.wasBackGesture()) {
    activityManager.goHome();
    return;
  }
  if (mappedInput.wasReleased(Button::Right) || mappedInput.wasReleased(Button::Down) ||
      mappedInput.wasReleased(Button::PageForward)) {
    turnPage(1);
    return;
  }
  if (mappedInput.wasReleased(Button::Left) || mappedInput.wasReleased(Button::Up) ||
      mappedInput.wasReleased(Button::PageBack)) {
    turnPage(-1);
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Up) {
    turnPage(1);
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Right || swipe == MappedInputManager::SwipeDir::Down) {
    turnPage(-1);
    return;
  }
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) turnPage(x >= renderer.getScreenWidth() / 2 ? 1 : -1);
}

void ReadingStatsActivity::renderTitleBar(const int x, const int y, const int w, const int h) const {
  renderer.fillRoundedRect(x, y, w, h, CARD_RADIUS, Color::Black);
  const int textY = y + (h - renderer.getLineHeight(TITLE_FONT)) / 2;
  renderer.drawText(TITLE_FONT, x + CARD_PAD, textY, tr(STR_READING_STATS), false, EpdFontFamily::BOLD);

  char status[32];
  char clock[12] = "";
  if (halClock.isAvailable()) halClock.formatTime(clock, sizeof(clock), SETTINGS.clockFormat == 1);
  snprintf(status, sizeof(status), "%s%s%u%%", clock, clock[0] ? "  " : "",
           static_cast<unsigned>(powerManager.getBatteryPercentage()));
  const int sw = renderer.getTextWidth(BODY_FONT, status);
  renderer.drawText(BODY_FONT, x + w - CARD_PAD - sw, y + (h - bodyLine(renderer)) / 2, status, false);
}

void ReadingStatsActivity::renderSummaryPage(const int x, int y, const int w, const int bottom) const {
  const int line = bodyLine(renderer);
  char value[24];

  // Summary tiles, two by two.
  const int tileW = (w - GAP) / 2;
  const int tileH = CARD_PAD + line + 4 + renderer.getLineHeight(VALUE_FONT) + CARD_PAD - 4;
  const char* labels[4] = {tr(STR_QZ_TODAY), tr(STR_QZ_THIS_WEEK), tr(STR_QZ_THIS_YEAR), tr(STR_QZ_STREAK)};
  for (int i = 0; i < 4; ++i) {
    const int tx = x + (i % 2) * (tileW + GAP);
    const int ty = y + (i / 2) * (tileH + GAP);
    drawCard(renderer, tx, ty, tileW, tileH);
    renderer.drawText(BODY_FONT, tx + CARD_PAD, ty + CARD_PAD - 2, labels[i]);
    if (i == 0) formatDuration(todaySeconds, value, sizeof(value));
    if (i == 1) formatDuration(weekSeconds, value, sizeof(value));
    if (i == 2) formatDuration(yearSeconds, value, sizeof(value));
    if (i == 3) formatDays(streakDays, value, sizeof(value));
    renderer.drawText(VALUE_FONT, tx + CARD_PAD, ty + CARD_PAD + line, value, true, EpdFontFamily::BOLD);
  }
  y += 2 * tileH + 2 * GAP;

  // Reading Days calendar.
  const int inner = w - 2 * CARD_PAD;
  const int weeks = std::clamp((inner + CELL_GAP) / (CELL + CELL_GAP), 1, MAX_WEEKS);
  const int gridH = 7 * CELL + 6 * CELL_GAP;
  const int calH = CARD_PAD + line + 8 + gridH + 8 + line + CARD_PAD - 4;
  drawCard(renderer, x, y, w, calH);
  renderer.drawText(TITLE_FONT, x + CARD_PAD, y + CARD_PAD - 4, tr(STR_QZ_READING_DAYS), true, EpdFontFamily::BOLD);
  const int gridX = x + CARD_PAD;
  const int gridY = y + CARD_PAD + line + 8;
  const int firstWeek = MAX_WEEKS - weeks;
  for (int week = 0; week < weeks; ++week) {
    for (int day = 0; day < 7; ++day) {
      const uint8_t shade = calendar[(firstWeek + week) * 7 + day];
      const int cx = gridX + week * (CELL + CELL_GAP);
      const int cy = gridY + day * (CELL + CELL_GAP);
      switch (shade) {
        case Future:
          break;
        case None:
          renderer.drawRoundedRect(cx, cy, CELL, CELL, 1, 3, true);
          break;
        case Light:
          renderer.fillRoundedRect(cx, cy, CELL, CELL, 3, Color::LightGray);
          break;
        case Medium:
          renderer.fillRoundedRect(cx, cy, CELL, CELL, 3, Color::DarkGray);
          break;
        default:
          renderer.fillRoundedRect(cx, cy, CELL, CELL, 3, Color::Black);
          break;
      }
    }
  }
  // Legend: "Best streak N days" left, "Less [][][][] More" right.
  const int legendY = gridY + gridH + 8;
  char best[40];
  char days[16];
  formatDays(bestStreakDays, days, sizeof(days));
  snprintf(best, sizeof(best), "%s %s", tr(STR_QZ_BEST_STREAK), days);
  renderer.drawText(SMALL_FONT_ID, gridX, legendY, best);
  const int swatch = 10;
  const int moreW = renderer.getTextWidth(SMALL_FONT_ID, tr(STR_QZ_MORE));
  int lx = x + w - CARD_PAD - moreW;
  renderer.drawText(SMALL_FONT_ID, lx, legendY, tr(STR_QZ_MORE));
  const Color swatches[3] = {Color::Black, Color::DarkGray, Color::LightGray};
  const int swatchY = legendY + (renderer.getLineHeight(SMALL_FONT_ID) - swatch) / 2;
  for (const Color c : swatches) {
    lx -= swatch + 4;
    renderer.fillRoundedRect(lx, swatchY, swatch, swatch, 2, c);
  }
  lx -= swatch + 4;
  renderer.drawRoundedRect(lx, swatchY, swatch, swatch, 1, 2, true);
  lx -= renderer.getTextWidth(SMALL_FONT_ID, tr(STR_QZ_LESS)) + 6;
  renderer.drawText(SMALL_FONT_ID, lx, legendY, tr(STR_QZ_LESS));
  y += calH + GAP;

  // Totals.
  const int rowH = line + 6;
  const int totalsH = std::min(bottom - y, CARD_PAD + line + 6 + 4 * rowH + CARD_PAD - 6);
  if (totalsH < CARD_PAD * 2 + line) return;
  drawCard(renderer, x, y, w, totalsH);
  renderer.drawText(TITLE_FONT, x + CARD_PAD, y + CARD_PAD - 4, tr(STR_QZ_TOTALS), true, EpdFontFamily::BOLD);
  int ry = y + CARD_PAD + line + 6;
  formatDuration(totalSeconds, value, sizeof(value));
  drawRow(renderer, x + CARD_PAD, ry, inner, tr(STR_QZ_READING_TIME), value);
  ry += rowH;
  formatCount(totalPages, value, sizeof(value));
  drawRow(renderer, x + CARD_PAD, ry, inner, tr(STR_QZ_PAGES_TURNED), value);
  ry += rowH;
  formatCount(totalSessions, value, sizeof(value));
  drawRow(renderer, x + CARD_PAD, ry, inner, tr(STR_QZ_SESSIONS), value);
  ry += rowH;
  formatCount(finishedAll, value, sizeof(value));
  drawRow(renderer, x + CARD_PAD, ry, inner, tr(STR_QZ_BOOKS_FINISHED), value);
}

void ReadingStatsActivity::renderFinishedPage(const int x, int y, const int w, const int bottom) const {
  const int line = bodyLine(renderer);
  const int small = renderer.getLineHeight(SMALL_FONT_ID);
  const int inner = w - 2 * CARD_PAD;
  char text[64];

  // Finished by month.
  const int barArea = 110;
  const int finH = CARD_PAD + line + 4 + line + 8 + small + barArea + small + CARD_PAD - 4;
  drawCard(renderer, x, y, w, finH);
  renderer.drawText(TITLE_FONT, x + CARD_PAD, y + CARD_PAD - 4, tr(STR_QZ_FINISHED), true, EpdFontFamily::BOLD);
  snprintf(text, sizeof(text), tr(STR_QZ_FINISHED_SUMMARY), static_cast<unsigned>(finished.finishedInYear),
           static_cast<unsigned>(finishedAll));
  renderer.drawText(BODY_FONT, x + CARD_PAD, y + CARD_PAD + line, text);
  const int most = std::max<int>(1, *std::max_element(finished.byMonth, finished.byMonth + 12));
  const int colGap = 6;
  const int colW = (inner - 11 * colGap) / 12;
  const int baseY = y + CARD_PAD + 2 * line + 8 + small + barArea;
  const char* letters = tr(STR_QZ_MONTH_LETTERS);
  for (int m = 0; m < 12; ++m) {
    const int cx = x + CARD_PAD + m * (colW + colGap);
    const int count = finished.byMonth[m];
    const int barH = count > 0 ? std::max(6, barArea * count / most) : 4;
    if (count > 0) {
      renderer.fillRoundedRect(cx, baseY - barH, colW, barH, 4, Color::Black);
      snprintf(text, sizeof(text), "%d", count);
      const int tw = renderer.getTextWidth(SMALL_FONT_ID, text);
      renderer.drawText(SMALL_FONT_ID, cx + (colW - tw) / 2, baseY - barH - small, text);
    } else {
      renderer.fillRoundedRect(cx, baseY - barH, colW, barH, 2, Color::LightGray);
    }
    const char letter[2] = {letters[m] ? letters[m] : ' ', '\0'};
    const int lw = renderer.getTextWidth(SMALL_FONT_ID, letter);
    renderer.drawText(SMALL_FONT_ID, cx + (colW - lw) / 2, baseY + 2, letter);
  }
  y += finH + GAP;

  // When You Read.
  const int rowH = line + 10;
  const int whenH = CARD_PAD + line + 8 + 4 * rowH + CARD_PAD - 10;
  drawCard(renderer, x, y, w, whenH);
  renderer.drawText(TITLE_FONT, x + CARD_PAD, y + CARD_PAD - 4, tr(STR_QZ_WHEN_YOU_READ), true, EpdFontFamily::BOLD);
  const char* buckets[4] = {tr(STR_QZ_MORNING), tr(STR_QZ_AFTERNOON), tr(STR_QZ_EVENING), tr(STR_QZ_NIGHT)};
  const uint32_t most4 = std::max<uint32_t>(1, *std::max_element(timeOfDaySeconds.begin(), timeOfDaySeconds.end()));
  int labelW = 0;
  for (const char* b : buckets) labelW = std::max(labelW, renderer.getTextWidth(BODY_FONT, b));
  const int valueW = renderer.getTextWidth(BODY_FONT, "000h 00m");
  const int trackX = x + CARD_PAD + labelW + 12;
  const int trackW = inner - labelW - 12 - valueW - 12;
  int ry = y + CARD_PAD + line + 8;
  for (int i = 0; i < 4; ++i) {
    renderer.drawText(BODY_FONT, x + CARD_PAD, ry, buckets[i]);
    const int trackH = 14;
    const int ty = ry + (line - trackH) / 2;
    renderer.drawRoundedRect(trackX, ty, trackW, trackH, 2, 7, true);
    const int fillW = static_cast<int>(static_cast<uint64_t>(trackW) * timeOfDaySeconds[i] / most4);
    if (fillW > 4) renderer.fillRoundedRect(trackX, ty, fillW, trackH, 7, Color::Black);
    formatDuration(timeOfDaySeconds[i], text, sizeof(text));
    const int tw = renderer.getTextWidth(BODY_FONT, text);
    renderer.drawText(BODY_FONT, x + w - CARD_PAD - tw, ry, text);
    ry += rowH;
  }
  y += whenH + GAP;

  // Recently Finished: as many as fit.
  const int recentH = bottom - y;
  if (recentH < CARD_PAD * 2 + 2 * line) return;
  drawCard(renderer, x, y, w, recentH);
  renderer.drawText(TITLE_FONT, x + CARD_PAD, y + CARD_PAD - 4, tr(STR_QZ_RECENTLY_FINISHED), true,
                    EpdFontFamily::BOLD);
  ry = y + CARD_PAD + line + 8;
  if (finished.recentCount == 0) {
    renderer.drawText(BODY_FONT, x + CARD_PAD, ry, tr(STR_QZ_NO_FINISHED));
    return;
  }
  const int listRow = line + 12;
  for (size_t i = 0; i < finished.recentCount; ++i) {
    if (ry + line > y + recentH - CARD_PAD) break;
    if (i > 0) renderer.drawLine(x + CARD_PAD, ry - 6, x + w - CARD_PAD, ry - 6, true);
    ReadingStatsDate date;
    char when[16] = "";
    if (readingStatsDateFromDayIndex(recent[i].dayIndex, date)) formatReadingStatsShortDate(date, when, sizeof(when));
    const int ww = renderer.getTextWidth(BODY_FONT, when);
    const std::string title = renderer.truncatedText(BODY_FONT, recent[i].title, inner - ww - 12);
    renderer.drawText(BODY_FONT, x + CARD_PAD, ry, title.c_str());
    renderer.drawText(BODY_FONT, x + w - CARD_PAD - ww, ry, when);
    ry += listRow;
  }
}

void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  int top = 0, right = 0, bottom = 0, left = 0;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int x = left + MARGIN;
  const int w = renderer.getScreenWidth() - left - right - 2 * MARGIN;
  int y = top + MARGIN;
  const int contentBottom = renderer.getScreenHeight() - bottom - metrics.buttonHintsHeight - MARGIN;

  renderTitleBar(x, y, w, BAR_HEIGHT);
  y += BAR_HEIGHT + GAP;

  if (!hasData) {
    renderer.drawCenteredText(BODY_FONT, y + (contentBottom - y) / 2, tr(STR_QZ_NO_STATS));
  } else if (page == 0) {
    renderSummaryPage(x, y, w, contentBottom);
  } else {
    renderFinishedPage(x, y, w, contentBottom);
  }

  char pageText[12];
  snprintf(pageText, sizeof(pageText), "%d / %d", page + 1, PAGE_COUNT);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", page > 0 ? "<" : "", page + 1 < PAGE_COUNT ? ">" : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.drawCenteredText(SMALL_FONT_ID, contentBottom + 2, hasData ? pageText : "");

  renderer.displayBuffer();
}
