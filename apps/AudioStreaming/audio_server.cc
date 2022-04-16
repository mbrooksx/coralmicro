#include <errno.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "apps/AudioStreaming/network.h"
#include "libs/tasks/AudioTask/audio_reader.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/stream_buffer.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/nxp/rt1176-sdk/middleware/lwip/src/include/lwip/sockets.h"

namespace valiant {
namespace {
constexpr int kPort = 33000;
constexpr int kNumSampleFormats = 2;
constexpr const char* kSampleFormatNames[] = {"S16_LE", "S32_LE"};
enum SampleFormat {
    kS16LE = 0,
    kS32LE = 1,
};

int DropSamples(AudioReader* reader, int min_count) {
    int count = 0;
    while (count < min_count) count += reader->FillBuffer();
    return count;
}

void ProcessClient(int client_socket) {
    int32_t params[5];
    if (ReadArray(client_socket, params, std::size(params)) != IOStatus::kOk) {
        printf("ERROR: Cannot read params from client socket\r\n");
        return;
    }

    const int sample_rate_hz = params[0];
    const int sample_format = params[1];
    const int dma_buffer_size_ms = params[2];
    const int num_dma_buffers = params[3];
    const int drop_first_samples_ms = params[4];

    auto sample_rate = CheckSampleRate(sample_rate_hz);
    if (!sample_rate.has_value()) {
        printf("ERROR: Invalid sample rate (Hz): %d\r\n", sample_rate_hz);
        return;
    }

    if (sample_format < 0 || sample_format >= kNumSampleFormats) {
        printf("ERROR: Invalid sample format: %d\r\n", sample_format);
        return;
    }

    if (dma_buffer_size_ms <= 0) {
        printf("ERROR: Invalid DMA buffer size (ms): %d\r\n",
               dma_buffer_size_ms);
        return;
    }

    if (num_dma_buffers <= 0) {
        printf("ERROR: Invalid number of DMA buffers: %d\r\n", num_dma_buffers);
        return;
    }

    printf("Format:\r\n");
    printf("  Sample rate (Hz): %d\r\n", sample_rate_hz);
    printf("  Sample format: %s\r\n", kSampleFormatNames[sample_format]);
    printf("  DMA buffer size (ms): %d\r\n", dma_buffer_size_ms);
    printf("  DMA buffer count: %d\r\n", num_dma_buffers);
    printf("Sending audio samples...\r\n");

    AudioReader reader(*sample_rate, dma_buffer_size_ms, num_dma_buffers);

    const auto& buffer32 = reader.Buffer();

    const int num_dropped_samples = DropSamples(
        &reader, audio::MsToSamples(*sample_rate, drop_first_samples_ms));

    int total_bytes = 0;
    if (sample_format == kS32LE) {
        while (true) {
            auto size = reader.FillBuffer();
            if (WriteArray(client_socket, buffer32.data(), size) !=
                IOStatus::kOk)
                break;
            total_bytes += size * sizeof(int32_t);
        }
    } else {
        std::vector<int16_t> buffer16(buffer32.size());
        while (true) {
            auto size = reader.FillBuffer();
            for (size_t i = 0; i < size; ++i) buffer16[i] = buffer32[i] >> 16;
            if (WriteArray(client_socket, buffer16.data(), size) !=
                IOStatus::kOk)
                break;
            total_bytes += size * sizeof(int16_t);
        }
    }

    printf("Bytes sent: %d\r\n", total_bytes);
    printf("Ring buffer overflows: %d\r\n", reader.OverflowCount());
    printf("Ring buffer underflows: %d\r\n", reader.UnderflowCount());
    printf("Dropped first samples: %d\r\n", num_dropped_samples);
    printf("Done.\r\n\r\n");
}

void RunServer() {
    const int server_socket = SocketServer(kPort, 5);
    if (server_socket == -1) {
        printf("ERROR: Cannot start server.\r\n");
        return;
    }

    while (true) {
        printf("INFO: Waiting for the client...\r\n");
        const int client_socket = ::accept(server_socket, nullptr, nullptr);
        if (client_socket == -1) {
            printf("ERROR: Cannot connect client.\r\n");
            continue;
        }

        printf("INFO: Client #%d connected.\r\n", client_socket);
        ProcessClient(client_socket);
        ::closesocket(client_socket);
        printf("INFO: Client #%d disconnected.\r\n", client_socket);
    }
}
}  // namespace
}  // namespace valiant

extern "C" void app_main(void* param) {
    valiant::RunServer();
    vTaskSuspend(nullptr);
}
