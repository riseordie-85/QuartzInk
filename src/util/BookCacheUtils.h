#pragma once

#include <string>

// Clears the reading cache for a book file if its extension is recognised
// (EPUB, XTC, or TXT). Does nothing for other file types.
void clearBookCache(const std::string& path);

// Returns true if the directory name matches a book cache entry.
bool isBookCacheDirectoryName(const char* name);

// Delete a local book and its local clipping state after the file is removed.
bool removeBookFile(const std::string& path);

// The cache directory a book's layout, progress and reading stats live in
// ("/.crosspoint/epub_<hash>" or "xtc_<hash>"); empty for other file types.
std::string bookCachePath(const std::string& path);

// Moves or renames a book file with everything saved for it: cache (progress,
// reading stats), bookmarks, clippings, protected-book sidecars (.key,
// .rights), its Recent Books entry and the open-book path. Undoes the moves
// already made if a step fails. False when nothing was moved; the target must
// not exist.
bool moveBookPreservingState(const std::string& oldPath, const std::string& newPath);
