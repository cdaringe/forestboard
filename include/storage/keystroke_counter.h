#pragma once

#include <Arduino.h>

class KeystrokeCounter {
public:
  void begin();
  void recordKeystroke();

  uint32_t count() const;
  bool takePendingMilestone(uint32_t& milestoneCount);

private:
  static bool isMilestone(uint32_t count);
  static bool isCheckpointDue(uint32_t count);
  void checkpoint();
  void writeCheckpoint();

  uint8_t activeSector_ = 1;
  uint32_t count_ = 0;
  uint32_t nextJournalSequence_ = 1;
  uint32_t nextJournalAddress_ = 0;
  uint32_t pendingMilestone_ = 0;
};
