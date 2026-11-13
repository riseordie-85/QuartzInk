#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include "DailyReadingLog.h"
#include "FinishedBooksLog.h"
#include "HalStorage.h"
#include "ReadingStatsUtils.h"
#include "TrustedTime.h"

namespace {
class ReadingStatsLogs : public ::testing::Test {
 protected:
  void SetUp() override {
    fake::reset();
    Storage.mkdir("/.crosspoint");
    setenv("TZ", "UTC0", 1);
    tzset();
  }
};

ReadingStatsDateTime at(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t min) {
  ReadingStatsDateTime dt;
  dt.date = {y, m, d};
  dt.hour = h;
  dt.minute = min;
  return dt;
}

uint32_t day(uint16_t y, uint8_t m, uint8_t d) { return readingStatsDayIndex(ReadingStatsDate{y, m, d}); }
}  // namespace

TEST_F(ReadingStatsLogs, DailyLogRoundTripsAndSplitsAtMidnight) {
  DailyReadingLog log;
  ASSERT_TRUE(log.load());
  log.addSpan(at(2026, 11, 12, 23, 50), 20 * 60);  // 10 min before, 10 after midnight
  log.addSpan(at(2026, 11, 13, 8, 0), 300);
  ASSERT_TRUE(log.save());

  DailyReadingLog again;
  ASSERT_TRUE(again.load());
  EXPECT_EQ(again.secondsOn(day(2026, 11, 12)), 600u);
  EXPECT_EQ(again.secondsOn(day(2026, 11, 13)), 900u);
  EXPECT_EQ(again.secondsBetween(day(2026, 11, 12), day(2026, 11, 13)), 1500u);
  EXPECT_EQ(again.lastDayIndex(), day(2026, 11, 13));
}

TEST_F(ReadingStatsLogs, DailyLogForgetsDaysOutsideTheWindow) {
  DailyReadingLog log;
  ASSERT_TRUE(log.load());
  log.addSpan(at(2025, 1, 1, 12, 0), 100);
  // More than DAILY_LOG_DAYS later: the old slot is reused and must read 0.
  log.addSpan(at(2026, 6, 1, 12, 0), 200);
  EXPECT_EQ(log.secondsOn(day(2025, 1, 1)), 0u);
  EXPECT_EQ(log.secondsOn(day(2026, 6, 1)), 200u);
  // A day that maps to the same slot inside the gap was cleared.
  const uint32_t reused = day(2026, 6, 1) - DAILY_LOG_DAYS + 1;
  EXPECT_EQ(log.secondsOn(reused), 0u);
}

TEST_F(ReadingStatsLogs, DailyLogCapsADayAt65535Seconds) {
  DailyReadingLog log;
  ASSERT_TRUE(log.load());
  log.addSpan(at(2026, 3, 1, 0, 0), 20 * 3600);
  EXPECT_EQ(log.secondsOn(day(2026, 3, 1)), 65535u);
}

TEST_F(ReadingStatsLogs, FinishedLogCountsByMonthAndListsNewestFirst) {
  ASSERT_TRUE(finishedbooks::record("/Books/A.epub", "Alpha", day(2026, 1, 5)));
  ASSERT_TRUE(finishedbooks::record("/Books/B.epub", "Beta", day(2026, 3, 9)));
  ASSERT_TRUE(finishedbooks::record("/Books/C.epub", "Gamma", day(2025, 12, 31)));
  ASSERT_TRUE(finishedbooks::record("/Books/A.epub", "Alpha again", day(2026, 4, 1)));  // duplicate

  finishedbooks::Entry recent[2];
  const auto summary = finishedbooks::summarize(2026, recent, 2);
  EXPECT_EQ(summary.total, 3u);
  EXPECT_EQ(summary.finishedInYear, 2u);
  EXPECT_EQ(summary.byMonth[0], 1u);
  EXPECT_EQ(summary.byMonth[2], 1u);
  ASSERT_EQ(summary.recentCount, 2u);
  EXPECT_STREQ(recent[0].title, "Gamma");
  EXPECT_STREQ(recent[1].title, "Beta");
}

TEST_F(ReadingStatsLogs, FinishedLogKeepsOnlyTheNewestEntries) {
  for (size_t i = 0; i < finishedbooks::MAX_FINISHED_BOOKS + 5; ++i) {
    const std::string path = "/Books/" + std::to_string(i) + ".epub";
    const std::string title = "Book " + std::to_string(i);
    ASSERT_TRUE(finishedbooks::record(path.c_str(), title.c_str(), day(2026, 1, 1)));
  }
  finishedbooks::Entry recent[3];
  const auto summary = finishedbooks::summarize(2026, recent, 3);
  EXPECT_EQ(summary.total, finishedbooks::MAX_FINISHED_BOOKS);
  ASSERT_EQ(summary.recentCount, 3u);
  const size_t last = finishedbooks::MAX_FINISHED_BOOKS + 4;
  EXPECT_EQ(std::string(recent[0].title), "Book " + std::to_string(last));
  EXPECT_EQ(std::string(recent[2].title), "Book " + std::to_string(last - 2));
}

TEST_F(ReadingStatsLogs, EpochConvertsToLocalTime) {
  setenv("TZ", "MST7MDT,M3.2.0,M11.1.0", 1);  // Denver
  tzset();
  ReadingStatsDateTime dt;
  // 2026-11-13 04:30:00 UTC is 21:30 on the 12th in Denver (MST).
  ASSERT_TRUE(readingStatsDateTimeFromEpoch(1794544200, dt));
  EXPECT_EQ(dt.date.day, 12);
  EXPECT_EQ(dt.hour, 21);
  EXPECT_EQ(dt.minute, 30);
  EXPECT_FALSE(readingStatsDateTimeFromEpoch(0, dt));
}
