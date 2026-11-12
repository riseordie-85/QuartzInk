#pragma once

#include <cstddef>

// QuartzInk reading-stats export: this device's totals, reading days and
// per-book stats as one JSON document (format "quartzink-stats", version 1;
// see docs/stats-export.md).
//
// The document is produced in pieces through `write` so the caller can stream
// it (the web server sends it as chunked HTTP) without holding the whole thing
// in memory. Returns false if a write failed.
namespace stats_export {

using WriteFn = bool (*)(const char* data, size_t length, void* context);

bool writeStatsJson(WriteFn write, void* context);

}  // namespace stats_export
