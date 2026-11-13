#pragma once

#include <cstdint>

#include "ReadingStatsUtils.h"

// Seconds read on each of the last DAILY_LOG_DAYS local days, for the Quartz
// stats page (Today / This week / This year, and the Reading Days calendar).
// Stored at /.crosspoint/reading_days.bin as a small fixed-size file:
//   magic "QZDL", u8 version, 3 reserved bytes, u32 lastDayIndex,
//   then DAILY_LOG_DAYS little-endian u16 seconds (capped at 65535, ~18h).
// Slot for day D is D % DAILY_LOG_DAYS; slots older than lastDayIndex -
// (DAILY_LOG_DAYS - 1) are stale and read as 0.
constexpr uint16_t DAILY_LOG_DAYS = 400;

class DailyReadingLog {
 public:
  // Loads the log into a heap buffer. Missing or unreadable files give an
  // empty log. Returns false only when the buffer could not be allocated.
  bool load();
  // Writes the log back through a .tmp file. Returns false on failure.
  bool save() const;

  // Adds a reading span starting at localStart, splitting it across midnight.
  void addSpan(const ReadingStatsDateTime& localStart, uint32_t seconds);

  // Seconds read on dayIndex (readingStatsDayIndex), 0 when out of range.
  uint32_t secondsOn(uint32_t dayIndex) const;
  // Sum of secondsOn() for the inclusive day range.
  uint32_t secondsBetween(uint32_t firstDay, uint32_t lastDay) const;
  uint32_t lastDayIndex() const { return lastDay; }
  bool isLoaded() const { return slots != nullptr; }

  DailyReadingLog() = default;
  ~DailyReadingLog();
  DailyReadingLog(const DailyReadingLog&) = delete;
  DailyReadingLog& operator=(const DailyReadingLog&) = delete;

 private:
  void addSeconds(uint32_t dayIndex, uint32_t seconds);
  void advanceTo(uint32_t dayIndex);

  // 800 bytes; heap rather than stack because it outlives one call frame and
  // exceeds the 256-byte local budget. Freed in the destructor.
  uint16_t* slots = nullptr;
  uint32_t lastDay = 0;
};
