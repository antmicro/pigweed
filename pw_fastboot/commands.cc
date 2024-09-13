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

CommandResult GetVarHandler(Device* device,
                            const std::vector<std::string>& args) {
  if (args.size() < 2) {
    return CommandResult::Failed("Missing argument");
  }

  auto* provider = device->get_variables();

  // Handle special variables first (e.g, "all")
  auto found_special = provider->special_variables().find(args[1]);
  if (found_special != provider->special_variables().end()) {
    if (!found_special->second.get(device)) {
      return CommandResult::Failed("Variable fetch failed");
    }
    return CommandResult::Okay();
  }

  // args[0] is command name, args[1] is variable.
  auto found_variable = provider->variables().find(args[1]);
  if (found_variable == provider->variables().end()) {
    return CommandResult::Failed("Unknown variable");
  }

  std::string message;
  std::vector<std::string> getvar_args(args.begin() + 2, args.end());
  if (!found_variable->second.get(device, getvar_args, &message)) {
    return CommandResult::Failed(message);
  }
  return CommandResult::Okay(message);
}

CommandResult OemCmdHandler(Device* device,
                            const std::vector<std::string>& args) {
  return device->device_hal()->OemCommand(device, args[0]);
}

CommandResult DownloadHandler(Device* device,
                              const std::vector<std::string>& args) {
  if (args.size() < 2) {
    return CommandResult::Failed("size argument unspecified");
  }

  if (device->device_hal()->IsDeviceLocked(device)) {
    return CommandResult::Failed("Download is not allowed on locked devices");
  }

  // arg[0] is the command name, arg[1] contains size of data to be downloaded
  // which should always be 8 bytes
  if (args[1].length() != 8) {
    return CommandResult::Failed("Invalid size (length of size != 8)");
  }
  unsigned int size;
  if (!stringutils::HexStringToInt(args[1], size, kMaxDownloadSizeDefault)
           .ok()) {
    return CommandResult::Failed("Invalid size");
  }
  if (size == 0) {
    return CommandResult::Failed("Invalid size (0)");
  }
  if (size > device->download_data().max_size()) {
    return CommandResult::Failed(stringutils::StringPrintf(
        "Download size (%d) exceeds configured maximum download size (%d)",
        size,
        device->download_data().max_size()));
  }
  device->download_data().resize(size);
  if (!device->WriteStatus(FastbootResult::DATA,
                           stringutils::StringPrintf("%08x", size))) {
    return CommandResult::Failed("Failed to send DATA message");
  }

  if (device->HandleData(true, device->download_data())) {
    return CommandResult::Okay();
  }

  PW_LOG_ERROR("Couldn't download data");
  return CommandResult::Failed("Couldn't download data");
}

CommandResult ShutDownHandler(Device* device,
                              const std::vector<std::string>& /* args */) {
  device->WriteInfo("Shutting down");
  if (!device->device_hal()->ShutDown(device)) {
    return CommandResult::Failed("Shutdown failed");
  }
  device->CloseDevice();
  return CommandResult::Okay();
}

CommandResult RebootHandler(Device* device,
                            const std::vector<std::string>& /* args */) {
  device->WriteInfo("Rebooting");
  if (!device->device_hal()->Reboot(device, RebootType::ToSoftware)) {
    return CommandResult::Failed("Reboot failed");
  }
  device->CloseDevice();
  return CommandResult::Okay();
}

CommandResult RebootBootloaderHandler(
    Device* device, const std::vector<std::string>& /* args */) {
  device->WriteInfo("Rebooting to bootloader");
  if (!device->device_hal()->Reboot(device, RebootType::ToBootloader)) {
    return CommandResult::Failed("Reboot failed");
  }
  device->CloseDevice();
  return CommandResult::Okay();
}

CommandResult RebootFastbootHandler(
    Device* device, const std::vector<std::string>& /* args */) {
  device->WriteInfo("Rebooting to fastboot");
  if (!device->device_hal()->Reboot(device, RebootType::ToFastboot)) {
    return CommandResult::Failed("Reboot failed");
  }
  device->CloseDevice();
  return CommandResult::Okay();
}

CommandResult RebootRecoveryHandler(
    Device* device, const std::vector<std::string>& /* args */) {
  device->WriteInfo("Rebooting to recovery");
  if (!device->device_hal()->Reboot(device, RebootType::ToRecovery)) {
    return CommandResult::Failed("Reboot failed");
  }
  device->CloseDevice();
  return CommandResult::Okay();
}

CommandResult FlashHandler(Device* device,
                           const std::vector<std::string>& args) {
  if (args.size() < 2) {
    return CommandResult::Failed("Invalid arguments");
  }

  if (device->device_hal()->IsDeviceLocked(device)) {
    return CommandResult::Failed("Flashing is not allowed on locked devices");
  }

  const auto& partition_name = args[1];
  return device->device_hal()->Flash(device, partition_name);
}

}  // namespace pw::fastboot