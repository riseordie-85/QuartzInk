#include "ReadingStatsRecorder.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <ZipFile.h>

#include <algorithm>
#include <functional>

#include "BookStatsTracking.h"
#include "CrossPointSettings.h"
#include "DailyReadingLog.h"
#include "FinishedBooksLog.h"
#include "GlobalReadingStats.h"
#include "ReadingSessionStats.h"

namespace readingstats {
namespace {

// QuartzInk (from CrossInk) named EPUB caches by a 64-bit FNV hash of the
// path; CrossPoint names them by std::hash. Same book, different folder.
std::string quartzInkCachePath(const std::string& bookPath) {
  return "/.crosspoint/epub_" + std::to_string(ZipFile::fnvHash64(bookPath.c_str(), bookPath.size()));
}

bool hasStatsFile(const std::string& cachePath) {
  return Storage.exists((cachePath + "/stats_v5.bin").c_str()) || Storage.exists((cachePath + "/stats.bin").c_str());
}

bool trackingOn(const std::string& cachePath) {
  return !cachePath.empty() && SETTINGS.shouldTrackReadingStats() && BookStatsTracking::isBookEnabled(cachePath);
}

}  // namespace

std::string cachePathForBook(const std::string& bookPath) {
  // Mirrors the Epub and Xtc constructors (lib/Epub/Epub.cpp, lib/Xtc/Xtc.h).
  const std::string hash = std::to_string(std::hash<std::string>{}(bookPath));
  if (FsHelpers::hasXtcExtension(bookPath)) return "/.crosspoint/xtc_" + hash;
  if (FsHelpers::hasEpubExtension(bookPath) || FsHelpers::hasTxtExtension(bookPath) ||
      FsHelpers::hasMarkdownExtension(bookPath)) {
    return "/.crosspoint/epub_" + hash;
  }
  return "";
}

BookReadingStats peekBook(const std::string& bookPath) {
  const std::string cachePath = cachePathForBook(bookPath);
  if (cachePath.empty()) return {};
  if (!hasStatsFile(cachePath)) {
    const std::string oldPath = quartzInkCachePath(bookPath);
    if (hasStatsFile(oldPath)) return BookReadingStats::load(oldPath);
  }
  return BookReadingStats::load(cachePath);
}

BookReadingStats loadBook(const std::string& bookPath, const std::string& cachePath) {
  if (!hasStatsFile(cachePath)) {
    const std::string oldPath = quartzInkCachePath(bookPath);
    if (oldPath != cachePath && hasStatsFile(oldPath)) {
      BookReadingStats adopted = BookReadingStats::load(oldPath);
      Storage.ensureDirectoryExists(cachePath.c_str());
      if (adopted.save(cachePath)) {
        LOG_INF("QZST", "Adopted stats for %s from %s", bookPath.c_str(), oldPath.c_str());
      } else {
        LOG_ERR("QZST", "Could not copy stats for %s from %s", bookPath.c_str(), oldPath.c_str());
      }
      return adopted;
    }
  }
  return BookReadingStats::load(cachePath);
}

void commitSession(const std::string& bookPath, const std::string& cachePath, const Session& session) {
  if (session.seconds == 0 || !trackingOn(cachePath)) return;

  BookReadingStats book = loadBook(bookPath, cachePath);
  GlobalReadingStats global = GlobalReadingStats::load();

  ReadingStatsDateTime start;
  const bool dated = readingStatsDateTimeFromEpoch(session.startEpoch, start);
  commitReadingSession(book, global, session.seconds, dated ? &start : nullptr);

  if (session.pages > 0) {
    book.totalPagesTurned += session.pages;
    global.totalPagesTurned += session.pages;
    // One pace sample per page at this session's average, capped so a long
    // session can't swamp the running average in one go.
    const uint32_t perPage = session.seconds / session.pages;
    const uint32_t samples = std::min<uint32_t>(session.pages, 50);
    for (uint32_t i = 0; i < samples; ++i) book.recordForwardPageRead(perPage);
  }

  // Time left from this book's own pace: reading time so far over progress so
  // far. Needs at least 1% read to be meaningful.
  if (session.endProgressBp >= 100 && session.endProgressBp < 10000) {
    const uint64_t perBp = static_cast<uint64_t>(book.totalReadingSeconds) / session.endProgressBp;
    book.estimatedTimeLeftSeconds =
        static_cast<uint32_t>(std::min<uint64_t>(perBp * (10000u - session.endProgressBp), UINT32_MAX));
  }

  if (!book.save(cachePath)) LOG_ERR("QZST", "Could not save book stats for %s", bookPath.c_str());
  global.save();

  if (dated) {
    DailyReadingLog daily;
    if (daily.load()) {
      daily.addSpan(start, session.seconds);
      if (!daily.save()) LOG_ERR("QZST", "Could not save the daily reading log");
    }
  }
}

void markFinished(const std::string& bookPath, const std::string& cachePath, const std::string& title) {
  if (!trackingOn(cachePath)) return;
  BookReadingStats book = loadBook(bookPath, cachePath);
  if (book.isCompleted) return;

  ReadingStatsDateTime now;
  const bool dated = getCurrentLocalReadingStatsDateTime(now);
  book.isCompleted = true;
  book.estimatedTimeLeftSeconds = 0;
  if (dated && !book.finishedDateManual) book.finishedDate = now.date;
  if (!book.save(cachePath)) {
    LOG_ERR("QZST", "Could not save finished state for %s", bookPath.c_str());
    return;
  }

  GlobalReadingStats global = GlobalReadingStats::load();
  ++global.completedBooks;
  global.save();

  if (dated) finishedbooks::record(bookPath.c_str(), title.c_str(), readingStatsDayIndex(now.date));
  LOG_INF("QZST", "Finished %s", bookPath.c_str());
}

}  // namespace readingstats
