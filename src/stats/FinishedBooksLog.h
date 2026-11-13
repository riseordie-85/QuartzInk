#pragma once

#include <cstddef>
#include <cstdint>

// Books the reader finished, newest last, for the Quartz stats page
// ("Finished" by month and "Recently Finished"). Stored at
// /.crosspoint/finished_books.bin:
//   magic "QZFB", u8 version, 3 reserved bytes,
//   then fixed ENTRY_SIZE records: u32 dayIndex, u32 pathHash, char title[56].
// Records are appended; the file keeps the newest MAX_FINISHED_BOOKS.
namespace finishedbooks {

constexpr size_t TITLE_LEN = 56;
constexpr size_t MAX_FINISHED_BOOKS = 200;

struct Entry {
  uint32_t dayIndex = 0;  // readingStatsDayIndex of the finish date
  uint32_t pathHash = 0;
  char title[TITLE_LEN] = {};
};

uint32_t hashPath(const char* path);

// Appends a finish unless this book is already in the log. Returns false on
// a storage error.
bool record(const char* bookPath, const char* title, uint32_t dayIndex);

// Streams the log once. Fills counts[month - 1] for finishes in `year`, sets
// total and finishedInYear, and copies up to maxRecent newest entries into
// recent (newest first). recent may be null when maxRecent is 0.
struct Summary {
  uint16_t total = 0;
  uint16_t finishedInYear = 0;
  uint8_t byMonth[12] = {};
  size_t recentCount = 0;
};
Summary summarize(uint16_t year, Entry* recent, size_t maxRecent);

}  // namespace finishedbooks
