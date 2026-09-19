#include "storage/keystroke_counter.h"

#include <stm32_def.h>

namespace {

// Separate erase sectors are essential: never erase the latest committed copy.
// Both supported 512 KiB parts have 128 KiB sectors 6 and 7 at these offsets.
constexpr uint32_t kSectorSize = 128U * 1024U;
constexpr uint32_t kJournalStart = FLASH_BASE + 256U * 1024U;
constexpr uint32_t kSectorIds[] = {FLASH_SECTOR_6, FLASH_SECTOR_7};
constexpr uint32_t kLegacyMagic = 0x4B455932;  // KEY2, old 100-key checkpoints.
constexpr uint32_t kJournalMagic = 0x4B455933; // KEY3, 10,000-key checkpoints.
constexpr uint32_t kCheckpointInterval = 10000;

struct JournalRecord {
  uint32_t magic;
  uint32_t sequence;
  uint32_t keystrokeCount;
  uint32_t checksum;
};
static_assert(sizeof(JournalRecord) == 16, "Journal records are four words");

struct SectorScan {
  uint32_t nextAddress;
  uint32_t sequence = 0;
  uint32_t count = 0;
};

uint32_t sectorStart(uint8_t sector) {
  return kJournalStart + sector * kSectorSize;
}

uint32_t sectorEnd(uint8_t sector) {
  return sectorStart(sector) + kSectorSize;
}

uint32_t updateCrc32(uint32_t crc, uint32_t value) {
  for (uint8_t byteIndex = 0; byteIndex < sizeof(value); ++byteIndex) {
    crc ^= value & 0xFFU;
    value >>= 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint32_t lowBitMask = 0U - (crc & 1U);
      crc = (crc >> 1) ^ (0xEDB88320U & lowBitMask);
    }
  }
  return crc;
}

uint32_t recordChecksum(const JournalRecord& record) {
  uint32_t crc = updateCrc32(0xFFFFFFFFU, record.magic);
  crc = updateCrc32(crc, record.sequence);
  return ~updateCrc32(crc, record.keystrokeCount);
}

JournalRecord readRecord(uint32_t address) {
  const auto* words = reinterpret_cast<const volatile uint32_t*>(address);
  return {words[0], words[1], words[2], words[3]};
}

bool isErased(const JournalRecord& record) {
  return record.magic == UINT32_MAX && record.sequence == UINT32_MAX &&
      record.keystrokeCount == UINT32_MAX && record.checksum == UINT32_MAX;
}

bool isLegacyValueValid(const JournalRecord& record) {
  const uint32_t count = record.keystrokeCount;
  const bool isCheckpoint = count == 1 || count == 10 || count % 100 == 0;
  const uint32_t maximumSequence =
      count < 100 ? (count == 1 ? 1U : 2U) : count / 100 + 2;
  return isCheckpoint && count <= 1000000000UL &&
      record.sequence <= maximumSequence;
}

bool isValueValid(const JournalRecord& record) {
  if (record.keystrokeCount == 0 || record.sequence == 0 ||
      record.sequence > record.keystrokeCount) {
    return false;
  }
  if (record.magic == kLegacyMagic) {
    return isLegacyValueValid(record);
  }
  return record.magic == kJournalMagic &&
      record.keystrokeCount % kCheckpointInterval == 0;
}

bool isValid(const JournalRecord& record) {
  return isValueValid(record) && record.checksum == recordChecksum(record);
}

bool isNewer(const JournalRecord& record, const SectorScan& scan) {
  if (record.sequence != scan.sequence) {
    return record.sequence > scan.sequence;
  }
  return record.keystrokeCount > scan.count;
}

SectorScan scanSector(uint8_t sector) {
  SectorScan scan{sectorStart(sector)};
  for (uint32_t address = sectorStart(sector); address < sectorEnd(sector);
      address += sizeof(JournalRecord)) {
    const JournalRecord record = readRecord(address);
    if (!isErased(record)) {
      scan.nextAddress = address + sizeof(JournalRecord);
    }
    if (isValid(record) && isNewer(record, scan)) {
      scan.sequence = record.sequence;
      scan.count = record.keystrokeCount;
    }
  }
  return scan;
}

bool isSectorErased(uint8_t sector) {
  for (uint32_t address = sectorStart(sector); address < sectorEnd(sector);
      address += sizeof(JournalRecord)) {
    if (!isErased(readRecord(address))) {
      return false;
    }
  }
  return true;
}

bool eraseSector(uint8_t sector) {
  FLASH_EraseInitTypeDef erase = {};
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Sector = kSectorIds[sector];
  erase.NbSectors = 1;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  uint32_t sectorError = 0;
  const auto result = HAL_FLASHEx_Erase(&erase, &sectorError);
  FLASH_FlushCaches();
  return result == HAL_OK && isSectorErased(sector);
}

bool programWord(uint32_t address, uint32_t value) {
  const auto result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, value);
  // Polling HAL programming does not invalidate the STM32F4 flash data cache.
  FLASH_FlushCaches();
  return result == HAL_OK;
}

bool isPayloadVerified(uint32_t address, const JournalRecord& expected) {
  const JournalRecord actual = readRecord(address);
  return actual.magic == UINT32_MAX && actual.sequence == expected.sequence &&
      actual.keystrokeCount == expected.keystrokeCount &&
      actual.checksum == expected.checksum;
}

bool appendJournalRecord(uint32_t address, uint32_t sequence, uint32_t count) {
  if (!isErased(readRecord(address))) {
    return false;
  }
  JournalRecord record{kJournalMagic, sequence, count, 0};
  record.checksum = recordChecksum(record);
  const bool isWritten = programWord(address + 4, sequence) &&
      programWord(address + 8, count) &&
      programWord(address + 12, record.checksum);
  if (!isWritten || !isPayloadVerified(address, record)) {
    return false;
  }
  // Promotion is the final word. No separate mutable active-sector pointer.
  programWord(address, kJournalMagic);
  const JournalRecord committed = readRecord(address);
  return isValid(committed) && committed.sequence == sequence &&
      committed.keystrokeCount == count;
}

} // namespace

void KeystrokeCounter::begin() {
  const SectorScan first = scanSector(0);
  const SectorScan second = scanSector(1);
  // Prefer sector 7 on an empty journal, matching the previous firmware.
  const bool isFirstNewer = first.sequence > second.sequence ||
      (first.sequence == second.sequence && first.count > second.count);
  activeSector_ = isFirstNewer ? 0 : 1;
  const SectorScan& newest = isFirstNewer ? first : second;
  count_ = newest.count;
  nextJournalSequence_ = newest.sequence + 1;
  nextJournalAddress_ = newest.nextAddress;
  pendingMilestone_ = 0;
}

void KeystrokeCounter::recordKeystroke() {
  if (count_ == UINT32_MAX) {
    return;
  }
  ++count_;
  if (isMilestone(count_)) {
    pendingMilestone_ = count_;
  }
  if (isCheckpointDue(count_)) {
    checkpoint();
  }
}

uint32_t KeystrokeCounter::count() const {
  return count_;
}

bool KeystrokeCounter::takePendingMilestone(uint32_t& milestoneCount) {
  if (pendingMilestone_ == 0) {
    return false;
  }
  milestoneCount = pendingMilestone_;
  pendingMilestone_ = 0;
  return true;
}

bool KeystrokeCounter::isMilestone(uint32_t count) {
  if (count == 0) {
    return false;
  }
  while (count % 10 == 0) {
    count /= 10;
  }
  return count == 1;
}

bool KeystrokeCounter::isCheckpointDue(uint32_t count) {
  return count != 0 && count % kCheckpointInterval == 0;
}

void KeystrokeCounter::writeCheckpoint() {
  const bool isRollover = nextJournalAddress_ >= sectorEnd(activeSector_);
  const uint8_t targetSector = isRollover ? 1 - activeSector_ : activeSector_;
  if (isRollover && !eraseSector(targetSector)) {
    return;
  }
  const uint32_t address =
      isRollover ? sectorStart(targetSector) : nextJournalAddress_;
  // A failed program can still change bits. Never retry that slot in place.
  if (!isRollover) {
    nextJournalAddress_ += sizeof(JournalRecord);
  }
  if (!appendJournalRecord(address, nextJournalSequence_, count_)) {
    return;
  }
  activeSector_ = targetSector;
  nextJournalAddress_ = address + sizeof(JournalRecord);
  ++nextJournalSequence_;
  // Retain the previous sector until it is needed for the next rollover.
}

void KeystrokeCounter::checkpoint() {
  if (HAL_FLASH_Unlock() != HAL_OK) {
    return;
  }
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
      FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
  writeCheckpoint();
  HAL_FLASH_Lock();
}
