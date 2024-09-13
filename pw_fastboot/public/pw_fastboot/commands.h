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

/// Represents a fastboot command result.
///
/// Each fastboot command must have exactly one associated response message,
/// which is represented using a CommandResult. This class can only represent
/// an OKAY or FAIL status, as:
///
///   - A DATA status does NOT end a command and requires further
///   handling.
///
///   - An INFO status may be sent many times in response to a command before
///   its execution is finished.
///
/// See "Transport and Framing" section in the fastboot README:
///   https://android.googlesource.com/platform/system/core/+/refs/heads/main/fastboot
///
/// NOTE: This class currently can't be entirely constexpr due to the usage of
/// std::string. If error message generation at runtime is not required, this
/// can be changed to take a std::string_view instead.
class CommandResult {
 public:
  // Create an OKAY command result
  // The provided message will be displayed to the user (may be omitted).
  static CommandResult Okay(std::string message = {}) {
    return CommandResult{FastbootResult::OKAY, std::move(message)};
  }

  // Create a FAIL command result
  // The provided message will be displayed to the user.
  static CommandResult Failed(std::string message) {
    return CommandResult{FastbootResult::FAIL, std::move(message)};
  }

  constexpr FastbootResult const& result() const { return result_; }
  std::string const& message() const { return message_; }

  constexpr explicit operator bool() const {
    return result_ == FastbootResult::OKAY;
  }

 private:
  CommandResult() : result_(FastbootResult::OKAY), message_() {}

  CommandResult(FastbootResult result, std::string message)
      : result_(std::move(result)), message_(std::move(message)) {}

  FastbootResult result_;
  std::string message_;
};

constexpr unsigned int kMaxDownloadSizeDefault = 0x10000000;
constexpr unsigned int kMaxFetchSizeDefault = 0x10000000;

// Execute a command with the given arguments (possibly empty).
using CommandHandler =
    std::function<CommandResult(Device*, const std::vector<std::string>&)>;

}  // namespace pw::fastboot