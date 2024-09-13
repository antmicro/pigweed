#pragma once

#include <charconv>
#include <cstddef>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "cpp-string/string_printf.h"
#include "pw_assert/assert.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace stringutils {

// pw_bluetooth_sapphire has the AOSP-style std::string printf utilities
// in an internal library along with tests; use it instead of rolling our
// own variant.
constexpr auto StringPrintf = bt_lib_cpp_string::StringPrintf;

inline bool StartsWith(std::string_view str, std::string_view prefix) {
#if defined(__cpp_lib_starts_ends_with) && __cpp_lib_starts_ends_with >= 201711L
  return str.starts_with(prefix);
#else
  return str.substr(0, prefix.length()) == prefix;
#endif
}

template <typename T>
inline pw::Status HexStringToInt(std::string_view string,
                                 T& value,
                                 size_t max = std::numeric_limits<T>::max()) {
  auto [_, ec] =
      std::from_chars(string.data(), string.data() + string.size(), value, 16);
  if (ec == std::errc::result_out_of_range) {
    return pw::Status::OutOfRange();
  } else if (ec != std::errc()) {
    return pw::Status::InvalidArgument();
  }
  if (value > max) {
    return pw::Status::OutOfRange();
  }
  return pw::OkStatus();
}

template <typename T>
inline std::string Join(pw::span<T> it, std::string_view sep) {
  return std::accumulate(
      it.begin(), it.end(), std::string{}, [sep](std::string a, T b) {
        return std::move(a) + std::string{sep} + b;
      });
}

inline std::vector<std::string> Split(std::string_view s,
                                      std::string_view delimiters) {
  PW_ASSERT(delimiters.size() != 0U);
  std::vector<std::string> result;
  size_t base = 0;
  size_t found;
  while (true) {
    found = s.find_first_of(delimiters, base);
    result.push_back(std::string{s.substr(base, found - base)});
    if (found == s.npos)
      break;
    base = found + 1;
  }
  return result;
}

}  // namespace stringutils
