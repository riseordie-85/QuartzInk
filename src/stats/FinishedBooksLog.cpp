#include "FinishedBooksLog.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ReadingStatsUtils.h"

namespace finishedbooks {
namespace {
constexpr char LOG_PATH[] = "/.crosspoint/finished_books.bin";
constexpr char LOG_TMP_PATH[] = "/.crosspoint/finished_books.bin.tmp";
constexpr uint8_t MAGIC[4] = {'Q', 'Z', 'F', 'B'};
constexpr uint8_t VERSION = 1;
constexpr size_t HEADER_SIZE = 8;
constexpr size_t ENTRY_SIZE = 8 + TITLE_LEN;

void putU32(uint8_t* out, const uint32_t value) {
  for (int i = 0; i < 4; ++i) out[i] = (value >> (8 * i)) & 0xFF;
}

uint32_t getU32(const uint8_t* in) {
  return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) | (static_cast<uint32_t>(in[2]) << 16) |
         (static_cast<uint32_t>(in[3]) << 24);
}

void encode(const Entry& entry, uint8_t* out) {
  putU32(out, entry.dayIndex);
  putU32(out + 4, entry.pathHash);
  memcpy(out + 8, entry.title, TITLE_LEN);
}

void decode(const uint8_t* in, Entry& entry) {
  entry.dayIndex = getU32(in);
  entry.pathHash = getU32(in + 4);
  memcpy(entry.title, in + 8, TITLE_LEN);
  entry.title[TITLE_LEN - 1] = '\0';
}

// Opens the log and checks its header. Returns the record count, or -1 when
// there is no usable log.
int openLog(HalFile& file) {
  if (!Storage.exists(LOG_PATH) || !Storage.openFileForRead("QZFB", LOG_PATH, file)) return -1;
  uint8_t header[HEADER_SIZE];
  if (file.read(header, HEADER_SIZE) != static_cast<int>(HEADER_SIZE) || memcmp(header, MAGIC, 4) != 0 ||
      header[4] != VERSION) {
    LOG_ERR("QZFB", "Ignoring unrecognised %s", LOG_PATH);
    file.close();
    return -1;
  }
  return static_cast<int>((file.fileSize() - HEADER_SIZE) / ENTRY_SIZE);
}

bool writeHeader(HalFile& file) {
  uint8_t header[HEADER_SIZE] = {};
  memcpy(header, MAGIC, 4);
  header[4] = VERSION;
  return file.write(header, HEADER_SIZE) == HEADER_SIZE;
}
}  // namespace

uint32_t hashPath(const char* path) {
  uint32_t hash = 2166136261u;  // FNV-1a
  for (const char* p = path; p && *p; ++p) {
    hash ^= static_cast<uint8_t>(*p);
    hash *= 16777619u;
  }
  return hash;
}

bool record(const char* bookPath, const char* title, const uint32_t dayIndex) {
  Entry entry;
  entry.dayIndex = dayIndex;
  entry.pathHash = hashPath(bookPath);
  snprintf(entry.title, sizeof(entry.title), "%s", title ? title : "");

  uint8_t buffer[ENTRY_SIZE];
  HalFile in;
  const int count = openLog(in);
  if (count > 0) {
    for (int i = 0; i < count; ++i) {
      if (in.read(buffer, ENTRY_SIZE) != static_cast<int>(ENTRY_SIZE)) break;
      if (getU32(buffer + 4) == entry.pathHash) {
        in.close();
        return true;  // already counted
      }
    }
    in.close();
  }

  // Copy the newest records plus the new one into a fresh file, so the log
  // never grows past MAX_FINISHED_BOOKS and a failed write leaves the old log.
  Storage.ensureDirectoryExists("/.crosspoint");
  HalFile out;
  if (!Storage.openFileForWrite("QZFB", LOG_TMP_PATH, out)) {
    LOG_ERR("QZFB", "Could not write %s", LOG_TMP_PATH);
    return false;
  }
  bool ok = writeHeader(out);
  if (ok && count > 0 && openLog(in) == count) {
    const int skip = count + 1 > static_cast<int>(MAX_FINISHED_BOOKS) ? count + 1 - MAX_FINISHED_BOOKS : 0;
    for (int i = 0; ok && i < count; ++i) {
      if (in.read(buffer, ENTRY_SIZE) != static_cast<int>(ENTRY_SIZE)) break;
      if (i >= skip) ok = out.write(buffer, ENTRY_SIZE) == ENTRY_SIZE;
    }
    in.close();
  }
  if (ok) {
    encode(entry, buffer);
    ok = out.write(buffer, ENTRY_SIZE) == ENTRY_SIZE;
  }
  out.close();
  if (!ok) {
    LOG_ERR("QZFB", "Short write to %s", LOG_TMP_PATH);
    Storage.remove(LOG_TMP_PATH);
    return false;
  }
  if (Storage.exists(LOG_PATH) && !Storage.remove(LOG_PATH)) {
    LOG_ERR("QZFB", "Could not replace %s", LOG_PATH);
    return false;
  }
  if (!Storage.rename(LOG_TMP_PATH, LOG_PATH)) {
    LOG_ERR("QZFB", "Could not publish %s", LOG_PATH);
    return false;
  }
  return true;
}

Summary summarize(const uint16_t year, Entry* recent, const size_t maxRecent) {
  Summary summary;
  HalFile file;
  const int count = openLog(file);
  if (count <= 0) return summary;

  // Keep the newest maxRecent records in a small ring, then reverse it.
  size_t ringNext = 0;
  uint8_t buffer[ENTRY_SIZE];
  Entry entry;
  for (int i = 0; i < count; ++i) {
    if (file.read(buffer, ENTRY_SIZE) != static_cast<int>(ENTRY_SIZE)) break;
    decode(buffer, entry);
    ++summary.total;
    ReadingStatsDate date;
    if (readingStatsDateFromDayIndex(entry.dayIndex, date) && date.year == year && date.month >= 1 &&
        date.month <= 12) {
      ++summary.finishedInYear;
      if (summary.byMonth[date.month - 1] < UINT8_MAX) ++summary.byMonth[date.month - 1];
    }
    if (recent && maxRecent > 0) {
      recent[ringNext % maxRecent] = entry;
      ++ringNext;
    }
  }
  file.close();

  if (recent && maxRecent > 0) {
    const size_t kept = ringNext < maxRecent ? ringNext : maxRecent;
    // Once the ring wrapped, the oldest kept record sits at ringNext % maxRecent.
    if (ringNext > maxRecent) std::rotate(recent, recent + (ringNext % maxRecent), recent + maxRecent);
    std::reverse(recent, recent + kept);  // newest first
    summary.recentCount = kept;
  }
  return summary;
}

}  // namespace finishedbooks
