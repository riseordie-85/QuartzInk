#include "StatsExport.h"

#include <ArduinoJson.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <LibraryBuilder.h>
#include <LibraryIndexFile.h>
#include <Logging.h>
#include <Xtc.h>
#ifndef SIMULATOR
#include <esp_mac.h>
#endif

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include "AppVersion.h"
#include "CrossPointSettings.h"
#include "RecentBooksStore.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/GlobalReadingStats.h"
#include "activities/reader/ReadingStatsUtils.h"

namespace stats_export {
namespace {

constexpr int FORMAT_VERSION = 1;
constexpr const char* CACHE_DIR = "/.crosspoint";
constexpr const char* TIME_BUCKETS[] = {"morning", "afternoon", "evening", "night"};

struct Out {
  WriteFn write;
  void* context;
  bool ok = true;

  void put(const char* data, const size_t length) {
    if (ok && length > 0) ok = write(data, length, context);
  }
  void put(const char* text) { put(text, std::strlen(text)); }
  // Each JSON object is serialized on its own, so only one book is in memory
  // at a time no matter how large the library is.
  void put(const JsonDocument& doc) {
    String text;
    serializeJson(doc, text);
    put(text.c_str(), text.length());
  }
};

void formatDate(const ReadingStatsDate& date, char* buffer, const size_t size) {
  std::snprintf(buffer, size, "%04u-%02u-%02u", static_cast<unsigned>(date.year), static_cast<unsigned>(date.month),
                static_cast<unsigned>(date.day));
}

void setDateOrNull(JsonObject object, const char* key, const ReadingStatsDate& date) {
  if (!isValidReadingStatsDate(date)) {
    object[key] = nullptr;
    return;
  }
  char buffer[12];
  formatDate(date, buffer, sizeof(buffer));
  object[key] = buffer;
}

template <size_t N>
void setBuckets(JsonObject parent, const std::array<uint32_t, N>& timeOfDay,
                const std::array<uint32_t, READING_DAY_OF_WEEK_COUNT>& dayOfWeek) {
  JsonObject buckets = parent["timeOfDaySeconds"].to<JsonObject>();
  for (size_t i = 0; i < N && i < std::size(TIME_BUCKETS); ++i) buckets[TIME_BUCKETS[i]] = timeOfDay[i];
  // Monday first, matching ISO weekdays.
  JsonArray days = parent["dayOfWeekSeconds"].to<JsonArray>();
  for (const uint32_t seconds : dayOfWeek) days.add(seconds);
}

void deviceId(char* buffer, const size_t size) {
  std::array<uint8_t, 6> mac{};
#ifndef SIMULATOR
  if (esp_efuse_mac_get_default(mac.data()) != ESP_OK) mac.fill(0);
#endif
  std::snprintf(buffer, size, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void writeHeader(Out& out) {
  JsonDocument doc;
  doc["format"] = "quartzink-stats";
  doc["version"] = FORMAT_VERSION;
  char id[16];
  deviceId(id, sizeof(id));
  JsonObject device = doc["device"].to<JsonObject>();
  device["id"] = id;
  device["name"] = SETTINGS.getEffectiveDeviceName();
  device["type"] = CROSSINK_FIRMWARE_DEVICE_TYPE;
  device["firmware"] = AppVersion::version();
  ReadingStatsDateTime now;
  if (getCurrentLocalReadingStatsDateTime(now)) {
    char stamp[24];
    std::snprintf(stamp, sizeof(stamp), "%04u-%02u-%02uT%02u:%02u:%02u", static_cast<unsigned>(now.date.year),
                  static_cast<unsigned>(now.date.month), static_cast<unsigned>(now.date.day),
                  static_cast<unsigned>(now.hour), static_cast<unsigned>(now.minute),
                  static_cast<unsigned>(now.second));
    doc["exportedAt"] = stamp;
  } else {
    doc["exportedAt"] = nullptr;
  }
  // Emit the header object without its closing brace so the global block and
  // the streamed book list can follow as further members.
  String text;
  serializeJson(doc, text);
  if (text.length() > 0) text.remove(text.length() - 1);
  out.put(text.c_str(), text.length());
}

void writeGlobal(Out& out) {
  // This device's own totals only. Stats merged in from other Xteinks over
  // Nearby Stats Sync are left out so an importer that combines several
  // devices does not count the same reading twice.
  const GlobalReadingStats stats = GlobalReadingStats::load();
  ReadingStatsDateTime now;
  const bool haveToday = getCurrentLocalReadingStatsDateTime(now);

  JsonDocument doc;
  JsonObject global = doc.to<JsonObject>();
  global["sessions"] = stats.totalSessions;
  global["readingSeconds"] = stats.totalReadingSeconds;
  global["pagesTurned"] = stats.totalPagesTurned;
  global["completedBooks"] = stats.completedBooks;
  global["longestStreakDays"] = stats.displayLongestReadingStreak();
  global["currentStreakDays"] = stats.currentReadingStreak(haveToday ? &now.date : nullptr);
  setBuckets(global, stats.timeOfDaySeconds, stats.dayOfWeekSeconds);

  out.put(",\"global\":");
  String text;
  serializeJson(doc, text);
  if (text.length() > 0) text.remove(text.length() - 1);  // reopen to append readingDays
  out.put(text.c_str(), text.length());

  // Every day with reading in the last two years, newest first.
  out.put(",\"readingDays\":[");
  bool first = true;
  if (stats.readingHistoryAnchorDay != 0) {
    for (size_t offset = 0; offset < READING_HISTORY_DAYS; ++offset) {
      if ((stats.readingHistoryBits[offset / 8] & static_cast<uint8_t>(1u << (offset % 8))) == 0) continue;
      if (offset > stats.readingHistoryAnchorDay) break;
      ReadingStatsDate date;
      if (!readingStatsDateFromDayIndex(stats.readingHistoryAnchorDay - static_cast<uint32_t>(offset), date)) continue;
      char item[16];
      std::snprintf(item, sizeof(item), "%s\"%04u-%02u-%02u\"", first ? "" : ",", static_cast<unsigned>(date.year),
                    static_cast<unsigned>(date.month), static_cast<unsigned>(date.day));
      out.put(item);
      first = false;
    }
  }
  out.put("]}");
}

// Writes one book if it has any stats. Returns true when it wrote something.
bool writeBook(Out& out, const std::string& path, const std::string& title, const std::string& author,
               const std::string& series, const bool first) {
  const bool isEpub = FsHelpers::hasEpubExtension(path);
  const bool isXtc = FsHelpers::hasXtcExtension(path);
  if (!isEpub && !isXtc) return false;
  const std::string cachePath =
      isEpub ? Epub::cachePathForFilePath(path, CACHE_DIR) : Xtc(path, CACHE_DIR).getCachePath();
  const BookReadingStats stats = BookReadingStats::load(cachePath);
  if (stats.sessionCount == 0 && stats.totalReadingSeconds == 0 && !stats.isCompleted) return false;

  JsonDocument doc;
  JsonObject book = doc.to<JsonObject>();
  book["path"] = path;
  book["title"] = title;
  book["author"] = author;
  if (series.empty()) {
    book["series"] = nullptr;
  } else {
    book["series"] = series;
  }
  book["format"] = isEpub ? "epub" : "xtc";
  book["sessions"] = stats.sessionCount;
  book["readingSeconds"] = stats.totalReadingSeconds;
  book["pagesTurned"] = stats.totalPagesTurned;
  book["completed"] = stats.isCompleted;
  setDateOrNull(book, "startDate", stats.startDate);
  setDateOrNull(book, "finishedDate", stats.finishedDate);
  book["secondsPerPage"] = stats.avgSecondsPerForwardPage;
  if (stats.estimatedTimeLeftSeconds > 0) {
    book["timeLeftSeconds"] = stats.estimatedTimeLeftSeconds;
  } else {
    book["timeLeftSeconds"] = nullptr;
  }
  setBuckets(book, stats.timeOfDaySeconds, stats.dayOfWeekSeconds);

  if (!first) out.put(",");
  out.put(doc);
  return true;
}

void writeBooks(Out& out) {
  out.put(",\"books\":[");
  bool first = true;

  // The Library index lists every book on the card with its metadata. It only
  // exists once Library has been opened; until then fall back to the recent
  // books list, which covers everything opened recently.
  library::LibraryIndexFile index;
  if (index.open(library::libraryIndexPath())) {
    std::string path;
    std::string title;
    std::string author;
    std::string series;
    for (uint16_t ordinal = 0; ordinal < index.bookCount() && out.ok; ++ordinal) {
      library::ClixRecord record{};
      if (!index.readRecord(ordinal, record) || !index.readPath(record, path)) continue;
      title.clear();
      author.clear();
      series.clear();
      index.readDisplayText(record, title, author);
      index.readSeries(record, series);
      if (writeBook(out, path, title, author, series, first)) first = false;
    }
    index.close();
  } else {
    LOG_INF("STX", "No Library index; exporting stats for recent books only");
    for (const auto& recent : RECENT_BOOKS.getBooks()) {
      if (!out.ok) break;
      if (writeBook(out, recent.path, recent.title, recent.author, std::string{}, first)) first = false;
    }
  }
  out.put("]");
}

}  // namespace

bool writeStatsJson(const WriteFn write, void* context) {
  Out out{write, context};
  writeHeader(out);
  writeGlobal(out);
  writeBooks(out);
  out.put("}");
  if (!out.ok) LOG_ERR("STX", "Stats export write failed");
  return out.ok;
}

}  // namespace stats_export
