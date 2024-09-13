/*
 * Copyright (C) 2018 The Android Open Source Project
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
#include <vector>

namespace pw::fastboot {

class Device;

enum class FastbootResult {
  OKAY,
  FAIL,
  INFO,
  DATA,
};

constexpr unsigned int kMaxDownloadSizeDefault = 0x10000000;
constexpr unsigned int kMaxFetchSizeDefault = 0x10000000;

// Execute a command with the given arguments (possibly empty).
using CommandHandler =
    std::function<bool(Device*, const std::vector<std::string>&)>;

}  // namespace pw::fastboot