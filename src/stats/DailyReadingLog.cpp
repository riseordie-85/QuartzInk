#include "DailyReadingLog.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <new>

namespace {
constexpr char LOG_PATH[] = "/.crosspoint/reading_days.bin";
constexpr char LOG_TMP_PATH[] = "/.crosspoint/reading_days.bin.tmp";
constexpr uint8_t MAGIC[4] = {'Q', 'Z', 'D', 'L'};
constexpr uint8_t VERSION = 1;
constexpr size_t HEADER_SIZE = 12;
constexpr size_t BODY_SIZE = DAILY_LOG_DAYS * 2;
constexpr uint32_t SECONDS_PER_DAY = 24u * 60u * 60u;

void putU32(uint8_t* out, const uint32_t value) {
  out[0] = value & 0xFF;
  out[1] = (value >> 8) & 0xFF;
  out[2] = (value >> 16) & 0xFF;
  out[3] = (value >> 24) & 0xFF;
}

uint32_t getU32(const uint8_t* in) {
  return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) | (static_cast<uint32_t>(in[2]) << 16) |
         (static_cast<uint32_t>(in[3]) << 24);
}
}  // namespace

DailyReadingLog::~DailyReadingLog() { delete[] slots; }

bool DailyReadingLog::load() {
  if (!slots) {
    slots = new (std::nothrow) uint16_t[DAILY_LOG_DAYS];
    if (!slots) {
      LOG_ERR("QZDL", "OOM: daily reading log");
      return false;
    }
  }
  std::fill(slots, slots + DAILY_LOG_DAYS, 0);
  lastDay = 0;

  if (!Storage.exists(LOG_PATH)) return true;
  HalFile file;
  if (!Storage.openFileForRead("QZDL", LOG_PATH, file)) return true;
  uint8_t header[HEADER_SIZE];
  const bool headerOk = file.read(header, HEADER_SIZE) == static_cast<int>(HEADER_SIZE) &&
                        memcmp(header, MAGIC, sizeof(MAGIC)) == 0 && header[4] == VERSION;
  if (!headerOk) {
    LOG_ERR("QZDL", "Ignoring unrecognised %s", LOG_PATH);
    file.close();
    return true;
  }
  uint8_t pair[2];
  for (uint16_t i = 0; i < DAILY_LOG_DAYS; ++i) {
    if (file.read(pair, 2) != 2) {
      LOG_ERR("QZDL", "Short %s; keeping %u days", LOG_PATH, static_cast<unsigned>(i));
      break;
    }
    slots[i] = static_cast<uint16_t>(pair[0] | (pair[1] << 8));
  }
  file.close();
  lastDay = getU32(header + 8);
  return true;
}

bool DailyReadingLog::save() const {
  if (!slots) return false;
  Storage.ensureDirectoryExists("/.crosspoint");
  HalFile file;
  if (!Storage.openFileForWrite("QZDL", LOG_TMP_PATH, file)) {
    LOG_ERR("QZDL", "Could not write %s", LOG_TMP_PATH);
    return false;
  }
  uint8_t header[HEADER_SIZE] = {};
  memcpy(header, MAGIC, sizeof(MAGIC));
  header[4] = VERSION;
  putU32(header + 8, lastDay);
  bool ok = file.write(header, HEADER_SIZE) == HEADER_SIZE;
  // Small fixed chunks keep the stack well under budget.
  uint8_t chunk[64];
  for (uint16_t i = 0; ok && i < DAILY_LOG_DAYS; i += sizeof(chunk) / 2) {
    const uint16_t count = std::min<uint16_t>(sizeof(chunk) / 2, DAILY_LOG_DAYS - i);
    for (uint16_t j = 0; j < count; ++j) {
      chunk[j * 2] = slots[i + j] & 0xFF;
      chunk[j * 2 + 1] = (slots[i + j] >> 8) & 0xFF;
    }
    ok = file.write(chunk, count * 2) == static_cast<size_t>(count * 2);
  }
  file.close();
  if (!ok) {
    LOG_ERR("QZDL", "Short write to %s", LOG_TMP_PATH);
    Storage.remove(LOG_TMP_PATH);
    return false;
  }
  if (Storage.exists(LOG_PATH) && !Storage.remove(LOG_PATH)) {
    LOG_ERR("QZDL", "Could not replace %s", LOG_PATH);
    return false;
  }
  if (!Storage.rename(LOG_TMP_PATH, LOG_PATH)) {
    LOG_ERR("QZDL", "Could not publish %s", LOG_PATH);
    return false;
  }
  return true;
}

void DailyReadingLog::advanceTo(const uint32_t dayIndex) {
  if (lastDay == 0) {
    lastDay = dayIndex;
    return;
  }
  if (dayIndex <= lastDay) return;
  // Clear the slots for days skipped since the last write so they don't show
  // last year's minutes.
  const uint32_t gap = std::min<uint32_t>(dayIndex - lastDay, DAILY_LOG_DAYS);
  for (uint32_t i = 1; i <= gap; ++i) slots[(lastDay + i) % DAILY_LOG_DAYS] = 0;
  lastDay = dayIndex;
}

void DailyReadingLog::addSeconds(const uint32_t dayIndex, const uint32_t seconds) {
  if (!slots || seconds == 0) return;
  advanceTo(dayIndex);
  if (dayIndex + DAILY_LOG_DAYS <= lastDay) return;  // older than the window
  uint16_t& slot = slots[dayIndex % DAILY_LOG_DAYS];
  slot = static_cast<uint16_t>(std::min<uint32_t>(static_cast<uint32_t>(slot) + seconds, UINT16_MAX));
}

void DailyReadingLog::addSpan(const ReadingStatsDateTime& localStart, uint32_t seconds) {
  if (!localStart.isValid()) return;
  uint32_t day = readingStatsDayIndex(localStart.date);
  uint32_t secondOfDay = localStart.hour * 3600u + localStart.minute * 60u + localStart.second;
  while (seconds > 0) {
    const uint32_t leftToday = SECONDS_PER_DAY - secondOfDay;
    const uint32_t part = std::min(seconds, leftToday);
    addSeconds(day, part);
    seconds -= part;
    ++day;
    secondOfDay = 0;
  }
}

uint32_t DailyReadingLog::secondsOn(const uint32_t dayIndex) const {
  if (!slots || lastDay == 0 || dayIndex > lastDay || dayIndex + DAILY_LOG_DAYS <= lastDay) return 0;
  return slots[dayIndex % DAILY_LOG_DAYS];
}

uint32_t DailyReadingLog::secondsBetween(const uint32_t firstDay, const uint32_t lastDayInclusive) const {
  uint32_t total = 0;
  for (uint32_t day = firstDay; day <= lastDayInclusive && day >= firstDay; ++day) total += secondsOn(day);
  return total;
}
