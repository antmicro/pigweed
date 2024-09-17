#include "stringutils/strings.h"

#include <cstdarg>

#include "pw_assert/assert.h"

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

std::vector<std::string> Split(const std::string& s,
                               const std::string& delimiters) {
  PW_ASSERT(delimiters.size() != 0U);
  std::vector<std::string> result;
  size_t base = 0;
  size_t found;
  while (true) {
    found = s.find_first_of(delimiters, base);
    result.push_back(s.substr(base, found - base));
    if (found == s.npos)
      break;
    base = found + 1;
  }
  return result;
}

template std::string Join(const std::vector<std::string>&, char);
template std::string Join(const std::vector<const char*>&, char);
template std::string Join(const std::vector<std::string>&, const std::string&);
template std::string Join(const std::vector<const char*>&, const std::string&);

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  // First try with a small fixed size buffer
  char space[1024];
  // It's possible for methods that use a va_list to invalidate
  // the data in it upon use.  The fix is to make a copy
  // of the structure before using it and use that copy instead.
  va_list backup_ap;
  va_copy(backup_ap, ap);
  int result = vsnprintf(space, sizeof(space), format, backup_ap);
  va_end(backup_ap);
  if (result < static_cast<int>(sizeof(space))) {
    if (result >= 0) {
      // Normal case -- everything fit.
      dst->append(space, result);
      return;
    }
    if (result < 0) {
      // Just an error.
      return;
    }
  }
  // Increase the buffer size to the size requested by vsnprintf,
  // plus one for the closing \0.
  int length = result + 1;
  char* buf = new char[length];
  // Restore the va_list before we use it again
  va_copy(backup_ap, ap);
  result = vsnprintf(buf, length, format, backup_ap);
  va_end(backup_ap);
  if (result >= 0 && result < length) {
    // It fit
    dst->append(buf, result);
  }
  delete[] buf;
}
std::string StringPrintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::string result;
  StringAppendV(&result, fmt, ap);
  va_end(ap);
  return result;
}
void StringAppendF(std::string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(dst, format, ap);
  va_end(ap);
}

bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.substr(0, prefix.size()) == prefix;
}

}  // namespace stringutils