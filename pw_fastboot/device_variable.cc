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
#include "pw_fastboot/device_variable.h"

#include <utility>

#include "pw_fastboot/constants.h"
#include "pw_fastboot/fastboot_device.h"
#include "stringutils/strings.h"

namespace pw::fastboot {

static bool GetVersion(Device* /* device */,
                       const std::vector<std::string>& /* args */,
                       std::string* message) {
  *message = FB_PROTOCOL_VERSION;
  return true;
}

static void GetAllVars(Device* device,
                       const std::string& name,
                       const SimpleVariable& handlers) {
  if (!handlers.get_all_args) {
    std::string message;
    if (!handlers.get(device, std::vector<std::string>(), &message)) {
      return;
    }
    device->WriteInfo(
        stringutils::StringPrintf("%s:%s", name.c_str(), message.c_str()));
    return;
  }

  auto all_args = handlers.get_all_args(device);
  for (const auto& args : all_args) {
    std::string message;
    if (!handlers.get(device, args, &message)) {
      continue;
    }
    std::string arg_string = stringutils::Join(args, ":");
    device->WriteInfo(stringutils::StringPrintf(
        "%s:%s:%s", name.c_str(), arg_string.c_str(), message.c_str()));
  }
}

static bool GetVarAll(Device* device) {
  for (const auto& [name, handlers] : device->get_variables()->variables()) {
    GetAllVars(device, name, handlers);
  }
  return true;
}

VariableProvider::VariableProvider() {
  // Register some basic fastboot variables (protocol version
  // and the "all" meta-variable)
  RegisterVariable(FB_VAR_VERSION, GetVersion, nullptr);
  RegisterSpecialVariable(FB_VAR_ALL, GetVarAll);
}

bool VariableProvider::RegisterVariable(std::string name,
                                              GetVarCb get,
                                              GetVarAllCb get_all_args) {
  auto it = simple_vars_.emplace(
      std::make_pair(name, SimpleVariable{get, get_all_args}));
  return it.second;
}

bool VariableProvider::RegisterSpecialVariable(std::string name,
                                                     GetSpecialVarCb get) {
  auto it = special_vars_.emplace(std::make_pair(name, SpecialVariable{get}));
  return it.second;
}

}  // namespace pw::fastboot