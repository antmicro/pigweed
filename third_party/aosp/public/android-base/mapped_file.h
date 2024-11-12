#pragma once

#include <cstddef>
#include <cstdint>

#include "pw_assert/check.h"

enum PROT_MOCK {
  PROT_READ,
};

#define CRASH_STRING                                                        \
  "UNIMPLEMENTED: You may only use in-memory functions with this build of " \
  "libsparse."

namespace android::base {
class MappedFile {
 public:
  static MappedFile* FromFd(int, size_t, size_t, PROT_MOCK) {
    PW_CRASH(CRASH_STRING);
  }

  operator bool() const { PW_CRASH(CRASH_STRING); }

  char* data() { PW_CRASH(CRASH_STRING); }

 private:
};

}  // namespace android::base
