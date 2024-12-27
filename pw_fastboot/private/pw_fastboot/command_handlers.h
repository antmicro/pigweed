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

#include <string>
#include <vector>

#include "pw_fastboot/commands.h"

namespace pw::fastboot {

class Device;

CommandResult DownloadHandler(Device* device,
                              const std::vector<std::string>& args);
CommandResult ShutDownHandler(Device* device,
                              const std::vector<std::string>& args);
CommandResult RebootHandler(Device* device,
                            const std::vector<std::string>& args);
CommandResult RebootBootloaderHandler(Device* device,
                                      const std::vector<std::string>& args);
CommandResult RebootFastbootHandler(Device* device,
                                    const std::vector<std::string>& args);
CommandResult RebootRecoveryHandler(Device* device,
                                    const std::vector<std::string>& args);
CommandResult GetVarHandler(Device* device,
                            const std::vector<std::string>& args);
CommandResult FlashHandler(Device* device,
                           const std::vector<std::string>& args);
CommandResult OemCmdHandler(Device* device,
                            const std::vector<std::string>& args);

}  // namespace pw::fastboot
