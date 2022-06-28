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

#ifndef _LIBS_TASKS_EDGETPUTASK_EDGETPU_TASK_H_
#define _LIBS_TASKS_EDGETPUTASK_EDGETPU_TASK_H_

#include "libs/base/queue_task.h"
#include "libs/base/tasks.h"
#include "libs/nxp/rt1176-sdk/usb_host_config.h"
#include "third_party/nxp/rt1176-sdk/devices/MIMXRT1176/drivers/fsl_gpio.h"
#include "third_party/nxp/rt1176-sdk/middleware/usb/host/usb_host.h"
#include "third_party/nxp/rt1176-sdk/middleware/usb/host/usb_host_hci.h"
#include "third_party/nxp/rt1176-sdk/middleware/usb/include/usb.h"
#include <functional>

namespace coral::micro {

inline constexpr int kEdgeTpuVid = 0x18d1;
inline constexpr int kEdgeTpuPid = 0x9302;

namespace edgetpu {

enum edgetpu_state {
    EDGETPU_STATE_UNATTACHED = 0,
    EDGETPU_STATE_ATTACHED,
    EDGETPU_STATE_SET_INTERFACE,
    EDGETPU_STATE_GET_STATUS,
    EDGETPU_STATE_CONNECTED,
    EDGETPU_STATE_ERROR,
};

enum class RequestType : uint8_t {
    NEXT_STATE,
    SET_POWER,
    GET_POWER,
};

struct NextStateRequest {
    edgetpu_state state;
};

struct SetPowerRequest {
    bool enable;
};

struct GetPowerResponse {
    bool enabled;
};

struct Response {
    RequestType type;
    union {
        GetPowerResponse get_power;
    } response;
};

struct Request {
    RequestType type;
    union {
        NextStateRequest next_state;
        SetPowerRequest set_power;
    } request;
    std::function<void(Response)> callback;
};

}  // namespace edgetpu

inline constexpr char kEdgeTpuTaskName[] = "edgetpu_task";

class EdgeTpuTask : public QueueTask<edgetpu::Request, edgetpu::Response,
                                    kEdgeTpuTaskName,
                                    configMINIMAL_STACK_SIZE * 3,
                                    EDGETPU_TASK_PRIORITY, /*QueueLength=*/4> {
  public:
    bool GetPower();
    void SetPower(bool enable);
    static EdgeTpuTask* GetSingleton() {
        static EdgeTpuTask task;
        return &task;
    }

    void SetDeviceHandle(usb_device_handle handle) {
      device_handle_ = handle;
    }
    usb_device_handle device_handle() {
      return device_handle_;
    }

    void SetInterfaceHandle(usb_host_interface_handle handle) {
      interface_handle_ = handle;
    }
    usb_host_interface_handle interface_handle() {
      return interface_handle_;
    }

    void SetClassHandle(usb_host_class_handle handle) {
      class_handle_ = handle;
    }
    usb_host_class_handle class_handle() {
      return class_handle_;
    }
  private:
    void TaskInit() override;
    void RequestHandler(edgetpu::Request *req) override;
    void HandleNextState(edgetpu::NextStateRequest& req);
    bool HandleGetPowerRequest();
    void HandleSetPowerRequest(edgetpu::SetPowerRequest& req);
    void SetNextState(enum edgetpu::edgetpu_state next_state);
    static void SetInterfaceCallback(void *param, uint8_t *data, uint32_t data_length, usb_status_t status);
    static void GetStatusCallback(void *param, uint8_t *data, uint32_t data_length, usb_status_t status);
    usb_status_t USBHostEvent(
            usb_host_handle host_handle,
            usb_device_handle device_handle,
            usb_host_configuration_handle config_handle,
            uint32_t event_code);

    usb_device_handle device_handle_;
    usb_host_interface_handle interface_handle_;
    usb_host_class_handle class_handle_;
    uint8_t status_;
    int enabled_count_ = 0;
};
}  // namespace coral::micro

#endif  // _LIBS_TASKS_EDGETPUTASK_EDGETPU_TASK_H_
