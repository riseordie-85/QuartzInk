#pragma once

#include <cstdint>
#include <string>

#include "BookReadingStats.h"

// Single entry point between the readers and the QuartzInk reading stats.
// ReaderActivity calls these for every format (EPUB, TXT/MD, XTC), so no
// reader keeps its own stats state.
namespace readingstats {

struct Session {
  uint32_t seconds = 0;        // active reading time (ReaderSession dwell)
  uint32_t pages = 0;          // forward pages that counted toward it
  int64_t startEpoch = 0;      // trusted epoch of the first counted page
  uint16_t endProgressBp = 0;  // whole-book progress, 0..10000
};

// This book's stats. On first use under CrossPoint it adopts the stats a
// QuartzInk 1.x build saved under that build's cache folder name.
BookReadingStats loadBook(const std::string& bookPath, const std::string& cachePath);

// Book cache folder the readers use for this path (EPUB, TXT/MD or XTC);
// empty for formats without stats.
std::string cachePathForBook(const std::string& bookPath);

// Read-only: this book's stats from its cache folder, else from the folder a
// QuartzInk 1.x build used. Never writes; safe from the web server.
BookReadingStats peekBook(const std::string& bookPath);

// Adds one finished reading session to the book, global and daily stats.
// No-op when stats are off globally or for this book.
void commitSession(const std::string& bookPath, const std::string& cachePath, const Session& session);

// Marks the book finished once: book stats, global finished count and the
// finished-books log. No-op when already finished or stats are off.
void markFinished(const std::string& bookPath, const std::string& cachePath, const std::string& title);

}  // namespace readingstats
