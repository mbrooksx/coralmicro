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

#ifndef LIBS_BASE_MUTEX_H_
#define LIBS_BASE_MUTEX_H_

#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/semphr.h"
#include "third_party/nxp/rt1176-sdk/devices/MIMXRT1176/drivers/fsl_sema4.h"
#include "third_party/nxp/rt1176-sdk/devices/MIMXRT1176/fsl_device_registers.h"

namespace coral::micro {

class MutexLock {
   public:
    explicit MutexLock(SemaphoreHandle_t sema) : sema_(sema) {
        if (__get_IPSR() != 0) {
            BaseType_t reschedule;
            xSemaphoreTakeFromISR(sema_, &reschedule);
            portYIELD_FROM_ISR(reschedule);
        } else {
            xSemaphoreTake(sema_, portMAX_DELAY);
        }
    }

    ~MutexLock() {
        if (__get_IPSR() != 0) {
            BaseType_t reschedule;
            xSemaphoreGiveFromISR(sema_, &reschedule);
            portYIELD_FROM_ISR(reschedule);
        } else {
            xSemaphoreGive(sema_);
        }
    }
    MutexLock(const MutexLock&) = delete;
    MutexLock(MutexLock&&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;
    MutexLock& operator=(MutexLock&&) = delete;

   private:
    SemaphoreHandle_t sema_;
};

class MulticoreMutexLock {
   public:
    explicit MulticoreMutexLock(uint8_t gate) : gate_(gate) {
        assert(gate < SEMA4_GATE_COUNT);
#if (__CORTEX_M == 7)
        core_ = 0;
#elif (__CORTEX_M == 4)
        core_ = 1;
#else
#error "Unknown __CORTEX_M"
#endif
        SEMA4_Lock(SEMA4, gate_, core_);
    }
    ~MulticoreMutexLock() { SEMA4_Unlock(SEMA4, gate_); }
    MulticoreMutexLock(const MutexLock&) = delete;
    MulticoreMutexLock(MutexLock&&) = delete;
    MulticoreMutexLock& operator=(const MutexLock&) = delete;
    MulticoreMutexLock& operator=(MutexLock&&) = delete;

   private:
    uint8_t gate_;
    uint8_t core_;
};

}  // namespace coral::micro

#endif  // LIBS_BASE_MUTEX_H_
