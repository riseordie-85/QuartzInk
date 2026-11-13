# QuartzInk reading stats export

QuartzInk can export this reader's reading stats as JSON so they can leave the
device (for Quartz Reader, a spreadsheet, or a backup).

## Getting the file

1. On the reader, open **File Transfer** and connect to Wi-Fi.
2. Open `http://<reader-ip>/api/stats?download=1` in a browser.

Apps can fetch the same document directly from `http://<reader-ip>/api/stats`.
Add `?download=1` to have browsers save it as `quartzink-stats.json`.

## Format (version 1)

```json
{
  "format": "quartzink-stats",
  "version": 1,
  "device": { "id": "a1b2c3d4e5f6", "name": "X4 Pro", "type": "X4 Pro", "firmware": "2.0.0" },
  "exportedAt": "2026-11-11T19:55:00",
  "global": {
    "sessions": 120,
    "readingSeconds": 86400,
    "pagesTurned": 5400,
    "completedBooks": 7,
    "longestStreakDays": 12,
    "currentStreakDays": 3,
    "timeOfDaySeconds": { "morning": 0, "afternoon": 0, "evening": 0, "night": 0 },
    "dayOfWeekSeconds": [0, 0, 0, 0, 0, 0, 0],
    "readingDays": ["2026-11-11", "2026-11-10"],
    "dailySeconds": [["2026-11-11", 1500], ["2026-11-10", 2400]]
  },
  "books": [
    {
      "path": "/Books/Horror/Example.epub",
      "title": "Example",
      "author": "Author Name",
      "series": null,
      "format": "epub",
      "sessions": 14,
      "readingSeconds": 21600,
      "pagesTurned": 900,
      "completed": false,
      "startDate": "2026-10-30",
      "finishedDate": null,
      "secondsPerPage": 24,
      "timeLeftSeconds": 5400,
      "timeOfDaySeconds": { "morning": 0, "afternoon": 0, "evening": 0, "night": 0 },
      "dayOfWeekSeconds": [0, 0, 0, 0, 0, 0, 0]
    }
  ]
}
```

- `device.id` is the reader's factory MAC address. It is stable across firmware
  updates, so an importer can keep one record per reader and replace it on each
  import instead of adding the numbers again.
- `global` holds this reader's own totals only.
- `readingDays` lists every day with reading in the last two years, newest
  first. Dates and times are the reader's local time.
- `dailySeconds` gives seconds read on each day of the last 400, newest first,
  days with reading only. It starts empty on a reader that has just moved to
  this firmware: days before that appear in `readingDays` only. Importers that
  don't know the field can ignore it; the format stays version 1.
- `dayOfWeekSeconds` starts on Monday.
- `books` lists every EPUB, TXT, Markdown and XTC book with any stats. With a
  Library index (open Library once), all books on the card are covered, with
  title and author from the Library; otherwise only books in the recent list
  are. `series` is always `null` on this firmware.
- Missing values are `null`: dates the reader never recorded, a time-left
  estimate it has not made yet, and `series`.
