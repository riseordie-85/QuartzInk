#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// QuartzInk sleep images: the folders wallpapers and overlays live in, which
// sleep modes show each kind, and the one pinned image
// (APP_STATE.pinnedSleepImage). Shared by the Wallpapers screen in Settings,
// the image viewer and the web Wallpapers page, so all three agree on what a
// pin means and which mode it needs.
namespace sleepimages {

enum class Kind : uint8_t { Wallpaper, Overlay };

// As SleepActivity reads them: Custom takes BMPs from the wallpaper folders;
// the overlay modes take BMPs or PNGs from the overlay folders. The first
// (dot) folder of each pair is where new files go.
inline constexpr const char* WALLPAPER_DIRS[] = {"/.sleep", "/sleep"};
inline constexpr const char* OVERLAY_DIRS[] = {"/.sleep-overlay", "/sleep-overlay"};

inline const char* uploadDir(const Kind kind) { return kind == Kind::Overlay ? OVERLAY_DIRS[0] : WALLPAPER_DIRS[0]; }

// The four sleep modes that show these images, in the order the Wallpapers
// screen cycles through them.
inline constexpr size_t IMAGE_MODE_COUNT = 4;
extern const uint8_t IMAGE_MODES[IMAGE_MODE_COUNT];

// True, with the kind, when `path` is a file directly inside one of the
// folders (FAT names are case-insensitive; the device skips subfolders).
bool kindOf(std::string_view path, Kind& out);
// Whether a file of this name is shown for this kind (by extension).
bool usableName(std::string_view name, Kind kind);
// Whether the sleep mode draws images of this kind.
bool modeShows(Kind kind, uint8_t mode);

enum class PinResult : uint8_t { Ok, NotUsable, SaveFailed };
// Pins `path` (empty unpins, back to a random image). Switches the sleep mode
// to one that shows the image when the current one would not, and saves both.
PinResult pin(const std::string& path);
bool isPinned(const std::string& path);

// Fills `out` with the full paths of this kind's images from both folders,
// sorted, at most `maxCount`.
void list(Kind kind, std::vector<std::string>& out, size_t maxCount);

}  // namespace sleepimages
