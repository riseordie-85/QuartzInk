#pragma once

#include <functional>
#include <string>

#include "MappedInputManager.h"
#include "activities/Activity.h"

class BmpViewerActivity final : public Activity {
 public:
  // returnOnBack: Back finishes back to the caller (the Wallpapers screen)
  // instead of opening the file browser.
  BmpViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string filePath,
                    bool returnOnBack = false);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  void loadSiblingImages();
  void doSetSleepCover();
  bool canSetSleepCover() const;
  // Images in the sleep folders are pinned in place rather than copied.
  bool inSleepFolder() const;
  void togglePin();
  const char* confirmLabel() const;
  bool renderImage();
  bool saveJpegSleepCover();

  std::string filePath;
  std::vector<std::string> siblingImages;
  int currentImageIndex = -1;
  bool returnOnBack = false;
};
