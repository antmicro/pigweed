// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
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

static constexpr const size_t kFlashSize = FLASH_SIZE * 1024;
static constexpr const uint8_t kEraseValue = 0xFF;

static const pw::Vector<pw::flash::PageLayout, 1> kFlashLayout = {
    pw::flash::PageLayout{
        .page_count = kFlashSize / MFLASH_SECTOR_SIZE,
        .page_size = MFLASH_SECTOR_SIZE,
    }};

static constexpr bool IsEraseValid(pw::flash::Range const& range) {
  return (range.start % MFLASH_SECTOR_SIZE) == 0 &&
         (range.size % MFLASH_SECTOR_SIZE) == 0 &&
         range.start + range.size < kFlashSize;
}

static constexpr bool IsWriteValid(pw::flash::Range const& range) {
  return (range.start % MFLASH_PAGE_SIZE) == 0 && (range.size % 4) == 0 &&
         range.start + range.size < kFlashSize;
}

static constexpr bool IsReadValid(pw::flash::Range const& range) {
  return (range.start % 4) == 0 && (range.size % 4) == 0 &&
         range.start + range.size < kFlashSize;
}

pw::Status pw::flash::McuxpressoFlash::Initialize() {
  const auto err = mflash_drv_init();
  return (err == kStatus_Success) ? OkStatus() : Status::Internal();
}

pw::Status pw::flash::McuxpressoFlash::Erase(pw::flash::Range range) {
  if (!IsEraseValid(range)) {
    return Status::InvalidArgument();
  }

  const auto sectors =
      (range.size + MFLASH_SECTOR_SIZE - 1) / MFLASH_SECTOR_SIZE;
  for (size_t i = 0; i < sectors; ++i) {
    const auto err =
        mflash_drv_sector_erase(range.start + i * MFLASH_SECTOR_SIZE);
    if (err != kStatus_Success) {
      return Status::Internal();
    }
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

pw::Status pw::flash::McuxpressoFlash::Write(pw::flash::Range range,
                                             pw::ConstByteSpan data) {
  if (!IsWriteValid(range) || (range.size != data.size_bytes())) {
    return Status::InvalidArgument();
  }

  const auto pages =
      (data.size_bytes() + MFLASH_PAGE_SIZE - 1) / MFLASH_PAGE_SIZE;
  for (size_t i = 0; i < pages; ++i) {
    const auto offset = i * MFLASH_PAGE_SIZE;
    const auto page_address = range.start + offset;
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

pw::Status pw::flash::McuxpressoFlash::Read(Range range, ByteSpan buffer) {
  if (!IsReadValid(range) || range.size > buffer.size_bytes()) {
    return Status::InvalidArgument();
  }

  auto* word_ptr = reinterpret_cast<uint32_t*>(buffer.data());
  const auto err = mflash_drv_read(range.start, word_ptr, buffer.size_bytes());
  if (err != kStatus_Success) {
    return Status::Internal();
  }
  return OkStatus();
}

pw::flash::FlashParams pw::flash::McuxpressoFlash::GetFlashParameters() {
  return {
      .write_block_size = MFLASH_PAGE_SIZE,
      .erase_value = kEraseValue,
  };
}

pw::Vector<pw::flash::PageLayout> const&
pw::flash::McuxpressoFlash::GetPageLayout() {
  return kFlashLayout;
}
