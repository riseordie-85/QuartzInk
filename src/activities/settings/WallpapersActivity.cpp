#include "WallpapersActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <iterator>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "activities/util/BmpViewerActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
constexpr StrId TAB_NAME_IDS[] = {StrId::STR_QZ_WALLPAPERS, StrId::STR_QZ_OVERLAYS};

// Names for every sleep mode, as in Settings > Display > Sleep Screen.
constexpr StrId SLEEP_MODE_NAME_IDS[] = {
    StrId::STR_DARK,         StrId::STR_LIGHT,        StrId::STR_CUSTOM,
    StrId::STR_COVER,        StrId::STR_COVER_CUSTOM, StrId::STR_NONE_OPT,
    StrId::STR_QUICK_RESUME, StrId::STR_TRANSPARENT,  StrId::STR_COVER_TRANSPARENT,
};
static_assert(std::size(SLEEP_MODE_NAME_IDS) == CrossPointSettings::SLEEP_SCREEN_MODE_COUNT);

const char* sleepModeName(const uint8_t mode) {
  return mode < std::size(SLEEP_MODE_NAME_IDS) ? I18N.get(SLEEP_MODE_NAME_IDS[mode]) : "";
}
}  // namespace

WallpapersActivity::WallpapersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("Wallpapers", renderer, mappedInput) {}

const char* WallpapersActivity::tabLabel(const int index) const { return I18N.get(TAB_NAME_IDS[index]); }

sleepimages::Kind WallpapersActivity::kind() const {
  return tab_ == Tab::Overlays ? sleepimages::Kind::Overlay : sleepimages::Kind::Wallpaper;
}

void WallpapersActivity::onEnter() {
  UiTabListActivity::onEnter();
  // Open on the tab the current sleep mode uses.
  tab_ = sleepimages::modeShows(sleepimages::Kind::Overlay, SETTINGS.sleepScreen) ? Tab::Overlays : Tab::Wallpapers;
  for (auto& n : tabNavs) n.selected = 1;  // first row
  tabNavs[static_cast<int>(tab_)].selected = 0;
  loadImages();
}

// Rereads the active tab's folders. Runs on entry, on tab switches and after
// the viewer returns (it may have changed the pin).
void WallpapersActivity::loadImages() {
  sleepimages::list(kind(), images_, MAX_IMAGES);
  names_.clear();
  names_.reserve(images_.size());
  for (const auto& path : images_) names_.push_back(path.substr(path.find_last_of('/') + 1));
  rebuildRowItems();
}

int WallpapersActivity::listCount() const {
  // An empty folder still gets one row saying how to add images.
  return FIXED_ROWS + std::max<int>(1, static_cast<int>(images_.size()));
}

void WallpapersActivity::rebuildRowItems() {
  const int count = listCount();
  rowItems_.clear();
  rowItems_.reserve(count);
  for (int i = 0; i < count; i++) {
    fui::ListItem item;
    if (i == ROW_MODE) {
      item.label = tr(STR_SLEEP_SCREEN);
    } else if (i == ROW_SHUFFLE) {
      item.label = tr(STR_QZ_SHUFFLE);
    } else if (images_.empty()) {
      item.label = tr(STR_QZ_NO_SLEEP_IMAGES);
    } else {
      item.label = names_[i - FIXED_ROWS].c_str();
    }
    item.actionValue = static_cast<int16_t>(i);
    rowItems_.push_back(item);
  }
}

void WallpapersActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safeArea = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int tabTop = safeArea.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int rightInset = renderer.getScreenWidth() - (safeArea.x + safeArea.width);
  const int bottomInset = renderer.getScreenHeight() - (safeArea.y + safeArea.height);
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(tabTop), static_cast<int16_t>(rightInset),
                                                static_cast<int16_t>(bottomInset), static_cast<int16_t>(safeArea.x)});

  buildTabBar(screen);

  // Values change with the pin and mode; labels only with the folder contents.
  for (size_t i = 0; i < rowItems_.size(); i++) {
    auto& item = rowItems_[i];
    if (i == ROW_MODE) {
      item.value = sleepModeName(SETTINGS.sleepScreen);
    } else if (i == ROW_SHUFFLE) {
      item.value = nullptr;
      GUI.setCheckboxRow(item, APP_STATE.pinnedSleepImage.empty());
    } else if (!images_.empty()) {
      item.value = sleepimages::isPinned(images_[i - FIXED_ROWS]) ? tr(STR_QZ_PINNED) : nullptr;
    }
  }

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.valueInset = 8;
  syncTabListViewport(screen, props);
  screen.list(props);
}

void WallpapersActivity::onTabAction(const int index) {
  if (tab_ != static_cast<Tab>(index)) {
    tab_ = static_cast<Tab>(index);
    loadImages();
    auto& n = activeNav();
    n.selected = 0;
    n.followOnBuild = true;
    requestUpdate();
  }
  app.clearTapFlash();
}

void WallpapersActivity::switchTab(const int direction) {
  const bool onTabBar = ringPos() == 0;
  constexpr int count = static_cast<int>(Tab::Count);
  tab_ = static_cast<Tab>((static_cast<int>(tab_) + direction + count) % count);
  loadImages();
  auto& n = activeNav();
  if (onTabBar) n.selected = 0;
  n.followOnBuild = true;
  requestUpdate();
}

void WallpapersActivity::activateIndex(const int index) {
  app.clearTapFlash();
  activateRow(index);
}

void WallpapersActivity::activateRow(const int row) {
  if (row == ROW_MODE) {
    cycleMode();
  } else if (row == ROW_SHUFFLE) {
    if (sleepimages::pin(std::string()) != sleepimages::PinResult::Ok) LOG_ERR("WALL", "Failed to clear the pin");
    requestUpdate();
  } else if (!images_.empty()) {
    openImage(row - FIXED_ROWS);
  }
}

// Steps through the four modes that show images; from any other mode it
// starts at the first one for this tab.
void WallpapersActivity::cycleMode() {
  const uint8_t current = SETTINGS.sleepScreen;
  const auto* begin = std::begin(sleepimages::IMAGE_MODES);
  const auto* end = std::end(sleepimages::IMAGE_MODES);
  const auto* at = std::find(begin, end, current);
  uint8_t next;
  if (at == end) {
    next = kind() == sleepimages::Kind::Overlay ? CrossPointSettings::TRANSPARENT_CUSTOM : CrossPointSettings::CUSTOM;
  } else {
    next = (at + 1 == end) ? *begin : *(at + 1);
  }
  SETTINGS.sleepScreen = next;
  if (!SETTINGS.saveToFile()) LOG_ERR("WALL", "Failed to save sleep screen %u", next);
  requestUpdate();
}

void WallpapersActivity::openImage(const int imageIndex) {
  if (imageIndex < 0 || imageIndex >= static_cast<int>(images_.size())) return;
  auto viewer = makeUniqueNoThrow<BmpViewerActivity>(renderer, mappedInput, images_[imageIndex], true);
  if (!viewer) {
    LOG_ERR("WALL", "OOM: image viewer");
    return;
  }
  startActivityForResult(std::move(viewer), [this](const ActivityResult&) {
    loadImages();
    resetUi();
    requestUpdate();
  });
}

bool WallpapersActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (ringPos() == 0) {
      switchTab(1);
    } else {
      activateRow(ringPos() - 1);
    }
    return true;
  }
  return false;
}

const char* WallpapersActivity::confirmLabelText() const {
  const int ring = ringPos();
  if (ring == 0) {
    return I18N.get(TAB_NAME_IDS[(static_cast<int>(tab_) + 1) % static_cast<int>(Tab::Count)]);
  }
  const int row = ring - 1;
  if (row == ROW_MODE || row == ROW_SHUFFLE) return tr(STR_SELECT);
  return images_.empty() ? "" : tr(STR_OPEN);
}

void WallpapersActivity::drawChrome() {
  GUI.drawHeader(renderer, UITheme::getInstance().getHeaderRect(renderer, true, false), tr(STR_QZ_WALLPAPERS));
}

void WallpapersActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabelText(), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
