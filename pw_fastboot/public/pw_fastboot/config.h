#pragma once

/// Defines the maximum allowed DOWNLOAD size.
/// Data received during a DOWNLOAD will be stored in a fixed-size
/// buffer allocated from the system heap. This defines the size of
/// this buffer.
#ifndef PW_FASTBOOT_MAX_DOWNLOAD_SIZE
#define PW_FASTBOOT_MAX_DOWNLOAD_SIZE (64 * 1024 - 1)
#endif
