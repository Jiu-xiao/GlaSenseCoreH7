#include "app_framework.hpp"
#include "libxr.hpp"

// Module headers
#include "BlinkLED.hpp"
#include "SensUS.hpp"
#include "ST7735.hpp"

static void XRobotMain(LibXR::HardwareContainer &hw) {
  using namespace LibXR;
  ApplicationManager appmgr;

  // Auto-generated module instantiations
  static BlinkLED blinkled(hw, appmgr, 250);
  static ST7735 ST7735_0(hw, appmgr, ST7735::PanelType::HANNSTAR_PANEL, ST7735::ScreenType::SCREEN_0_9, ST7735::Orientation::LANDSCAPE_ROT180, ST7735::PixelFormat::FORMAT_RGB565);
  static SensUS<1> SensUS_0(hw, appmgr, ST7735_0);

  while (true) {
    appmgr.MonitorAll();
    Thread::Sleep(1000);
  }
}