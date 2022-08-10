// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdio>

#include "libs/base/ipc_m4.h"
#include "libs/base/led.h"
#include "libs/base/main_freertos_m4.h"
#include "libs/camera/camera.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/freertos_kernel/include/timers.h"
#include "third_party/nxp/rt1176-sdk/devices/MIMXRT1176/drivers/fsl_gpio.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/examples/person_detection/detection_responder.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/examples/person_detection/main_functions.h"

namespace {
bool volatile g_person_detected = false;
bool g_status_led_state = true;
}  // namespace

void RespondToDetection(tflite::ErrorReporter* error_reporter,
                        int8_t person_score, int8_t no_person_score) {
  printf("person_score: %d no_person_score: %d\r\n", person_score,
         no_person_score);
// For normal operation, use the person score to determine detection.
// In the demo version, this is handled by a constant timer.
#if !defined(MULTICORE_MODEL_CASCADE_DEMO)
  g_person_detected = (person_score > no_person_score);
#endif  // !defined(MULTICORE_MODEL_CASCADE_DEMO)
}

extern "C" void app_main(void* param) {
  (void)param;

  coralmicro::IpcM4::GetSingleton()->RegisterAppMessageHandler(
      [handle = xTaskGetCurrentTaskHandle()](const uint8_t data[]) {
        vTaskResume(handle);
      });
  coralmicro::CameraTask::GetSingleton()->Init(I2C5Handle());
  coralmicro::CameraTask::GetSingleton()->SetPower(false);
  vTaskDelay(pdMS_TO_TICKS(100));
  coralmicro::CameraTask::GetSingleton()->SetPower(true);
  setup();

  coralmicro::LedSet(coralmicro::Led::kStatus, g_status_led_state);
  auto status_led_timer = xTimerCreate(
      "status_led_timer", pdMS_TO_TICKS(1000), pdTRUE, nullptr,
      +[](TimerHandle_t xTimer) {
        g_status_led_state = !g_status_led_state;
        coralmicro::LedSet(coralmicro::Led::kStatus, g_status_led_state);
      });
  xTimerStart(status_led_timer, 0);

#if defined(MULTICORE_MODEL_CASCADE_DEMO)
  TimerHandle_t m4_timer = xTimerCreate(
      "m4_timer", pdMS_TO_TICKS(10000), pdTRUE, nullptr,
      +[](TimerHandle_t xTimer) { g_person_detected = true; });
  xTimerStart(m4_timer, 0);
#endif  // defined(MULTICORE_MODEL_CASCADE_DEMO)

  while (true) {
    printf("M4 main loop\r\n");
    coralmicro::CameraTask::GetSingleton()->Enable(
        coralmicro::CameraMode::kStreaming);
    g_person_detected = false;

    while (true) {
      loop();
      coralmicro::LedSet(coralmicro::Led::kUser, g_person_detected);

      if (g_person_detected) {
        break;
      }
    }
    printf("Person detected, let M7 take over.\r\n");
    coralmicro::CameraTask::GetSingleton()->Disable();
    coralmicro::IpcMessage msg;
    msg.type = coralmicro::IpcMessageType::kApp;
    coralmicro::IpcM4::GetSingleton()->SendMessage(msg);
    vTaskSuspend(nullptr);
  }
}