#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: SensUS
constructor_args:
  - screen: '@ST7735_0'
template_args:
  - FrameSizeIndex: 0
required_hardware: []
depends:
  - xrobot-org/ST7735
=== END MANIFEST === */
// clang-format on

#include "ST7735.hpp"
#include "app_framework.hpp"
#include "libxr_time.hpp"
#include "semaphore.hpp"
#include "thread.hpp"
#include <cstdint>

extern "C" {
#include "camera.h"
extern DCMI_HandleTypeDef hdcmi;
extern DMA_HandleTypeDef hdma_dcmi;
extern I2C_HandleTypeDef hi2c1;
}

#include "libxr_def.hpp"

static void (*dcmi_callback_fun)() = nullptr;

template <int FrameSizeIndex> class SensUS : public LibXR::Application {
public:
  enum class FrameSize : uint8_t {
    QQVGA = 0,
    QVGA = 1,
    VGA = 2,
    SVGA = 3,
    XGA = 4,
    SXGA = 5,
    NUMBER
  };

  static constexpr uint16_t RESOLUTIONS[static_cast<uint8_t>(
      FrameSize::NUMBER)][2] = {{160, 120}, {320, 240},  {640, 480},
                                {800, 600}, {1024, 768}, {1280, 1024}};

  static constexpr framesize_t CAMERA_FRAMESIZE_MAP[static_cast<uint8_t>(
      FrameSize::NUMBER)] = {FRAMESIZE_QQVGA, FRAMESIZE_QVGA, FRAMESIZE_VGA,
                             FRAMESIZE_SVGA,  FRAMESIZE_XGA,  FRAMESIZE_SXGA};

  static constexpr uint16_t QQVGA_WIDTH = 160;
  static constexpr uint16_t QQVGA_HEIGHT = 120;

  static constexpr uint16_t FrameWidth = RESOLUTIONS[FrameSizeIndex][0];
  static constexpr uint16_t FrameHeight = RESOLUTIONS[FrameSizeIndex][1];

  SensUS(LibXR::HardwareContainer &hw, LibXR::ApplicationManager &app,
         ST7735 &st7735_)
      : st7735_(&st7735_) {
    UNUSED(app);
    UNUSED(hw);
    self_ = this;
    dcmi_callback_fun = CameraReadyCallback;
    thread_.Create(this, ThreadFun, "SensUS", 4096,
                   LibXR::Thread::Priority::REALTIME);
  }

  static void ThreadFun(SensUS *self) {
    char text[20];
    Camera_Init_Device(&hi2c1, CAMERA_FRAMESIZE_MAP[FrameSizeIndex]);

    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)&picture_buffer_,
                       FrameWidth * FrameHeight * 2 / 4);
    while (1) {
      if (self->frame_ready_.Wait() == ErrorCode::OK) {
        SCB_InvalidateDCache_by_Addr(picture_buffer_, sizeof(picture_buffer_));
#if 1
        ScaleImage(reinterpret_cast<uint16_t *>(picture_buffer_), FrameWidth,
                   FrameHeight, self->display_buffer_, QQVGA_WIDTH,
                   QQVGA_HEIGHT);
#else
        CropToQQVGA(reinterpret_cast<uint16_t *>(picture_buffer_), FrameWidth,
                    FrameHeight, self->display_buffer_);
#endif
        self->st7735_->FillRGBRect(
            0, 0, reinterpret_cast<uint8_t *>(self->display_buffer_),
            self->st7735_->GetWidth(), self->st7735_->GetHeight());
        sprintf((char *)&text, "%dFPS", self->fps_);
        self->st7735_->ShowString(ST7735::Color::BLACK, ST7735::Color::WHITE, 5,
                                  5, 60, 16, 12, text);
      }
    }
  }

  void OnMonitor() override {}

private:
  static inline SensUS *self_;
  ST7735 *st7735_ = nullptr;

  uint32_t fps_ = 0;

  uint16_t display_buffer_[QQVGA_WIDTH * QQVGA_HEIGHT];

  __attribute__((section(".axi_ram"))) inline static uint16_t
      picture_buffer_[FrameWidth * FrameHeight];

  LibXR::Semaphore frame_ready_;

  LibXR::Thread thread_;

  static void CameraReadyCallback() {
    static LibXR::TimestampMS tick = 0;
    static uint32_t count = 0;

    if (LibXR::Timebase::GetMilliseconds() - tick >= 1000) {
      tick = LibXR::Timebase::GetMilliseconds();
      SensUS::self_->fps_ = count;
      count = 0;
    }
    count++;

    SensUS::self_->frame_ready_.PostFromCallback(true);
  }

  static void ScaleImage(uint16_t *src, uint16_t src_width, uint16_t src_height,
                         uint16_t *dst, uint16_t dst_width,
                         uint16_t dst_height) {
    float scale_x = static_cast<float>(src_width) / dst_width;
    float scale_y = static_cast<float>(src_height) / dst_height;

    for (uint16_t y = 0; y < dst_height; ++y) {
      for (uint16_t x = 0; x < dst_width; ++x) {
        uint16_t src_x = static_cast<uint16_t>(x * scale_x);
        uint16_t src_y = static_cast<uint16_t>(y * scale_y);
        dst[y * dst_width + x] = src[src_y * src_width + src_x];
      }
    }
  }

  static void CropToQQVGA(const uint16_t *src, uint16_t src_width,
                          uint16_t src_height, uint16_t *dst) {
    // 居中裁剪窗口起点
    uint16_t offset_x = (src_width - QQVGA_WIDTH) / 2;
    uint16_t offset_y = (src_height - QQVGA_HEIGHT) / 2;

    for (uint16_t y = 0; y < QQVGA_HEIGHT; ++y) {
      const uint16_t *src_row = &src[(offset_y + y) * src_width + offset_x];
      uint16_t *dst_row = &dst[y * QQVGA_WIDTH];
      memcpy(dst_row, src_row, QQVGA_WIDTH * sizeof(uint16_t));
    }
  }
};

// NOLINTNEXTLINE
extern "C" void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi) {
  UNUSED(hdcmi);

  if (dcmi_callback_fun != nullptr)
    dcmi_callback_fun();
}
