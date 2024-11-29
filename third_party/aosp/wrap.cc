#include "pw_assert/check.h"

#define CRASH_STRING                                                      \
  "Reached unsupported libsparse function. File-based functions are not " \
  "available in the MCU build of libsparse."

extern "C" [[noreturn]] void __wrap_sparse_file_add_file() {
  PW_CRASH(CRASH_STRING);
}

extern "C" [[noreturn]] void __wrap_sparse_file_add_fd() {
  PW_CRASH(CRASH_STRING);
}

extern "C" [[noreturn]] void __wrap_sparse_file_write() {
  PW_CRASH(CRASH_STRING);
}

extern "C" [[noreturn]] void __wrap_sparse_file_read() {
  PW_CRASH(CRASH_STRING);
}

extern "C" [[noreturn]] void __wrap_sparse_file_import() {
  PW_CRASH(CRASH_STRING);
}

extern "C" [[noreturn]] void __wrap_sparse_file_import_auto() {
  PW_CRASH(CRASH_STRING);
}

extern "C" [[noreturn]] void __wrap_open() {
  PW_CRASH(CRASH_STRING);
}

extern "C" [[noreturn]] void __wrap_close() {
  PW_CRASH(CRASH_STRING);
}
