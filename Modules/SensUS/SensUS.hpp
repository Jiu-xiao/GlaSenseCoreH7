#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: SensUS
constructor_args: []
template_args: []
required_hardware: []
depends: []
=== END MANIFEST === */
// clang-format on

#include "app_framework.hpp"
#include "thread.hpp"

extern "C" {
#include "camera.h"
#include "lcd.h"

#define PE3_Pin GPIO_PIN_3
#define PE3_GPIO_Port GPIOE
#define KEY_Pin GPIO_PIN_13
#define KEY_GPIO_Port GPIOC
#define LCD_CS_Pin GPIO_PIN_11
#define LCD_CS_GPIO_Port GPIOE
#define LCD_WR_RS_Pin GPIO_PIN_13
#define LCD_WR_RS_GPIO_Port GPIOE

extern DCMI_HandleTypeDef hdcmi;
extern DMA_HandleTypeDef hdma_dcmi;
extern I2C_HandleTypeDef hi2c1;

// QQVGA
#define FrameWidth 160
#define FrameHeight 120

__attribute__((section(".axi_ram"))) uint16_t pic[FrameWidth][FrameHeight];
uint32_t DCMI_FrameIsReady;
uint32_t Camera_FPS = 0;
}

#include "libxr_def.hpp"

extern "C" void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi) {
  static uint32_t count = 0, tick = 0;

  if (HAL_GetTick() - tick >= 1000) {
    tick = HAL_GetTick();
    Camera_FPS = count;
    count = 0;
  }
  count++;

  DCMI_FrameIsReady = 1;
}

void LED_Blink(uint32_t Hdelay, uint32_t Ldelay) {
  HAL_GPIO_WritePin(PE3_GPIO_Port, PE3_Pin, GPIO_PIN_SET);
  LibXR::Thread::Sleep(Hdelay);
  HAL_GPIO_WritePin(PE3_GPIO_Port, PE3_Pin, GPIO_PIN_RESET);
  LibXR::Thread::Sleep(Ldelay);
}

class SensUS : public LibXR::Application {
public:
  SensUS(LibXR::HardwareContainer &hw, LibXR::ApplicationManager &app) {
    // Hardware initialization example:
    // auto dev = hw.template Find<LibXR::GPIO>("led");
    UNUSED(app);
    UNUSED(hw);
    uint8_t text[20];
    LCD_Test();
    Camera_Init_Device(&hi2c1, FRAMESIZE_QQVGA);
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 58, ST7735Ctx.Width, 16, BLACK);
    while (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET) {

      sprintf((char *)&text, "Camera id:0x%x   ", hcamera.device_id);
      LCD_ShowString(0, 58, ST7735Ctx.Width, 16, 12, text);

      LED_Blink(5, 500);

      sprintf((char *)&text, "LongPress K1 to Run");
      LCD_ShowString(0, 58, ST7735Ctx.Width, 16, 12, text);

      LED_Blink(5, 500);
    }

    sprintf((char *)&text, "%p", pic);

    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)&pic,
                       FrameWidth * FrameHeight * 2 / 4);
    while (1) {
      if (DCMI_FrameIsReady) {
        DCMI_FrameIsReady = 0;
        SCB_InvalidateDCache_by_Addr(pic,
                                     sizeof(pic));
#ifdef TFT96
        ST7735_FillRGBRect(&st7735_pObj, 0, 0, (uint8_t *)&pic[20][0],
                           ST7735Ctx.Width, 80);
#elif TFT18
        ST7735_FillRGBRect(&st7735_pObj, 0, 0, (uint8_t *)&pic[0][0],
                           ST7735Ctx.Width, ST7735Ctx.Height);
#endif
        sprintf((char *)&text, "%dFPS", Camera_FPS);
        LCD_ShowString(5, 5, 60, 16, 12, text);
      } else {
        LibXR::Thread::Sleep(1);
      }
    }
  }

  void OnMonitor() override {}

private:
};
