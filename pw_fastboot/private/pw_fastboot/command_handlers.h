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

#include <string>
#include <vector>

namespace pw::fastboot {

class Device;

bool DownloadHandler(Device* device, const std::vector<std::string>& args);
bool ShutDownHandler(Device* device, const std::vector<std::string>& args);
bool RebootHandler(Device* device, const std::vector<std::string>& args);
bool RebootBootloaderHandler(Device* device,
                             const std::vector<std::string>& args);
bool RebootFastbootHandler(Device* device,
                           const std::vector<std::string>& args);
bool RebootRecoveryHandler(Device* device,
                           const std::vector<std::string>& args);
bool GetVarHandler(Device* device, const std::vector<std::string>& args);
bool FlashHandler(Device* device, const std::vector<std::string>& args);
bool OemCmdHandler(Device* device, const std::vector<std::string>& args);

}  // namespace pw::fastboot
