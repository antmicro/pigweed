#pragma once

#include "gmock/gmock.h"
#include "pw_fastboot/device_hal.h"
#include "pw_function/function.h"

namespace pw::fastboot {

class MockHAL : public pw::fastboot::DeviceHAL {
 public:
  MOCK_METHOD(bool, Flash, (pw::fastboot::Device*, std::string), (override));
  MOCK_METHOD(bool,
              OemCommand,
              (pw::fastboot::Device*, std::string),
              (override));
  MOCK_METHOD(bool, IsDeviceLocked, (pw::fastboot::Device*), (override));
  MOCK_METHOD(bool,
              Reboot,
              (pw::fastboot::Device*, pw::fastboot::RebootType),
              (override));
  MOCK_METHOD(bool, ShutDown, (pw::fastboot::Device*), (override));
};

}  // namespace pw::fastboot