#pragma once

#include <sstream>
#include <string>
#include <vector>

/*
 * Copyright (C) 2015 The Android Open Source Project
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
namespace stringutils {

// Splits a string into a vector of strings.
//
// The string is split at each occurrence of a character in delimiters.
//
// The empty string is not a valid delimiter list.
std::vector<std::string> Split(const std::string& s,
                               const std::string& delimiters);
// Joins a container of things into a single string, using the given separator.
template <typename ContainerT, typename SeparatorT>
std::string Join(const ContainerT& things, SeparatorT separator) {
  if (things.empty()) {
    return "";
  }
  std::ostringstream result;
  result << *things.begin();
  for (auto it = std::next(things.begin()); it != things.end(); ++it) {
    result << separator << *it;
  }
  return result.str();
}
// We instantiate the common cases in strings.cpp.
extern template std::string Join(const std::vector<std::string>&, char);
extern template std::string Join(const std::vector<const char*>&, char);
extern template std::string Join(const std::vector<std::string>&,
                                 const std::string&);
extern template std::string Join(const std::vector<const char*>&,
                                 const std::string&);

// Parses the unsigned decimal integer in the string 's' and sets 'out' to
// that value. Optionally allows the caller to define a 'max' beyond which
// otherwise valid values will be rejected. Returns boolean success.
template <typename T>
bool ParseUint(const char* s, T* out, T max = std::numeric_limits<T>::max()) {
  int base = (s[0] == '0' && s[1] == 'x') ? 16 : 10;
  errno = 0;
  char* end;
  unsigned long long int result = strtoull(s, &end, base);
  if (errno != 0 || s == end || *end != '\0') {
    return false;
  }
  if (max < result) {
    return false;
  }
  *out = static_cast<T>(result);
  return true;
}

// These printf-like functions are implemented in terms of vsnprintf, so they
// use the same attribute for compile-time format string checking. On Windows,
// if the mingw version of vsnprintf is used, use `gnu_printf' which allows z
// in %zd and PRIu64 (and related) to be recognized by the compile-time
// checking.
#define FORMAT_ARCHETYPE __printf__
#ifdef __USE_MINGW_ANSI_STDIO
#if __USE_MINGW_ANSI_STDIO
#undef FORMAT_ARCHETYPE
#define FORMAT_ARCHETYPE gnu_printf
#endif
#endif

// Returns a string corresponding to printf-like formatting of the arguments.
std::string StringPrintf(const char* fmt, ...)
    __attribute__((__format__(FORMAT_ARCHETYPE, 1, 2)));

}  // namespace stringutils
