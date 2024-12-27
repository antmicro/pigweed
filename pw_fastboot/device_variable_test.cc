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
#include "pw_fastboot/device_variable.h"

#include <string>

#include "pw_fastboot/constants.h"
#include "pw_unit_test/framework.h"

namespace pw::fastboot {

TEST(Variables, Construction) {
  auto provider = pw::fastboot::VariableProvider{};
  (void)provider;
}

TEST(Variables, AddingSimpleVariables) {
  static std::string kVarName = "SIMPLE";
  auto provider = pw::fastboot::VariableProvider{};

  auto cb = [](auto, auto, auto) -> bool { return true; };

  ASSERT_TRUE(provider.RegisterVariable(kVarName, cb));
  const auto it = provider.variables().find(kVarName);
  ASSERT_NE(it, provider.variables().end());
}

TEST(Variables, AddingSimpleVariablesCallback) {
  static std::string kVarName = "SIMPLE";
  auto provider = pw::fastboot::VariableProvider{};

  auto callback_was_called = false;
  auto cb = [&callback_was_called](auto, auto, auto) -> bool {
    callback_was_called = true;
    return true;
  };
  ASSERT_TRUE(provider.RegisterVariable(kVarName, cb));
  const auto it = provider.variables().find(kVarName);
  ASSERT_NE(it, provider.variables().end());
  ASSERT_TRUE(static_cast<bool>((*it).second.get));
  (*it).second.get({}, {}, {});
  ASSERT_TRUE(callback_was_called);
}

TEST(Variables, AddingSpecialVariables) {
  static std::string kVarName = "SPECIAL";
  auto provider = pw::fastboot::VariableProvider{};

  auto cb = [](auto) -> bool { return true; };

  ASSERT_TRUE(provider.RegisterSpecialVariable(kVarName, cb));
  const auto it = provider.special_variables().find(kVarName);
  ASSERT_NE(it, provider.special_variables().end());
}

TEST(Variables, AddingSpecialVariablesCallback) {
  static std::string kVarName = "SPECIAL";
  auto provider = pw::fastboot::VariableProvider{};

  auto callback_was_called = false;
  auto cb = [&callback_was_called](auto) -> bool {
    callback_was_called = true;
    return true;
  };
  ASSERT_TRUE(provider.RegisterSpecialVariable(kVarName, cb));
  const auto it = provider.special_variables().find(kVarName);
  ASSERT_NE(it, provider.special_variables().end());
  ASSERT_TRUE(static_cast<bool>((*it).second.get));
  (*it).second.get({});
  ASSERT_TRUE(callback_was_called);
}

TEST(Variables, DefaultVariablePresentVersion) {
  auto provider = pw::fastboot::VariableProvider{};
  auto it = provider.variables().find(FB_VAR_VERSION);
  ASSERT_NE(it, provider.variables().end());
}

TEST(Variables, DefaultVariablePresentAll) {
  auto provider = pw::fastboot::VariableProvider{};
  auto it = provider.special_variables().find(FB_VAR_ALL);
  ASSERT_NE(it, provider.special_variables().end());
}

TEST(Variables, CannotOverrideExistingVariable) {
  auto provider = pw::fastboot::VariableProvider{};
  auto cb = [](auto, auto, auto) -> bool { return true; };
  ASSERT_FALSE(provider.RegisterVariable(FB_VAR_VERSION, cb));
}

TEST(Variables, CannotOverrideExistingSpecialVariable) {
  auto provider = pw::fastboot::VariableProvider{};
  auto cb = [](auto) -> bool { return true; };
  ASSERT_FALSE(provider.RegisterSpecialVariable(FB_VAR_ALL, cb));
}

TEST(Variables, VariableDoesNotExist) {
  static std::string kVarName = "DOESNOTEXIST";
  auto provider = pw::fastboot::VariableProvider{};
  auto it = provider.variables().find(kVarName);
  ASSERT_EQ(it, provider.variables().end());
}

TEST(Variables, SpecialVariableDoesNotExist) {
  static std::string kVarName = "DOESNOTEXIST";
  auto provider = pw::fastboot::VariableProvider{};
  auto it = provider.special_variables().find(kVarName);
  ASSERT_EQ(it, provider.special_variables().end());
}

}  // namespace pw::fastboot