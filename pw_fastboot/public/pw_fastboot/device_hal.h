/*
 * Copyright (C) 2024 Antmicro
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace pw::fastboot {

class Device;

enum class RebootType { ToSoftware, ToRecovery, ToBootloader, ToFastboot };

class DeviceHAL {
 public:
  virtual ~DeviceHAL() = default;

  virtual bool Flash(Device*, std::string /*partition*/) {
    return false;
  }
  virtual bool Reboot(Device*, RebootType) { return false; }
  virtual bool ShutDown(Device*) { return false; }
  virtual bool OemCommand(Device*, std::string /*command*/) {
    return false;
  }
  virtual bool IsDeviceLocked(Device*) { return false; }

 private:
};

}  // namespace pw::fastboot