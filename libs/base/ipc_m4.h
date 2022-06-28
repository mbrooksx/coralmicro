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

#ifndef LIBS_BASE_IPC_M4_H_
#define LIBS_BASE_IPC_M4_H_

#include <functional>

#include "libs/base/ipc.h"

namespace coral::micro {
class IPCM4 : public IPC {
   public:
    static IPCM4* GetSingleton() {
        static IPCM4 ipc;
        return &ipc;
    }

    void Init() override;

   protected:
    void RxTaskFn() override;

   private:
    void HandleSystemMessage(const ipc::SystemMessage& message) override;
};
}  // namespace coral::micro

#endif  // LIBS_BASE_IPC_M4_H_
