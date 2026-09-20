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

constexpr uint32_t kSnapshotMagic =
    0x43464731; // CFG1: count + full configuration.
constexpr uint32_t kSnapshotVersion = 4;
constexpr uint32_t kSnapshotSize = 64;
static_assert(
    kSettingCount == 22, "Change journal version when changing the schema");
// Version 3 packs bounded settings into 16-bit slots, retaining the same
// 64-byte record size and CRC coverage so older firmware detects a future
// version.
static_assert(kSettingCount <= 22, "Snapshot settings capacity exceeded");
struct Snapshot {
  uint32_t words[kSnapshotSize / sizeof(uint32_t)];
};

struct SectorScan {
  uint32_t nextAddress;
  uint32_t sequence = 0;
  uint32_t count = 0;
  Configuration configuration;
  bool hasFutureVersion = false;
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

Snapshot readSnapshot(uint32_t address) {
  Snapshot snapshot{};
  const auto* source = reinterpret_cast<const volatile uint32_t*>(address);
  for (size_t i = 0; i < 16; ++i) {
    snapshot.words[i] = source[i];
  }
  return snapshot;
}

uint32_t snapshotChecksum(const Snapshot& snapshot) {
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t i = 0; i < 16; ++i) {
    if (i != 3) {
      crc = updateCrc32(crc, snapshot.words[i]);
    }
  }
  return ~crc;
}

bool decodeSnapshot(const Snapshot& snapshot, Configuration& config) {
  const uint32_t version = snapshot.words[4];
  if (snapshot.words[0] != kSnapshotMagic || snapshot.words[1] == 0 ||
      version < 1 || version > kSnapshotVersion ||
      snapshot.words[3] != snapshotChecksum(snapshot)) {
    return false;
  }
  config = Configuration{};
  const size_t storedSettingCount = version == 1 ? 7
      : version == 2                             ? 8
      : version == 3                             ? 21
                                                 : kSettingCount;
  for (size_t i = 0; i < storedSettingCount; ++i) {
    const uint32_t value = version < 3
        ? snapshot.words[5 + i]
        : (snapshot.words[5 + i / 2] >> ((i % 2) * 16)) & 0xffffU;
    if (!config.set(static_cast<Setting>(i), value)) {
      return false;
    }
  }
  if (version < 3) {
    for (size_t i = 5 + storedSettingCount; i < 16; ++i) {
      if (snapshot.words[i] != 0) {
        return false;
      }
    }
  } else {
    for (size_t i = storedSettingCount; i < 22; ++i) {
      if (((snapshot.words[5 + i / 2] >> ((i % 2) * 16)) & 0xffffU) != 0) {
        return false;
      }
    }
  }
  return config.isValid();
}

SectorScan scanSector(uint8_t sector) {
  SectorScan scan{sectorStart(sector)};
  for (uint32_t address = sectorStart(sector); address < sectorEnd(sector);
      address += sizeof(JournalRecord)) {
    const JournalRecord record = readRecord(address);
    if (!isErased(record)) {
      scan.nextAddress = address + sizeof(JournalRecord);
    }
    if (record.magic == kSnapshotMagic &&
        address + kSnapshotSize <= sectorEnd(sector)) {
      const Snapshot snapshot = readSnapshot(address);
      if (snapshot.words[4] > kSnapshotVersion &&
          snapshot.words[3] == snapshotChecksum(snapshot)) {
        scan.hasFutureVersion = true;
      }
      Configuration config;
      if (decodeSnapshot(snapshot, config)) {
        if (isNewer(record, scan)) {
          scan.sequence = record.sequence;
          scan.count = record.keystrokeCount;
          scan.configuration = config;
        }
        scan.nextAddress = address + kSnapshotSize;
        address += kSnapshotSize - sizeof(JournalRecord);
      }
    } else if (isValid(record) && isNewer(record, scan)) {
      scan.sequence = record.sequence;
      scan.count = record.keystrokeCount;
      scan.configuration = Configuration{};
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

bool appendSnapshot(uint32_t address, uint32_t sequence, uint32_t count,
    const Configuration& config) {
  Snapshot snapshot{};
  snapshot.words[0] = kSnapshotMagic;
  snapshot.words[1] = sequence;
  snapshot.words[2] = count;
  snapshot.words[4] = kSnapshotVersion;
  for (size_t i = 0; i < kSettingCount; ++i) {
    snapshot.words[5 + i / 2] |= config.get(static_cast<Setting>(i))
        << ((i % 2) * 16);
  }
  snapshot.words[3] = snapshotChecksum(snapshot);
  const Snapshot erased = readSnapshot(address);
  for (uint32_t word : erased.words) {
    if (word != UINT32_MAX) {
      return false;
    }
  }
  for (size_t i = 1; i < 16; ++i) {
    if (!programWord(address + i * 4, snapshot.words[i])) {
      return false;
    }
  }
  const Snapshot payload = readSnapshot(address);
  if (payload.words[0] != UINT32_MAX) {
    return false;
  }
  for (size_t i = 1; i < 16; ++i) {
    if (payload.words[i] != snapshot.words[i]) {
      return false;
    }
  }
  // Publish only after the complete versioned payload is read back.
  programWord(address, kSnapshotMagic);
  const Snapshot committed = readSnapshot(address);
  Configuration decoded;
  if (!decodeSnapshot(committed, decoded)) {
    return false;
  }
  for (size_t i = 0; i < 16; ++i) {
    if (committed.words[i] != snapshot.words[i]) {
      return false;
    }
  }
  return true;
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
  configuration_ = newest.configuration;
  isStorageWritable_ = !first.hasFutureVersion && !second.hasFutureVersion;
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

bool KeystrokeCounter::writeCheckpoint() {
  if (nextJournalSequence_ == 0) {
    return false; // Never wrap the journal sequence.
  }
  const bool isRollover =
      nextJournalAddress_ + kSnapshotSize > sectorEnd(activeSector_);
  const uint8_t targetSector = isRollover ? 1 - activeSector_ : activeSector_;
  if (isRollover && !eraseSector(targetSector)) {
    return false;
  }
  const uint32_t address =
      isRollover ? sectorStart(targetSector) : nextJournalAddress_;
  // A failed program can still change bits. Never retry that slot in place.
  if (!isRollover) {
    nextJournalAddress_ += kSnapshotSize;
  }
  if (!appendSnapshot(address, nextJournalSequence_, count_, configuration_)) {
    return false;
  }
  activeSector_ = targetSector;
  nextJournalAddress_ = address + kSnapshotSize;
  ++nextJournalSequence_;
  // Retain the previous sector until it is needed for the next rollover.
  return true;
}

bool KeystrokeCounter::checkpoint() {
  if (!isStorageWritable_) {
    return false;
  }
  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
      FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
  const bool success = writeCheckpoint();
  HAL_FLASH_Lock();
  return success;
}

bool KeystrokeCounter::saveConfiguration(const Configuration& value) {
  if (!value.isValid()) {
    return false;
  }
  if (!isStorageWritable_) {
    return false;
  }
  if (value == configuration_) {
    return true;
  }
  const Configuration previous = configuration_;
  configuration_ = value;
  if (checkpoint()) {
    return true;
  }
  configuration_ = previous;
  return false;
}

bool KeystrokeCounter::resetStats() {
  if (!isStorageWritable_) {
    return false;
  }
  if (count_ == 0) {
    return true;
  }
  const uint32_t previousCount = count_;
  count_ = 0;
  if (!checkpoint()) {
    count_ = previousCount;
    return false;
  }
  pendingMilestone_ = 0;
  return true;
}
