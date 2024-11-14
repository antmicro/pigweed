#include <cstddef>
#include <cstdint>
#include <cstring>

#include "pw_bytes/array.h"
#include "pw_bytes/span.h"
#include "pw_flash/flash.h"
#include "pw_flash_mcuxpresso/flash.h"
#include "pw_preprocessor/util.h"
#include "pw_status/status.h"

PW_EXTERN_C_START
#include "mflash_drv.h"
PW_EXTERN_C_END

static constexpr const uint32_t kFlashBase = MFLASH_BASE_ADDRESS;
static constexpr const uint32_t kFlashEnd = kFlashBase + FLASH_SIZE * 1024;

static bool IsWithinFlash(pw::flash::Range const& range) {
  return range.start >= kFlashBase && (range.start + range.size < kFlashEnd);
}

static bool IsEraseAligned(pw::flash::Range const& range) {
  return (range.start % MFLASH_SECTOR_SIZE) == 0;
}

static bool IsWriteAligned(pw::flash::Range const& range) {
  return (range.start % MFLASH_PAGE_SIZE) == 0;
}

pw::Status pw::flash::McuxpressoFlash::Initialize() {
  const auto err = mflash_drv_init();
  return (err == kStatus_Success) ? OkStatus() : Status::Internal();
}

pw::Status pw::flash::McuxpressoFlash::Erase(pw::flash::Range range) {
  if (!IsWithinFlash(range) || !IsEraseAligned(range)) {
    return Status::InvalidArgument();
  }

  const auto sectors =
      (range.size + MFLASH_SECTOR_SIZE - 1) / MFLASH_SECTOR_SIZE;
  for (size_t i = 0; i < sectors; ++i) {
    const auto err = mflash_drv_sector_erase(range.start - kFlashBase +
                                             i * MFLASH_SECTOR_SIZE);
    if (err != kStatus_Success) {
      return Status::Internal();
    }
  }
  // If size of the erased range is not aligned to the sector size,
  // indicate to the caller that more was erased than requested.
  if ((range.size % MFLASH_SECTOR_SIZE) != 0) {
    return Status::DataLoss();
  }
  return OkStatus();
}

// `data` must point to a byte buffer of MFLASH_PAGE_SIZE bytes
// and be aligned on a word boundary.
static int32_t ProgramFullPage(uint32_t address, uint32_t const* data) {
  // const_cast: ``data`` is only read, SDK stores non-const pointer
  // in a generic transfer structure
  const auto err =
      mflash_drv_page_program(address, const_cast<uint32_t*>(data));
  return err;
}

// `data` can be of any alignment and size. Performs a copy to ensure
// alignment and add padding.
static int32_t ProgramPartialPage(uint32_t address,
                                  uint8_t const* data,
                                  size_t size) {
  alignas(uint32_t) auto buf =
      pw::bytes::Initialized<std::byte, MFLASH_PAGE_SIZE>(0xFF);
  std::memcpy(buf.data(), data, size);
  return ProgramFullPage(address,
                         reinterpret_cast<uint32_t const*>(buf.data()));
}

pw::Status pw::flash::McuxpressoFlash::Program(pw::flash::Range range,
                                               pw::ConstByteSpan data) {
  if (!IsWithinFlash(range) || !IsWriteAligned(range) ||
      (range.size != data.size_bytes())) {
    return Status::InvalidArgument();
  }

  const auto pages =
      (data.size_bytes() + MFLASH_PAGE_SIZE - 1) / MFLASH_PAGE_SIZE;
  for (size_t i = 0; i < pages; ++i) {
    const auto offset = i * MFLASH_PAGE_SIZE;
    const auto page_address = (range.start - kFlashBase) + offset;
    void const* ptr = data.data() + offset;

    int32_t err;
    if (offset + MFLASH_PAGE_SIZE > data.size_bytes()) {
      const auto remaining_bytes = data.size_bytes() % MFLASH_PAGE_SIZE;
      err = ProgramPartialPage(
          page_address, reinterpret_cast<uint8_t const*>(ptr), remaining_bytes);
    } else {
      err =
          ProgramFullPage(page_address, reinterpret_cast<uint32_t const*>(ptr));
    }

    if (err != kStatus_Success) {
      return Status::Internal();
    }
  }

  return OkStatus();
}
