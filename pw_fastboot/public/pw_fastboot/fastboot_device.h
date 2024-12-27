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
#include <string>
#include <unordered_map>

#include "commands.h"
#include "config.h"
#include "device_hal.h"
#include "device_variable.h"
#include "pw_containers/vector.h"
#include "transport.h"

namespace pw::fastboot {

class Device {
 public:
  Device(std::unique_ptr<Transport> transport,
         std::unique_ptr<VariableProvider> variables,
         std::unique_ptr<DeviceHAL> device_hal);
  ~Device();

  void CloseDevice();
  void ExecuteCommands();
  bool WriteStatus(FastbootResult result, const std::string& message);
  bool HandleData(bool read, pw::Vector<char>& data);
  bool HandleData(bool read, char* data, size_t size);

  bool WriteInfo(const std::string& message);

  pw::Vector<char>& download_data() { return *download_data_; }
  Transport* get_transport() { return transport_.get(); }
  VariableProvider* get_variables() { return variables_.get(); }
  DeviceHAL* device_hal() { return device_hal_.get(); }

 private:
  const std::unordered_map<std::string, CommandHandler> kCommandMap;

  std::unique_ptr<Transport> transport_;
  std::unique_ptr<VariableProvider> variables_;
  std::unique_ptr<DeviceHAL> device_hal_;
  std::unique_ptr<pw::Vector<char, PW_FASTBOOT_MAX_DOWNLOAD_SIZE>>
      download_data_;
  static_assert(
      PW_FASTBOOT_MAX_DOWNLOAD_SIZE <=
          std::numeric_limits<typename pw::Vector<char>::size_type>::max(),
      "Maximum download size must fit within a pw::Vector<char>");
};

}  // namespace pw::fastboot