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
#include "timebase.hpp"
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

  static constexpr uint16_t DISPLAY_WIDTH = 160;
  static constexpr uint16_t DISPLAY_HEIGHT = 80;

  static constexpr uint16_t FRAME_WIDTH = RESOLUTIONS[FrameSizeIndex][0];
  static constexpr uint16_t FRAME_HEIGHT = RESOLUTIONS[FrameSizeIndex][1];

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
    static char text[1024];
    Camera_Init_Device(&hi2c1, CAMERA_FRAMESIZE_MAP[FrameSizeIndex]);
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT,
                       (uint32_t)picture_buffer_raw_[self->buffer_index_],
                       FRAME_WIDTH * FRAME_HEIGHT * 2 / 4);
    while (1) {
      if (self->frame_ready_.Wait() == ErrorCode::OK) {
        SCB_InvalidateDCache_by_Addr(picture_buffer_raw_[self->buffer_index_],
                                     sizeof(picture_buffer_raw_[0]));
        self->MakeAreaToGray();
        static LibXR::MillisecondTimestamp last_frame_time = 0;
        if (LibXR::Timebase::GetMilliseconds() - last_frame_time >= 40) {
          last_frame_time = LibXR::Timebase::GetMilliseconds();
        } else {
          continue;
        }
#if 1
        ScaleImage(reinterpret_cast<uint16_t *>(
                       picture_buffer_raw_[self->buffer_index_]),
                   FRAME_WIDTH, FRAME_HEIGHT, self->display_buffer_,
                   DISPLAY_WIDTH, DISPLAY_HEIGHT);
#else
        CropToQQVGA(reinterpret_cast<uint16_t *>(
                        picture_buffer_raw_[self->buffer_index_]),
                    FrameWidth, FrameHeight, self->display_buffer_);
#endif
        self->MarkArea();
        self->st7735_->FillRGBRect(
            0, 0, reinterpret_cast<uint8_t *>(self->display_buffer_),
            self->st7735_->GetWidth(), self->st7735_->GetHeight());
        sprintf((char *)&text, "%dFPS R%3.4f D%3.4f", self->fps_,
                self->ref_average_light_, self->det_average_light_);
        self->st7735_->ShowString(ST7735::Color::BLACK, ST7735::Color::WHITE, 2,
                                  0, self->st7735_->GetWidth(), 16, 12, text);
        LibXR::STDIO::Printf("%f,%f\n", self->ref_average_light_,
                             self->det_average_light_);
      }
    }
  }

  void OnMonitor() override {}

private:
  static inline SensUS *self_;
  ST7735 *st7735_ = nullptr;

  uint32_t fps_ = 0;
  float reference_area_start_x_ = 0.2f;
  float reference_area_start_y_ = 0.3f;
  float reference_area_end_x_ = 0.4f;
  float reference_area_end_y_ = 0.7f;

  float detection_area_start_x_ = 0.6f;
  float detection_area_start_y_ = 0.3f;
  float detection_area_end_x_ = 0.8f;
  float detection_area_end_y_ = 0.7f;

  double ref_average_light_ = 0;
  double det_average_light_ = 0;

  uint16_t display_buffer_[DISPLAY_WIDTH * DISPLAY_HEIGHT];

  uint8_t buffer_index_ = false;

  __attribute__((section(".axi_ram"))) inline static uint16_t
      picture_buffer_raw_[2][FRAME_WIDTH * FRAME_HEIGHT];

  LibXR::Semaphore frame_ready_;

  LibXR::Thread thread_;

  static void CameraReadyCallback() {
    static LibXR::MillisecondTimestamp tick = 0;
    static uint32_t count = 0;

    if (LibXR::Timebase::GetMilliseconds() - tick >= 1000) {
      tick = LibXR::Timebase::GetMilliseconds();
      SensUS::self_->fps_ = count;
      count = 0;
    }
    count++;

    if (picture_buffer_raw_[self_->buffer_index_] == picture_buffer_raw_[0]) {
      HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT,
                         (uint32_t)picture_buffer_raw_[0],
                         FRAME_WIDTH * FRAME_HEIGHT * 2 / 4);
    } else {
      HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT,
                         (uint32_t)picture_buffer_raw_[1],
                         FRAME_WIDTH * FRAME_HEIGHT * 2 / 4);
    }

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
    uint16_t offset_x = (src_width - DISPLAY_WIDTH) / 2;
    uint16_t offset_y = (src_height - DISPLAY_HEIGHT) / 2;

    for (uint16_t y = 0; y < DISPLAY_HEIGHT; ++y) {
      const uint16_t *src_row = &src[(offset_y + y) * src_width + offset_x];
      uint16_t *dst_row = &dst[y * DISPLAY_WIDTH];
      memcpy(dst_row, src_row, DISPLAY_WIDTH * sizeof(uint16_t));
    }
  }

  inline uint16_t RGB565ToGray(uint16_t rgb565, uint8_t &gray) {
    rgb565 = (rgb565 >> 8) | (rgb565 << 8);
    uint8_t r8 = ((rgb565 >> 11) & 0x1F) << 3;
    uint8_t g8 = ((rgb565 >> 5) & 0x3F) << 2;
    uint8_t b8 = (rgb565 & 0x1F) << 3;
    gray = (uint8_t)((r8 * 299 + g8 * 587 + b8 * 114) / 1000);
    auto ans = ((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3);
    return (ans << 8) | (ans >> 8);
  }

  void MakeAreaToGray() {
    uint16_t ref_start_x =
        static_cast<uint16_t>(reference_area_start_x_ * FRAME_WIDTH);
    uint16_t ref_start_y =
        static_cast<uint16_t>(reference_area_start_y_ * FRAME_HEIGHT);
    uint16_t ref_end_x =
        static_cast<uint16_t>(reference_area_end_x_ * FRAME_WIDTH);
    uint16_t ref_end_y =
        static_cast<uint16_t>(reference_area_end_y_ * FRAME_HEIGHT);
    uint64_t gray_sum = 0;
    uint8_t gray = 0;
    for (uint16_t y = ref_start_y; y < ref_end_y; ++y) {
      uint16_t *src_row = &picture_buffer_raw_[buffer_index_][y * FRAME_WIDTH];
      for (uint16_t x = ref_start_x; x < ref_end_x; ++x) {
        src_row[x] = RGB565ToGray(src_row[x], gray);
        gray_sum += gray;
      }
    }

    ref_average_light_ = static_cast<double>(gray_sum) /
                         (ref_end_x - ref_start_x) / (ref_end_y - ref_start_y);

    gray_sum = 0;

    uint16_t det_start_x =
        static_cast<uint16_t>(detection_area_start_x_ * FRAME_WIDTH);
    uint16_t det_start_y =
        static_cast<uint16_t>(detection_area_start_y_ * FRAME_HEIGHT);
    uint16_t det_end_x =
        static_cast<uint16_t>(detection_area_end_x_ * FRAME_WIDTH);
    uint16_t det_end_y =
        static_cast<uint16_t>(detection_area_end_y_ * FRAME_HEIGHT);
    for (uint16_t y = det_start_y; y < det_end_y; ++y) {
      uint16_t *src_row = &picture_buffer_raw_[buffer_index_][y * FRAME_WIDTH];
      for (uint16_t x = det_start_x; x < det_end_x; ++x) {
        src_row[x] = RGB565ToGray(src_row[x], gray);
        gray_sum += gray;
      }
    }

    det_average_light_ = static_cast<double>(gray_sum) /
                         (det_end_x - det_start_x) / (det_end_y - det_start_y);
  }

  void MarkArea() {
    uint16_t ref_start_x =
        static_cast<uint16_t>(reference_area_start_x_ * DISPLAY_WIDTH);
    uint16_t ref_start_y =
        static_cast<uint16_t>(reference_area_start_y_ * DISPLAY_HEIGHT);
    uint16_t ref_end_x =
        static_cast<uint16_t>(reference_area_end_x_ * DISPLAY_WIDTH);
    uint16_t ref_end_y =
        static_cast<uint16_t>(reference_area_end_y_ * DISPLAY_HEIGHT);

    for (uint16_t y = ref_start_y; y < ref_end_y; ++y) {
      uint16_t *dst_row = &display_buffer_[y * DISPLAY_WIDTH];
      for (uint16_t x = ref_start_x; x < ref_end_x; ++x) {
        if (y == ref_start_y || y == ref_end_y - 1 || x == ref_start_x ||
            x == ref_end_x - 1) {
          dst_row[x] = ST7735::Color::RED;
        }
      }
    }

    uint16_t det_start_x =
        static_cast<uint16_t>(detection_area_start_x_ * DISPLAY_WIDTH);
    uint16_t det_start_y =
        static_cast<uint16_t>(detection_area_start_y_ * DISPLAY_HEIGHT);
    uint16_t det_end_x =
        static_cast<uint16_t>(detection_area_end_x_ * DISPLAY_WIDTH);
    uint16_t det_end_y =
        static_cast<uint16_t>(detection_area_end_y_ * DISPLAY_HEIGHT);

    for (uint16_t y = det_start_y; y < det_end_y; ++y) {
      uint16_t *dst_row = &display_buffer_[y * DISPLAY_WIDTH];
      for (uint16_t x = det_start_x; x < det_end_x; ++x) {
        if (y == det_start_y || y == det_end_y - 1 || x == det_start_x ||
            x == det_end_x - 1) {
          dst_row[x] = ST7735::Color::GREEN;
        }
      }
    }
  }
};

// NOLINTNEXTLINE
extern "C" void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi) {
  UNUSED(hdcmi);

  if (dcmi_callback_fun != nullptr)
    dcmi_callback_fun();
}
