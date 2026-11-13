#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>

class CrossPointState : public PersistableStore<CrossPointState> {
  CrossPointState() = default;

  friend class PersistableStore<CrossPointState>;

 public:
  static constexpr uint8_t SLEEP_AVOID_REPEAT_LOOKBACK = 5;

  std::string openEpubPath;
  // QuartzInk: a sleep image chosen to show every time instead of a random one.
  // In the overlay folder it is used by the overlay modes, otherwise by Custom.
  std::string pinnedSleepImage;
  uint32_t sleepLcgCount = 0;
  uint32_t sleepLcgState = 0;
  uint32_t sleepOverlayLcgCount = 0;
  uint32_t sleepOverlayLcgState = 0;
  uint8_t readerActivityLoadCount = 0;
  bool lastSleepFromReader = false;
  bool showBootScreen = true;

  static const char* getFilePath() { return "/.crosspoint/state.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  void getSleepLCG(uint32_t& seed, uint32_t& state, uint16_t& indexesLeft, uint16_t& indexesTotal) const;
  void getSleepOverlayLCG(uint32_t& seed, uint32_t& state, uint16_t& indexesLeft, uint16_t& indexesTotal) const;

  void setSleepLCG(uint32_t seed, uint32_t state, uint16_t indexesLeft, uint16_t indexesTotal);
  void setSleepOverlayLCG(uint32_t seed, uint32_t state, uint16_t indexesLeft, uint16_t indexesTotal);
};

#define APP_STATE CrossPointState::getInstance()
