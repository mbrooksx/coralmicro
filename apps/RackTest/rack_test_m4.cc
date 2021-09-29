#include "apps/RackTest/rack_test_ipc.h"
#include "libs/base/ipc.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"

static void HandleAppMessage(const uint8_t data[valiant::ipc::kMessageBufferDataSize], void *param) {
    const RackTestAppMessage* app_message = reinterpret_cast<const RackTestAppMessage*>(data);
    switch (app_message->message_type) {
        case RackTestAppMessageType::XOR: {
            valiant::ipc::Message reply;
            reply.type = valiant::ipc::MessageType::APP;
            RackTestAppMessage* app_reply = reinterpret_cast<RackTestAppMessage*>(&reply.message.data);
            app_reply->message_type = RackTestAppMessageType::XOR;
            app_reply->message.xor_value = (app_message->message.xor_value ^ 0xFEEDDEED);
            valiant::IPC::GetSingleton()->SendMessage(reply);
            break;
        }
        default:
            printf("Unknown message type\r\n");
    }
}

extern "C" void app_main(void *param) {
    valiant::IPC::GetSingleton()->RegisterAppMessageHandler(HandleAppMessage, nullptr);
    vTaskSuspend(NULL);
}