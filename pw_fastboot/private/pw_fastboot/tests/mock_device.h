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

#include <memory>

#include "pw_fastboot/device_hal.h"
#include "pw_fastboot/device_variable.h"
#include "pw_fastboot/fastboot_device.h"
#include "pw_fastboot/tests/mock_hal.h"
#include "pw_fastboot/tests/mock_transport.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

namespace pw::fastboot {

class MockDevice : public pw::fastboot::Device {
 public:
  MockDevice()
      : pw::fastboot::Device(std::make_unique<MockTransport>(),
                             std::make_unique<pw::fastboot::VariableProvider>(),
                             std::make_unique<MockHAL>()) {
    transport_ = static_cast<MockTransport*>(this->get_transport());
    variables_ = static_cast<VariableProvider*>(this->get_variables());
    hal_ = static_cast<MockHAL*>(this->device_hal());
  }

  ~MockDevice() {
    transport_->Close();
    if (mock_thread_.joinable()) {
      mock_thread_.join();
    }
  }

  void Start() {
    mock_thread_ = pw::thread::Thread(pw::thread::stl::Options(),
                                      [this] { ExecuteCommands(); });
  }

  MockTransport* transport() const { return transport_; }
  pw::fastboot::VariableProvider* variables() const { return variables_; }
  MockHAL* hal() const { return hal_; }
  pw::fastboot::Device* device() { return this; }

 private:
  MockTransport* transport_;
  pw::fastboot::VariableProvider* variables_;
  MockHAL* hal_;
  pw::thread::Thread mock_thread_;
};

}  // namespace pw::fastboot