#include "app_main.h"

#include "libxr.hpp"
#include "main.h"
#include "stm32_adc.hpp"
#include "stm32_can.hpp"
#include "stm32_canfd.hpp"
#include "stm32_gpio.hpp"
#include "stm32_i2c.hpp"
#include "stm32_power.hpp"
#include "stm32_pwm.hpp"
#include "stm32_spi.hpp"
#include "stm32_timebase.hpp"
#include "stm32_uart.hpp"
#include "stm32_usb.hpp"
#include "stm32_watchdog.hpp"
#include "flash_map.hpp"
#include "app_framework.hpp"
#include "xrobot_main.hpp"


using namespace LibXR;

/* User Code Begin 1 */
/* User Code End 1 */
/* External HAL Declarations */
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi4;
extern TIM_HandleTypeDef htim16;
extern TIM_HandleTypeDef htim17;
extern TIM_HandleTypeDef htim1;
extern USBD_HandleTypeDef hUsbDeviceFS;
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* DMA Resources */
static uint8_t spi4_tx_buf[51200] __attribute__((section(".ram_d3")));
static uint8_t i2c1_buf[32] __attribute__((section(".ram_d3")));

extern "C" void app_main(void) {
  /* User Code Begin 2 */
  
  /* User Code End 2 */
  STM32TimerTimebase timebase(&htim17);
  PlatformInit(2, 1024);
  STM32PowerManager power_manager;

  /* GPIO Configuration */
  STM32GPIO PC13(GPIOC, GPIO_PIN_13);
  STM32GPIO LCD_CS(LCD_CS_GPIO_Port, LCD_CS_Pin);
  STM32GPIO LCD_WR_RS(LCD_WR_RS_GPIO_Port, LCD_WR_RS_Pin);
  STM32GPIO LED(LED_GPIO_Port, LED_Pin);


  STM32PWM pwm_tim1_ch2n(&htim1, TIM_CHANNEL_2, true);

  STM32SPI spi4(&hspi4, {nullptr, 0}, spi4_tx_buf, 3);

  STM32I2C i2c1(&hi2c1, i2c1_buf, 3);

  STM32VirtualUART uart_cdc(hUsbDeviceFS, UserTxBufferFS, UserRxBufferFS, 5);
  STDIO::read_ = uart_cdc.read_port_;
  STDIO::write_ = uart_cdc.write_port_;
  RamFS ramfs("XRobot");
  Terminal<32, 32, 5, 5> terminal(ramfs);
  LibXR::Thread term_thread;
  term_thread.Create(&terminal, terminal.ThreadFun, "terminal", 1024,
                     static_cast<LibXR::Thread::Priority>(3));


  LibXR::HardwareContainer peripherals{
    LibXR::Entry<LibXR::PowerManager>({power_manager, {"power_manager"}}),
    LibXR::Entry<LibXR::GPIO>({PC13, {"PC13"}}),
    LibXR::Entry<LibXR::GPIO>({LCD_CS, {"LCD_CS", "st7735_spi_cs"}}),
    LibXR::Entry<LibXR::GPIO>({LCD_WR_RS, {"LCD_WR_RS", "st7735_spi_rs"}}),
    LibXR::Entry<LibXR::GPIO>({LED, {"LED"}}),
    LibXR::Entry<LibXR::PWM>({pwm_tim1_ch2n, {"pwm_tim1_ch2n", "st7735_pwm"}}),
    LibXR::Entry<LibXR::SPI>({spi4, {"spi4", "st7735_spi"}}),
    LibXR::Entry<LibXR::I2C>({i2c1, {"i2c1"}}),
    LibXR::Entry<LibXR::UART>({uart_cdc, {"uart_cdc"}}),
    LibXR::Entry<LibXR::RamFS>({ramfs, {"ramfs"}}),
    LibXR::Entry<LibXR::Terminal<32, 32, 5, 5>>({terminal, {"terminal"}})
  };

  /* User Code Begin 3 */
  XRobotMain(peripherals);
  /* User Code End 3 */
}