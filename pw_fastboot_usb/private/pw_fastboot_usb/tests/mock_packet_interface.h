// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once
#include "gmock/gmock.h"
#include "pw_fastboot_usb/packet.h"

namespace pw::fastboot {

class MockUsbPacketInterface : public pw::fastboot::UsbPacketInterface {
 public:
  MockUsbPacketInterface() {
    ON_CALL(*this, Init)
        .WillByDefault(
            [](pw::fastboot::UsbPacketInterface::InitStatusUpdateCb cb) {
              cb(true);
            });
  }

  MOCK_METHOD(void, Init, (InitStatusUpdateCb), (override));
  MOCK_METHOD(void, Deinit, (), (override));
  MOCK_METHOD(std::ptrdiff_t, QueuePacket, (pw::ConstByteSpan), (override));

 private:
};

}  // namespace pw::fastboot