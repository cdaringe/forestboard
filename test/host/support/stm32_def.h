#pragma once
#include <cstdint>
constexpr uint32_t FLASH_BASE = 0x08000000;
constexpr uint32_t FLASH_SECTOR_6 = 6, FLASH_SECTOR_7 = 7;
constexpr uint32_t FLASH_TYPEERASE_SECTORS = 0, FLASH_TYPEPROGRAM_WORD = 0;
constexpr uint32_t FLASH_VOLTAGE_RANGE_3 = 3;
constexpr uint32_t FLASH_FLAG_EOP = 1, FLASH_FLAG_OPERR = 2;
constexpr uint32_t FLASH_FLAG_WRPERR = 4, FLASH_FLAG_PGAERR = 8;
constexpr uint32_t FLASH_FLAG_PGPERR = 16, FLASH_FLAG_PGSERR = 32;
enum HAL_StatusTypeDef { HAL_OK, HAL_ERROR };
struct FLASH_EraseInitTypeDef {
  uint32_t TypeErase, Sector, NbSectors, VoltageRange;
};
inline void __HAL_FLASH_CLEAR_FLAG(uint32_t) {}
HAL_StatusTypeDef HAL_FLASH_Unlock();
HAL_StatusTypeDef HAL_FLASH_Lock();
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t, uint32_t, uint64_t);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef*, uint32_t*);
void FLASH_FlushCaches();
