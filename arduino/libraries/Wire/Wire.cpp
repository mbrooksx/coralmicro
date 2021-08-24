#include "Wire.h"
#include "libs/base/tasks.h"

#include <cassert>
extern "C" uint32_t LPI2C_GetInstance(LPI2C_Type*);
static IRQn_Type const kLpi2cIrqs[] = LPI2C_IRQS;

namespace valiant {
namespace arduino {

void HardwareI2C::StaticSlaveCallback(LPI2C_Type *base, lpi2c_slave_transfer_t *transfer, void *userData) {
    assert(userData);
    reinterpret_cast<HardwareI2C*>(userData)->SlaveCallback(base, transfer);
}

void HardwareI2C::SlaveCallback(LPI2C_Type *base, lpi2c_slave_transfer_t *transfer) {
    assert(base == base_);
    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent:
            break;
        case kLPI2C_SlaveTransmitEvent:
            if (!request_cb_) {
                transfer->data = nullptr;
                transfer->dataSize = 0;
                break;
            }
            request_cb_();
            transfer->data = tx_buffer_;
            transfer->dataSize = tx_buffer_used_;
            tx_buffer_used_ = 0;
            break;
        case kLPI2C_SlaveReceiveEvent:
            transfer->data = isr_rx_buffer_;
            transfer->dataSize = kBufferSize;
            break;
        case kLPI2C_SlaveCompletionEvent: {
            for (int i = 0; i < transfer->transferredCount; ++i) {
                rx_buffer_.store_char(isr_rx_buffer_[i]);
            }
            BaseType_t reschedule;
            vTaskNotifyGiveFromISR(receive_task_handle_, &reschedule);
            portYIELD_FROM_ISR(reschedule);
            break;
        }
        default:
            assert(false);
            break;
    }
}

HardwareI2C::HardwareI2C(LPI2C_Type* base) {
    base_ = base;
}

// Begin for master devices
void HardwareI2C::begin() {
    lpi2c_master_config_t config;
    uint32_t instance = LPI2C_GetInstance(base_);
    NVIC_SetPriority(kLpi2cIrqs[instance], kInterruptPriority);
    LPI2C_MasterGetDefaultConfig(&config);
    status_t status = LPI2C_RTOS_Init(&handle_, base_, &config, CLOCK_GetFreq(kCLOCK_OscRc48MDiv2));
}

void HardwareI2C::StaticOnReceiveHandler(void *param) {
    reinterpret_cast<HardwareI2C*>(param)->OnReceiveHandler();
}

void HardwareI2C::OnReceiveHandler() {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (receive_cb_) {
            receive_cb_(rx_buffer_.available());
        }
    }
    vTaskSuspend(NULL);
}

// Begin for slave devices
void HardwareI2C::begin(uint8_t address) {
    lpi2c_slave_config_t config;
    uint32_t instance = LPI2C_GetInstance(base_);

    if (!receive_task_handle_) {
        xTaskCreate(HardwareI2C::StaticOnReceiveHandler, "HardwareI2COnReceiveHandler", configMINIMAL_STACK_SIZE, this, APP_TASK_PRIORITY, &receive_task_handle_);
    }

    NVIC_SetPriority(kLpi2cIrqs[instance], 3);
    LPI2C_SlaveGetDefaultConfig(&config);
    config.address0 = address;
    LPI2C_SlaveInit(base_, &config, CLOCK_GetFreq(kCLOCK_OscRc48MDiv2));

    LPI2C_SlaveTransferCreateHandle(base_, &slave_handle_, HardwareI2C::StaticSlaveCallback, this);
    status_t status = LPI2C_SlaveTransferNonBlocking(base_, &slave_handle_, kLPI2C_SlaveCompletionEvent | kLPI2C_SlaveAddressMatchEvent);
}

void HardwareI2C::end() {
    // clean up if we created anything in begin
    if (receive_task_handle_) {
        vTaskDelete(receive_task_handle_);
    }
    LPI2C_SlaveTransferAbort(base_, &slave_handle_);
}

void HardwareI2C::setClock(uint32_t freq) {
    LPI2C_MasterSetBaudRate(base_, CLOCK_GetFreq(kCLOCK_OscRc48MDiv2), freq);
}

void HardwareI2C::beginTransmission(uint8_t address) {
    tx_address_ = address;
    tx_buffer_used_ = 0;
    memset(tx_buffer_, 0, sizeof(tx_buffer_));
}

uint8_t HardwareI2C::endTransmission(bool stopBit) {
    lpi2c_master_transfer_t transfer;
    transfer.flags = stopBit ? kLPI2C_TransferDefaultFlag : kLPI2C_TransferNoStopFlag;
    transfer.slaveAddress = tx_address_;
    transfer.direction = kLPI2C_Write;
    transfer.subaddress = 0;
    transfer.subaddressSize = 0;
    transfer.data = tx_buffer_;
    transfer.dataSize = tx_buffer_used_;
    status_t s = LPI2C_RTOS_Transfer(&handle_, &transfer);
    tx_buffer_used_ = 0;
    switch (s) {
        case kStatus_Success:
            return kSuccess;
        case kStatus_LPI2C_Nak:
            return kAddressNACK;
        default:
            return kOther;
    }
}

uint8_t HardwareI2C::endTransmission(void) {
    return endTransmission(true);
}

size_t HardwareI2C::requestFrom(uint8_t address, size_t len, bool stopBit) {
    if (len > kBufferSize) {
        return 0;
    }
    char buf[kBufferSize];
    lpi2c_master_transfer_t transfer;
    transfer.flags = stopBit ? kLPI2C_TransferDefaultFlag : kLPI2C_TransferNoStopFlag;
    transfer.slaveAddress = address;
    transfer.direction = kLPI2C_Read;
    transfer.subaddress = 0;
    transfer.subaddressSize = 0;
    transfer.data = &buf[0];
    transfer.dataSize = len;

    status_t status = LPI2C_RTOS_Transfer(&handle_, &transfer);
    if (status != kStatus_Success) {
        return 0;
    }

    for (int i = 0; i < len; ++i) {
        rx_buffer_.store_char(buf[i]);
    }
    return len;
}

size_t HardwareI2C::requestFrom(uint8_t address, size_t len) {
    return requestFrom(address, len, true);
}

void HardwareI2C::onReceive(void(cb)(int)) {
    receive_cb_ = cb;
}

void HardwareI2C::onRequest(void(cb)(void)) {
    request_cb_ = cb;
}

size_t HardwareI2C::write(uint8_t c) {
    if (tx_buffer_used_ < kBufferSize) {
        tx_buffer_[tx_buffer_used_++] = c;
        return 1;
    } else {
        return 0;
    }
}

size_t HardwareI2C::write(const char *str) {
    return write(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

size_t HardwareI2C::write(const uint8_t *buffer, size_t size) {
    int stored = 0;
    for (int i = 0; i < size; ++i) {
        stored += write(buffer[i]);
    }
    return stored;
}

int HardwareI2C::available() {
    return rx_buffer_.available();
}

int HardwareI2C::read() {
    return rx_buffer_.read_char();
}

int HardwareI2C::peek() {
    return rx_buffer_.peek();
}

}  // namespace arduino
}  // namespace valiant

valiant::arduino::HardwareI2C Wire(LPI2C1);