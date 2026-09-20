#include "Arduino.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#define private public
#include "../../src/storage/keystroke_counter.cpp"
#undef private

namespace {
struct PowerLoss {};
int cutAt = -1;
int operationPoint = 0;
int programCalls = 0;
int eraseCalls = 0;
int cacheFlushes = 0;
int failedProgram = -1;
bool isLocked = true;
bool isPayloadCorrupted = false;
bool isEraseIncomplete = false;

void interruptPower() {
  if (operationPoint++ == cutAt) {
    throw PowerLoss{};
  }
}

uint32_t* flashWord(uint32_t address) {
  return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(address));
}

void resetFaults() {
  cutAt = -1;
  operationPoint = 0;
  programCalls = 0;
  eraseCalls = 0;
  cacheFlushes = 0;
  failedProgram = -1;
  isLocked = true;
  isPayloadCorrupted = false;
  isEraseIncomplete = false;
}

void clearFlash() {
  resetFaults();
  memset(flashWord(kJournalStart), 0xff, 2 * kSectorSize);
}

void seedRecord(uint32_t address, uint32_t count, uint32_t sequence = 1,
    uint32_t magic = kJournalMagic) {
  JournalRecord record{magic, sequence, count, 0};
  record.checksum = recordChecksum(record);
  memcpy(flashWord(address), &record, sizeof(record));
}

uint32_t recoveredCount() {
  resetFaults();
  KeystrokeCounter restored;
  restored.begin();
  return restored.count();
}

void saveCount(KeystrokeCounter& counter, uint32_t count) {
  counter.count_ = count - 1;
  counter.recordKeystroke();
}

void testCheckpointFrequency() {
  clearFlash();
  KeystrokeCounter counter;
  counter.begin();
  for (unsigned i = 1; i < 10000; ++i) {
    counter.recordKeystroke();
    assert(programCalls == 0 && eraseCalls == 0);
    if (i == 1 || i == 10 || i == 100 || i == 1000) {
      uint32_t milestone;
      assert(counter.takePendingMilestone(milestone) && milestone == i);
    }
  }
  counter.recordKeystroke();
  assert(programCalls == 16 && cacheFlushes == 16 && isLocked);
  for (unsigned i = 10000; i < 20000; ++i) {
    counter.recordKeystroke();
  }
  assert(programCalls == 32 && eraseCalls == 0);
  assert(recoveredCount() == 20000);
}

void testLegacyMigration() {
  clearFlash();
  seedRecord(sectorStart(1), 12100, 123, kLegacyMagic);
  KeystrokeCounter counter;
  counter.begin();
  assert(counter.count() == 12100);
  saveCount(counter, 20000);
  assert(readRecord(sectorStart(1) + 16).sequence == 124);
  assert(recoveredCount() == 20000);
}

void seedFullSector(uint8_t sector) {
  // Occupied invalid tail models a full journal, including torn records.
  memset(flashWord(sectorStart(sector)), 0, kSectorSize);
  seedRecord(sectorStart(sector), 10000);
}

void testRolloverPowerCut(uint8_t active, int cut) {
  clearFlash();
  seedFullSector(active);
  // Old contents of the destination must be erased before use.
  memset(flashWord(sectorStart(1 - active)), 0, kSectorSize);
  std::vector<uint8_t> original(kSectorSize);
  memcpy(original.data(), flashWord(sectorStart(active)), kSectorSize);
  KeystrokeCounter counter;
  counter.begin();
  cutAt = cut;
  try {
    saveCount(counter, 20000);
  } catch (const PowerLoss&) {
  }
  assert(memcmp(original.data(), flashWord(sectorStart(active)), kSectorSize) ==
      0);
  const uint32_t restored = recoveredCount();
  assert(restored == 10000 || restored == 20000);
  if (cut >= 51) {
    assert(restored == 20000);
  }
}

void testPowerLossAtEveryRolloverStep() {
  // Before/during/after erase, each payload word, and the commit word.
  for (uint8_t active : {0, 1}) {
    for (int cut = 0; cut <= 51; ++cut) {
      testRolloverPowerCut(active, cut);
    }
  }
}

void testPowerLossDuringAppend() {
  for (int cut = 0; cut <= 48; ++cut) {
    clearFlash();
    seedRecord(sectorStart(1), 10000);
    KeystrokeCounter counter;
    counter.begin();
    cutAt = cut;
    try {
      saveCount(counter, 20000);
    } catch (const PowerLoss&) {
    }
    const uint32_t restored = recoveredCount();
    assert(restored == 10000 || restored == 20000);
    KeystrokeCounter next;
    next.begin();
    saveCount(next, 30000);
    assert(recoveredCount() == 30000);
  }
}

void testFailedWriteSkipsSlot() {
  clearFlash();
  seedRecord(sectorStart(1), 10000);
  KeystrokeCounter counter;
  counter.begin();
  failedProgram = 2;
  saveCount(counter, 20000);
  assert(isLocked && readRecord(sectorStart(1) + 16).magic == UINT32_MAX);
  failedProgram = -1;
  saveCount(counter, 30000);
  Configuration config;
  assert(decodeSnapshot(
      readSnapshot(sectorStart(1) + 16 + kSnapshotSize), config));
  assert(recoveredCount() == 30000);
}

void testPayloadVerificationPrecedesCommit() {
  clearFlash();
  seedRecord(sectorStart(1), 10000);
  KeystrokeCounter counter;
  counter.begin();
  isPayloadCorrupted = true;
  saveCount(counter, 20000);
  assert(programCalls == 15); // Never issued the commit write.
  assert(readRecord(sectorStart(1) + 16).magic == UINT32_MAX);
  assert(recoveredCount() == 10000);
}

void testIncompleteEraseRetainsOldCheckpoint() {
  clearFlash();
  seedFullSector(1);
  memset(flashWord(sectorStart(0)), 0, kSectorSize);
  KeystrokeCounter counter;
  counter.begin();
  isEraseIncomplete = true;
  saveCount(counter, 20000);
  assert(programCalls == 0 && isLocked);
  assert(recoveredCount() == 10000);
}

void testCorruptNewestRecordFallsBack() {
  clearFlash();
  seedRecord(sectorStart(1), 10000);
  seedRecord(sectorStart(0), 20000, 2);
  *flashWord(sectorStart(0) + 12) ^= 1;
  assert(recoveredCount() == 10000);
}

void testFullCounterRange() {
  clearFlash();
  seedRecord(sectorStart(1), 4294960000U, 429496);
  assert(recoveredCount() == 4294960000U);
  KeystrokeCounter counter;
  counter.begin();
  counter.count_ = UINT32_MAX;
  counter.recordKeystroke();
  assert(counter.count() == UINT32_MAX && programCalls == 0);
}

Configuration changedConfiguration(uint32_t fps) {
  Configuration value;
  assert(value.set(Setting::fps, fps));
  assert(value.set(Setting::animationSeconds, 120));
  assert(value.set(Setting::refreshSeconds, 180));
  assert(value.set(Setting::contrast, 100));
  assert(value.set(Setting::idleSeconds, fps == 30 ? 0 : 120));
  assert(value.set(Setting::gameMode, 1));
  assert(value.set(Setting::showSplash, 1));
  assert(value.set(Setting::mtbSpeed, 90));
  assert(value.set(Setting::mtbJumpIntervalMs, 2500));
  assert(value.set(Setting::warpSpeed, 250));
  assert(value.set(Setting::curvedRings, 12));
  assert(value.set(Setting::mtbWheelieKey, 0x0b));
  return value;
}

void testConfigurationPersistence() {
  clearFlash();
  KeystrokeCounter counter;
  counter.begin();
  const Configuration changed = changedConfiguration(30);
  assert(counter.saveConfiguration(changed)); // Valid even at zero keystrokes.
  assert(programCalls == 16 && isLocked);
  assert(counter.saveConfiguration(changed));
  assert(programCalls == 16); // Unchanged saves do not wear flash.
  KeystrokeCounter boot;
  boot.begin();
  assert(boot.count() == 0 && boot.savedConfiguration() == changed);
  saveCount(boot, 10000);
  KeystrokeCounter next;
  next.begin();
  assert(next.count() == 10000 && next.savedConfiguration() == changed);
}

void testSettingsPowerCuts(bool rollover, uint8_t active) {
  const Configuration before = changedConfiguration(30);
  Configuration after = changedConfiguration(90);
  assert(after.set(Setting::showSplash, 0));
  for (int cut = 0; cut <= (rollover ? 51 : 48); ++cut) {
    clearFlash();
    KeystrokeCounter counter;
    counter.begin();
    counter.activeSector_ = active;
    counter.nextJournalAddress_ = sectorStart(active);
    assert(counter.saveConfiguration(before));
    counter.count_ = 1234;
    if (rollover) {
      memset(flashWord(sectorStart(active) + kSnapshotSize), 0,
          kSectorSize - kSnapshotSize);
      counter.nextJournalAddress_ = sectorEnd(active);
      memset(flashWord(sectorStart(1 - active)), 0, kSectorSize);
    }
    std::vector<uint8_t> original(kSectorSize);
    memcpy(original.data(), flashWord(sectorStart(active)), kSectorSize);
    resetFaults();
    cutAt = cut;
    try {
      counter.saveConfiguration(after);
    } catch (const PowerLoss&) {
    }
    if (rollover) {
      assert(memcmp(original.data(), flashWord(sectorStart(active)),
                 kSectorSize) == 0);
    }
    resetFaults();
    KeystrokeCounter restored;
    restored.begin();
    // A complete old or new snapshot, never mixed values or lost settings.
    if (restored.savedConfiguration() == before) {
      assert(restored.count() == 0);
    } else {
      assert(
          restored.savedConfiguration() == after && restored.count() == 1234);
    }
    if (cut == (rollover ? 51 : 48)) {
      assert(restored.savedConfiguration() == after);
    }
    const Configuration retry = changedConfiguration(45);
    assert(restored.saveConfiguration(retry));
    KeystrokeCounter retried;
    retried.begin();
    assert(retried.savedConfiguration() == retry);
  }
}

void testRejectsDuplicateBindingsBeforeWriting() {
  clearFlash();
  KeystrokeCounter counter;
  counter.begin();
  Configuration invalid;
  invalid.set(Setting::mtbWheelieKey, invalid.mtbBackflipKey());
  assert(!invalid.isValid() && !counter.saveConfiguration(invalid));
  assert(programCalls == 0 && eraseCalls == 0 && isLocked);
}

void testConfigurationWriteFailures() {
  clearFlash();
  KeystrokeCounter counter;
  counter.begin();
  Configuration original = changedConfiguration(30);
  assert(counter.saveConfiguration(original));
  resetFaults();
  failedProgram = 3;
  assert(!counter.saveConfiguration(changedConfiguration(90)));
  assert(counter.savedConfiguration() == original && isLocked);
  resetFaults();
  assert(counter.saveConfiguration(changedConfiguration(45)));
  KeystrokeCounter restored;
  restored.begin();
  assert(restored.savedConfiguration() == changedConfiguration(45));
}

void testSettingsValidationAndCorruption() {
  for (int corruption = 0; corruption < 5; ++corruption) {
    clearFlash();
    KeystrokeCounter counter;
    counter.begin();
    assert(counter.saveConfiguration(changedConfiguration(30)));
    assert(counter.saveConfiguration(changedConfiguration(90)));
    const uint32_t address = sectorStart(1) + kSnapshotSize;
    Snapshot broken = readSnapshot(address);
    if (corruption == 0) {
      broken.words[5] ^= 1; // CRC mismatch.
    }
    if (corruption == 1) {
      broken.words[5] = (broken.words[5] & 0xffff0000U) | 121;
      broken.words[3] = snapshotChecksum(broken);
    }
    if (corruption == 2) {
      broken.words[4] = 999;
      broken.words[3] = snapshotChecksum(broken);
    }
    if (corruption == 3 || corruption == 4) {
      const size_t index = static_cast<size_t>(Setting::mtbWheelieKey);
      const uint32_t value =
          corruption == 3 ? changedConfiguration(90).mtbBackflipKey() : 0x28;
      const uint32_t shift = (index % 2) * 16;
      broken.words[5 + index / 2] =
          (broken.words[5 + index / 2] & ~(0xffffU << shift)) |
          (value << shift);
      broken.words[3] = snapshotChecksum(broken);
    }
    memcpy(flashWord(address), &broken, sizeof(broken));
    KeystrokeCounter restored;
    restored.begin();
    assert(restored.savedConfiguration() == changedConfiguration(30));
    if (corruption == 2) {
      resetFaults();
      assert(!restored.saveConfiguration(changedConfiguration(45)));
      assert(
          programCalls == 0 && eraseCalls == 0); // Do not erase future formats.
    }
  }
}

void testResetStatsPowerCuts(bool rollover, uint8_t active) {
  const Configuration settings = changedConfiguration(30);
  for (int cut = 0; cut <= (rollover ? 51 : 48); ++cut) {
    clearFlash();
    KeystrokeCounter counter;
    counter.begin();
    counter.activeSector_ = active;
    counter.nextJournalAddress_ = sectorStart(active);
    counter.count_ = 12345;
    assert(counter.saveConfiguration(settings));
    if (rollover) {
      memset(flashWord(sectorStart(active) + kSnapshotSize), 0,
          kSectorSize - kSnapshotSize);
      counter.nextJournalAddress_ = sectorEnd(active);
      memset(flashWord(sectorStart(1 - active)), 0, kSectorSize);
    }
    resetFaults();
    cutAt = cut;
    try {
      counter.resetStats();
    } catch (const PowerLoss&) {
    }
    resetFaults();
    KeystrokeCounter restored;
    restored.begin();
    assert(restored.savedConfiguration() == settings);
    assert(restored.count() == 12345 || restored.count() == 0);
    if (cut == (rollover ? 51 : 48)) {
      assert(restored.count() == 0);
    }
    assert(restored.resetStats());
    assert(restored.count() == 0 && restored.savedConfiguration() == settings);
    KeystrokeCounter next;
    next.begin();
    assert(next.count() == 0 && next.savedConfiguration() == settings);
    next.recordKeystroke();
    assert(next.count() == 1);
  }
}

void testResetStatsFailure() {
  clearFlash();
  KeystrokeCounter counter;
  counter.begin();
  counter.count_ = 10;
  counter.pendingMilestone_ = 10;
  assert(counter.saveConfiguration(changedConfiguration(30)));
  resetFaults();
  failedProgram = 2;
  assert(!counter.resetStats());
  assert(counter.count() == 10 && counter.pendingMilestone_ == 10 && isLocked);
  resetFaults();
  assert(counter.resetStats() && counter.pendingMilestone_ == 0);
  KeystrokeCounter boot;
  boot.begin();
  assert(boot.count() == 0 &&
      boot.savedConfiguration() == changedConfiguration(30));
}

void testSequenceExhaustion() {
  clearFlash();
  KeystrokeCounter counter;
  counter.begin();
  counter.nextJournalSequence_ = UINT32_MAX;
  assert(counter.saveConfiguration(changedConfiguration(30)));
  resetFaults();
  KeystrokeCounter restored;
  restored.begin();
  assert(restored.savedConfiguration() == changedConfiguration(30));
  assert(!restored.saveConfiguration(changedConfiguration(45)));
  assert(programCalls == 0 && eraseCalls == 0);
}

void testPreviousSettingsVersionsMigration() {
  for (uint32_t version : {1U, 2U, 3U}) {
    clearFlash();
    Configuration original;
    const Configuration customized = changedConfiguration(30);
    const size_t oldCount = version == 1 ? 7 : version == 2 ? 8 : 21;
    Snapshot old{};
    old.words[0] = kSnapshotMagic;
    old.words[1] = 1;
    old.words[2] = 12345;
    old.words[4] = version;
    for (size_t i = 0; i < oldCount; ++i) {
      const Setting setting = static_cast<Setting>(i);
      if (version < 3) {
        old.words[5 + i] = customized.get(setting);
      } else {
        old.words[5 + i / 2] |= customized.get(setting) << ((i % 2) * 16);
      }
      original.set(setting, customized.get(setting));
    }
    old.words[3] = snapshotChecksum(old);
    memcpy(flashWord(sectorStart(1)), &old, sizeof(old));
    KeystrokeCounter migrated;
    migrated.begin();
    assert(
        migrated.count() == 12345 && migrated.savedConfiguration() == original);
    assert(migrated.isStorageWritable_ && programCalls == 0 && eraseCalls == 0);
    assert(migrated.savedConfiguration().idleSeconds() == 300);
    Configuration next = migrated.savedConfiguration();
    next.set(Setting::mtbSpeed, 80);
    next.set(Setting::warpSpeed, 250);
    next.set(Setting::mtbWheelieKey, 0x0b);
    next.set(Setting::idleSeconds, 120);
    assert(migrated.saveConfiguration(next));
    assert(readSnapshot(sectorStart(1) + kSnapshotSize).words[4] == 4);
    assert(memcmp(flashWord(sectorStart(1)), &old, sizeof(old)) == 0);
    KeystrokeCounter boot;
    boot.begin();
    assert(boot.count() == 12345 && boot.savedConfiguration() == next);
  }
}

void testLegacySettingsMigration() {
  for (uint32_t magic : {kLegacyMagic, kJournalMagic}) {
    clearFlash();
    seedRecord(sectorStart(1), 10000, 100, magic);
    KeystrokeCounter counter;
    counter.begin();
    assert(counter.count() == 10000 &&
        counter.savedConfiguration() == Configuration{});
    assert(counter.saveConfiguration(changedConfiguration(30)));
    KeystrokeCounter restored;
    restored.begin();
    assert(restored.count() == 10000 &&
        restored.savedConfiguration() == changedConfiguration(30));
  }
}

} // namespace

HAL_StatusTypeDef HAL_FLASH_Unlock() {
  isLocked = false;
  return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Lock() {
  isLocked = true;
  return HAL_OK;
}
void FLASH_FlushCaches() {
  ++cacheFlushes;
}

HAL_StatusTypeDef HAL_FLASH_Program(
    uint32_t, uint32_t address, uint64_t value) {
  assert(!isLocked);
  ++programCalls;
  interruptPower();
  uint32_t& word = *flashWord(address);
  assert(
      (word & value) == value); // Model NOR flash: only 1 -> 0 without erase.
  word &= static_cast<uint32_t>(value) | 0xFFFF0000U;
  interruptPower();
  if (programCalls == failedProgram) {
    return HAL_ERROR;
  }
  word &= value;
  if (isPayloadCorrupted && programCalls == 2) {
    word ^= 1;
  }
  interruptPower();
  return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* erase, uint32_t*) {
  assert(!isLocked && erase->NbSectors == 1);
  assert(erase->Sector == 6 || erase->Sector == 7);
  ++eraseCalls;
  uint32_t* start = flashWord(sectorStart(erase->Sector - 6));
  interruptPower();
  memset(start, 0xff, kSectorSize / 2);
  interruptPower();
  if (!isEraseIncomplete) {
    memset(start, 0xff, kSectorSize);
  }
  interruptPower();
  return HAL_OK;
}

int main() {
  void* base = reinterpret_cast<void*>(static_cast<uintptr_t>(FLASH_BASE));
  void* mapped = mmap(base, 512 * 1024, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  assert(mapped == base);
  testCheckpointFrequency();
  testLegacyMigration();
  testPowerLossAtEveryRolloverStep();
  testPowerLossDuringAppend();
  testFailedWriteSkipsSlot();
  testPayloadVerificationPrecedesCommit();
  testIncompleteEraseRetainsOldCheckpoint();
  testCorruptNewestRecordFallsBack();
  testFullCounterRange();
  testConfigurationPersistence();
  testSequenceExhaustion();
  testResetStatsFailure();
  testConfigurationWriteFailures();
  testRejectsDuplicateBindingsBeforeWriting();
  testSettingsValidationAndCorruption();
  testLegacySettingsMigration();
  testPreviousSettingsVersionsMigration();
  for (uint8_t active : {0, 1}) {
    testSettingsPowerCuts(false, active);
    testSettingsPowerCuts(true, active);
    testResetStatsPowerCuts(false, active);
    testResetStatsPowerCuts(true, active);
  }
  munmap(mapped, 512 * 1024);
  puts("Storage checkpoint and power-loss tests passed");
}
