#pragma once

#include <array>
#include <cstdint>

#include "activities/Activity.h"
#include "stats/FinishedBooksLog.h"
#include "stats/ReadingStatsUtils.h"

// QuartzInk reading stats, laid out like Quartz Reader's stats page on two
// e-ink pages:
//   1. Today / This week / This year / Streak, the Reading Days calendar, totals
//   2. Finished by month, When You Read, Recently Finished
// Everything is read once in onEnter(); render() only draws.
class ReadingStatsActivity final : public Activity {
 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr int PAGE_COUNT = 2;
  static constexpr int MAX_WEEKS = 30;
  static constexpr size_t MAX_RECENT = 6;

  // Calendar cell shades, oldest week first, Monday first within a week.
  enum Shade : uint8_t { Future = 0, None, Light, Medium, Full };

 private:
  void renderTitleBar(int x, int y, int width, int height) const;
  void renderSummaryPage(int x, int y, int width, int bottom) const;
  void renderFinishedPage(int x, int y, int width, int bottom) const;
  void turnPage(int delta);

  int page = 0;
  bool hasData = false;
  uint16_t currentYear = 0;

  uint32_t todaySeconds = 0;
  uint32_t weekSeconds = 0;
  uint32_t yearSeconds = 0;
  uint16_t streakDays = 0;
  uint16_t bestStreakDays = 0;

  uint32_t totalSeconds = 0;
  uint32_t totalPages = 0;
  uint32_t totalSessions = 0;
  uint32_t finishedAll = 0;

  std::array<uint32_t, READING_TIME_BUCKET_COUNT> timeOfDaySeconds{};
  std::array<uint8_t, MAX_WEEKS * 7> calendar{};

  finishedbooks::Summary finished;
  std::array<finishedbooks::Entry, MAX_RECENT> recent{};
};
