/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBS_CAMERA_CAMERA_H_
#define LIBS_CAMERA_CAMERA_H_

#include <cstdint>
#include <functional>
#include <list>

#include "libs/base/queue_task.h"
#include "libs/base/tasks.h"
#include "third_party/nxp/rt1176-sdk/devices/MIMXRT1176/drivers/fsl_csi.h"
#include "third_party/nxp/rt1176-sdk/devices/MIMXRT1176/drivers/fsl_lpi2c_freertos.h"

namespace coral::micro {

namespace camera {

enum class Mode : uint8_t {
    kStandBy = 0,
    kStreaming = 1,
    kTrigger = 5,
};

enum class RequestType : uint8_t {
    kEnable,
    kDisable,
    kFrame,
    kPower,
    kTestPattern,
    kDiscard,
};

struct FrameRequest {
    int index;
};

struct FrameResponse {
    int index;
};

struct PowerRequest {
    bool enable;
};

struct DiscardRequest {
    int count;
};

struct EnableResponse {
    bool success;
};

struct PowerResponse {
    bool success;
};

enum class TestPattern : uint8_t {
    kNone = 0x00,
    kColorBar = 0x01,
    kWalkingOnes = 0x11,
};

struct TestPatternRequest {
    TestPattern pattern;
};

struct Response {
    RequestType type;
    union {
        FrameResponse frame;
        EnableResponse enable;
        PowerResponse power;
    } response;
};

struct Request {
    RequestType type;
    union {
        FrameRequest frame;
        PowerRequest power;
        TestPatternRequest test_pattern;
        Mode mode;
        DiscardRequest discard;
    } request;
    std::function<void(Response)> callback;
};

enum class Format {
    kRgba,
    kRgb,
    kY8,
    kRaw,
};

enum class FilterMethod {
    kBilinear = 0,
    kNearestNeighbor,
};

// Clockwise rotations
enum class Rotation {
    k0,
    k90,
    k180,
    k270,
};

struct FrameFormat {
    Format fmt;
    FilterMethod filter = FilterMethod::kBilinear;
    Rotation rotation = Rotation::k0;
    int width;
    int height;
    bool preserve_ratio;
    uint8_t* buffer;
    bool white_balance = true;
};

}  // namespace camera

inline constexpr char kCameraTaskName[] = "camera_task";

class CameraTask
    : public QueueTask<camera::Request, camera::Response, kCameraTaskName,
                       configMINIMAL_STACK_SIZE * 10, CAMERA_TASK_PRIORITY,
                       /*QueueLength=*/4> {
   public:
    void Init(lpi2c_rtos_handle_t* i2c_handle);
    static CameraTask* GetSingleton() {
        static CameraTask camera;
        return &camera;
    }
    void Enable(camera::Mode mode);
    void Disable();
    // TODO(atv): Convert this to return a class that cleans up?
    int GetFrame(uint8_t** buffer, bool block);
    static bool GetFrame(const std::list<camera::FrameFormat>& fmts);
    void ReturnFrame(int index);
    bool SetPower(bool enable);
    void SetTestPattern(camera::TestPattern pattern);
    void Trigger();
    void DiscardFrames(int count);

    // CSI driver wants width to be divisible by 8, and 324 is not.
    // 324 * 324 == 13122 * 8 -- this makes the CSI driver happy!
    static constexpr size_t kCsiWidth = 8;
    static constexpr size_t kCsiHeight = 13122;
    static constexpr size_t kWidth = 324;
    static constexpr size_t kHeight = 324;

    static int FormatToBPP(camera::Format fmt);

   private:
    void TaskInit() override;
    void RequestHandler(camera::Request* req) override;
    camera::EnableResponse HandleEnableRequest(const camera::Mode& mode);
    void HandleDisableRequest();
    camera::PowerResponse HandlePowerRequest(const camera::PowerRequest& power);
    camera::FrameResponse HandleFrameRequest(const camera::FrameRequest& frame);
    void HandleTestPatternRequest(
        const camera::TestPatternRequest& test_pattern);
    void HandleDiscardRequest(const camera::DiscardRequest& discard);
    void SetMode(const camera::Mode& mode);
    bool Read(uint16_t reg, uint8_t* val);
    bool Write(uint16_t reg, uint8_t val);
    void SetDefaultRegisters();
    static void BayerToRGB(const uint8_t *camera_raw, uint8_t *camera_rgb, int width, int height, camera::FilterMethod filter, camera::Rotation rotation);
    static void BayerToRGBA(const uint8_t *camera_raw, uint8_t *camera_rgb, int width, int height, camera::FilterMethod filter, camera::Rotation rotation);
    static void BayerToGrayscale(const uint8_t *camera_raw, uint8_t *camera_grayscale, int width, int height, camera::FilterMethod filter, camera::Rotation rotation);
    static void RGBToGrayscale(const uint8_t *camera_rgb, uint8_t *camera_grayscale, int width, int height);
    static void AutoWhiteBalance(uint8_t* camera_rgb, int width, int height);
    static void ResizeNearestNeighbor(const uint8_t *src, int src_width, int src_height,
                                      uint8_t *dst, int dst_width, int dst_height,
                                      int components, bool preserve_aspect);

    static constexpr uint8_t kModelIdHExpected = 0x01;
    static constexpr uint8_t kModelIdLExpected = 0xB0;
    lpi2c_rtos_handle_t* i2c_handle_;
    csi_handle_t csi_handle_;
    csi_config_t csi_config_;
    camera::Mode mode_;
    camera::TestPattern test_pattern_;
};

}  // namespace coral::micro

#endif  // LIBS_CAMERA_CAMERA_H_
