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

#include "pw_fastboot/commands.h"

#include <unordered_set>

#include "private/pw_fastboot/command_handlers.h"
#include "pw_fastboot/constants.h"
#include "pw_fastboot/device_hal.h"
#include "pw_fastboot/device_variable.h"
#include "pw_fastboot/fastboot_device.h"
#include "pw_log/log.h"
#include "stringutils/strings.h"

namespace pw::fastboot {

bool GetVarHandler(Device* device, const std::vector<std::string>& args) {
  if (args.size() < 2) {
    return device->WriteFail("Missing argument");
  }

  auto* provider = device->get_variables();

  // Handle special variables first (e.g, "all")
  auto found_special = provider->special_variables().find(args[1]);
  if (found_special != provider->special_variables().end()) {
    if (!found_special->second.get(device)) {
      return false;
    }
    return device->WriteOkay("");
  }

  // args[0] is command name, args[1] is variable.
  auto found_variable = provider->variables().find(args[1]);
  if (found_variable == provider->variables().end()) {
    return device->WriteFail("Unknown variable");
  }

  std::string message;
  std::vector<std::string> getvar_args(args.begin() + 2, args.end());
  if (!found_variable->second.get(device, getvar_args, &message)) {
    return device->WriteFail(message);
  }
  return device->WriteOkay(message);
}

bool OemCmdHandler(Device* device, const std::vector<std::string>& args) {
  if (!device->device_hal()->OemCommand(device, args[0])) {
    return device->WriteFail("Unable to do OEM command " + args[0]);
  }
  return device->WriteOkay("");
}

bool DownloadHandler(Device* device, const std::vector<std::string>& args) {
  if (args.size() < 2) {
    return device->WriteFail("size argument unspecified");
  }

  if (device->device_hal()->IsDeviceLocked(device)) {
    return device->WriteFail("Download is not allowed on locked devices");
  }

  // arg[0] is the command name, arg[1] contains size of data to be downloaded
  // which should always be 8 bytes
  if (args[1].length() != 8) {
    return device->WriteFail("Invalid size (length of size != 8)");
  }
  unsigned int size;
  std::string hexstring = std::string{"0x"} + args[1];
  if (!stringutils::ParseUint(
          hexstring.c_str(), &size, kMaxDownloadSizeDefault)) {
    return device->WriteFail("Invalid size");
  }
  if (size == 0) {
    return device->WriteFail("Invalid size (0)");
  }
  device->download_data().resize(size);
  if (!device->WriteStatus(FastbootResult::DATA,
                           stringutils::StringPrintf("%08x", size))) {
    return false;
  }

  if (device->HandleData(true, &device->download_data())) {
    return device->WriteOkay("");
  }

  PW_LOG_ERROR("Couldn't download data");
  return device->WriteFail("Couldn't download data");
}

bool ShutDownHandler(Device* device,
                     const std::vector<std::string>& /* args */) {
  auto result = device->WriteInfo("Shutting down");
  if (!device->device_hal()->ShutDown(device)) {
    return device->WriteFail("Shutdown failed");
  }
  device->CloseDevice();
  return result;
}

bool RebootHandler(Device* device, const std::vector<std::string>& /* args */) {
  auto result = device->WriteInfo("Rebooting");
  if (!device->device_hal()->Reboot(device, RebootType::ToSoftware)) {
    return device->WriteFail("Reboot failed");
  }
  device->CloseDevice();
  return result;
}

bool RebootBootloaderHandler(Device* device,
                             const std::vector<std::string>& /* args */) {
  auto result = device->WriteInfo("Rebooting to bootloader");
  if (!device->device_hal()->Reboot(device, RebootType::ToBootloader)) {
    return device->WriteFail("Reboot failed");
  }
  device->CloseDevice();
  return result;
}

bool RebootFastbootHandler(Device* device,
                           const std::vector<std::string>& /* args */) {
  auto result = device->WriteInfo("Rebooting to fastboot");
  if (!device->device_hal()->Reboot(device, RebootType::ToFastboot)) {
    return device->WriteFail("Reboot failed");
  }
  device->CloseDevice();
  return result;
}

bool RebootRecoveryHandler(Device* device,
                           const std::vector<std::string>& /* args */) {
  auto result = device->WriteInfo("Rebooting to recovery");
  if (!device->device_hal()->Reboot(device, RebootType::ToRecovery)) {
    return device->WriteFail("Reboot failed");
  }
  device->CloseDevice();
  return result;
}

bool FlashHandler(Device* device, const std::vector<std::string>& args) {
  if (args.size() < 2) {
    return device->WriteFail("Invalid arguments");
  }

  if (device->device_hal()->IsDeviceLocked(device)) {
    return device->WriteFail("Flashing is not allowed on locked devices");
  }

  const auto& partition_name = args[1];
  if (!device->device_hal()->Flash(device, partition_name)) {
    return device->WriteFail("Flashing failed");
  }
  return device->WriteOkay("Flashing done");
}

}  // namespace pw::fastboot