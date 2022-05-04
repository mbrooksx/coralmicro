#include <cstdio>

#include "libs/base/ipc_m4.h"
#include "libs/base/led.h"
#include "libs/tasks/CameraTask/camera_task.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/freertos_kernel/include/timers.h"
#include "third_party/nxp/rt1176-sdk/devices/MIMXRT1176/drivers/fsl_gpio.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/examples/person_detection/detection_responder.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/examples/person_detection/main_functions.h"

static bool g_person_detected = false;
static bool g_power_led_state = true;

void RespondToDetection(tflite::ErrorReporter* error_reporter,
                        int8_t person_score, int8_t no_person_score) {
    TF_LITE_REPORT_ERROR(error_reporter, "person_score: %d no_person_score: %d",
                         person_score, no_person_score);

// For normal operation, use the person score to determine detection.
// In the OOBE demo, this is handled by a constant timer.
#if !defined(OOBE_DEMO)
    GPIO_PinWrite(GPIO13, 6, person_score > no_person_score);
    g_person_detected = (person_score > no_person_score);
#endif  // !defined(OOBE_DEMO)
}

static void HandleAppMessage(
    const uint8_t data[coral::micro::ipc::kMessageBufferDataSize],
    void* param) {
    (void)data;
    vTaskResume(reinterpret_cast<TaskHandle_t>(param));
}

extern "C" void app_main(void* param) {
    (void)param;
    coral::micro::IPCM4::GetSingleton()->RegisterAppMessageHandler(
        HandleAppMessage, xTaskGetCurrentTaskHandle());
    coral::micro::CameraTask::GetSingleton()->SetPower(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    coral::micro::CameraTask::GetSingleton()->SetPower(true);
    setup();
    GPIO_PinWrite(GPIO13, 6, 1);
    coral::micro::led::Set(coral::micro::led::LED::kPower, g_power_led_state);
    auto power_led_timer =
        xTimerCreate("power_led_timer", pdMS_TO_TICKS(1000), pdTRUE, nullptr,
                     [](TimerHandle_t xTimer) {
                         g_power_led_state = !g_power_led_state;
                         coral::micro::led::Set(coral::micro::led::LED::kPower,
                                                g_power_led_state);
                     });
    xTimerStart(power_led_timer, 0);

#if defined(OOBE_DEMO)
    TimerHandle_t m4_timer =
        xTimerCreate("m4_timer", pdMS_TO_TICKS(10000), pdFALSE, (void*)0,
                     [](TimerHandle_t xTimer) {
                         g_person_detected = true;
                         GPIO_PinWrite(GPIO13, 6, true);
                         xTimerReset(xTimer, 0);
                     });
#endif  // defined(OOBE_DEMO)

    while (true) {
        printf("M4 main loop\r\n");
        coral::micro::CameraTask::GetSingleton()->Enable(
            coral::micro::camera::Mode::STREAMING);
        gpio_pin_config_t user_led = {kGPIO_DigitalOutput, 0, kGPIO_NoIntmode};
        GPIO_PinInit(GPIO13, 6, &user_led);
#if defined(OOBE_DEMO)
        g_person_detected = false;
        GPIO_PinWrite(GPIO13, 6, false);
        xTimerStart(m4_timer, 0);
#endif  // defined(OOBE_DEMO)

        while (true) {
            loop();
#if !defined(OOBE_SIMPLE)
            if (g_person_detected) {
                break;
            }
#endif
        }
        printf("Person detected, let M7 take over.\r\n");
        coral::micro::CameraTask::GetSingleton()->Disable();
        coral::micro::ipc::Message msg;
        msg.type = coral::micro::ipc::MessageType::APP;
        coral::micro::IPCM4::GetSingleton()->SendMessage(msg);
        vTaskSuspend(nullptr);
    }
}
