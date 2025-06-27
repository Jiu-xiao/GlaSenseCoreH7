#pragma once
// Auto-generated Flash Layout Map
// MCU: STM32H750VBT6

#include "main.h"

#include "stm32_flash.hpp"

constexpr LibXR::FlashSector FLASH_SECTORS[] = {
  {0x08000000, 0x00020000},
};

constexpr size_t FLASH_SECTOR_NUMBER = sizeof(FLASH_SECTORS) / sizeof(LibXR::FlashSector);