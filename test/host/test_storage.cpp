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
  assert(programCalls == 4 && cacheFlushes == 4 && isLocked);
  for (unsigned i = 10000; i < 20000; ++i) {
    counter.recordKeystroke();
  }
  assert(programCalls == 8 && eraseCalls == 0);
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
  if (cut >= 15) {
    assert(restored == 20000);
  }
}

void testPowerLossAtEveryRolloverStep() {
  // Before/during/after erase, each payload word, and the commit word.
  for (uint8_t active : {0, 1}) {
    for (int cut = 0; cut <= 15; ++cut) {
      testRolloverPowerCut(active, cut);
    }
  }
}

void testPowerLossDuringAppend() {
  for (int cut = 0; cut <= 12; ++cut) {
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
  assert(isValid(readRecord(sectorStart(1) + 32)));
  assert(recoveredCount() == 30000);
}

void testPayloadVerificationPrecedesCommit() {
  clearFlash();
  seedRecord(sectorStart(1), 10000);
  KeystrokeCounter counter;
  counter.begin();
  isPayloadCorrupted = true;
  saveCount(counter, 20000);
  assert(programCalls == 3); // Never issued the commit write.
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
  munmap(mapped, 512 * 1024);
  puts("Storage checkpoint and power-loss tests passed");
}
