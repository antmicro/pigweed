#pragma once

#include "cpp-string/string_printf.h"

namespace android::base {

constexpr auto StringPrintf = bt_lib_cpp_string::StringPrintf;
constexpr auto StringAppendV = bt_lib_cpp_string::StringVAppendf;

}  // namespace android::base
