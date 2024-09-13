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

// Callback used for simple variables that return a string
using GetVarCb =
    std::function<bool(Device*, const std::vector<std::string>&, std::string*)>;
// Callback to retrieve all possible argument combinations, for getvar all.
using GetVarAllCb =
    std::function<std::vector<std::vector<std::string>>(Device*)>;
// Callback used for special variables that contain multiline strings. These
// should be excluded from getvar all (example: log output)
using GetSpecialVarCb = std::function<bool(Device* device)>;

struct SimpleVariable {
  SimpleVariable() = default;
  SimpleVariable(GetVarCb get_handler, GetVarAllCb get_all_handler)
      : get(std::move(get_handler)), get_all_args(std::move(get_all_handler)) {}

  GetVarCb get;
  GetVarAllCb get_all_args;
};

struct SpecialVariable {
  SpecialVariable() = default;
  SpecialVariable(GetSpecialVarCb get_handler) : get(std::move(get_handler)) {}

  // Callback to retrieve the values
  GetSpecialVarCb get;
};

class VariableProvider {
 public:
  VariableProvider();
  bool RegisterVariable(std::string name,
                        GetVarCb get,
                        GetVarAllCb get_all_args = nullptr);
  bool RegisterSpecialVariable(std::string name, GetSpecialVarCb);

  std::unordered_map<std::string, SimpleVariable> const& variables() const {
    return simple_vars_;
  }

  std::unordered_map<std::string, SpecialVariable> const& special_variables()
      const {
    return special_vars_;
  }

 private:
  std::unordered_map<std::string, SimpleVariable> simple_vars_;
  std::unordered_map<std::string, SpecialVariable> special_vars_;
};

}  // namespace pw::fastboot