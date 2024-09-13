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