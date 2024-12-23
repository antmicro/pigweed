// Copyright 2025 The Pigweed Authors
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
#include <array>
#include <cstddef>
#include <vector>

#include "pw_bytes/array.h"
#include "pw_flash/flash.h"
#include "pw_status/try.h"

namespace examples {

constexpr std::size_t kPageSize = 4096;

pw::Status EraseSingleExample(pw::flash::Flash& flash) {
  // DOCSTAG: [pw_flash-erase-example]
  // All implementations of `pw::flash::Flash` must support erasing single
  // pages.
  PW_TRY(flash.Initialize());
  PW_TRY(flash.Erase(pw::flash::Range{0x0, 1 * kPageSize}));
  // DOCSTAG: [pw_flash-erase-example]

  return pw::OkStatus();
}

pw::Status EraseManyExample(pw::flash::Flash& flash) {
  // DOCSTAG: [pw_flash-erase-many-example]
  // Implementations may support erasing many pages within a single `Erase()`
  // call.
  PW_TRY(flash.Initialize());
  PW_TRY(flash.Erase(pw::flash::Range{0x0, 16 * kPageSize}));
  // DOCSTAG: [pw_flash-erase-many-example]

  return pw::OkStatus();
}

pw::Status WriteExampleFull(pw::flash::Flash& flash) {
  // DOCSTAG: [pw_flash-write-example]
  // All implementations of `pw::flash::Flash` must support writing single
  // pages.

  // You can only write to erased pages.
  constexpr pw::flash::Range range{0x0, 1 * kPageSize};
  PW_TRY(flash.Initialize());
  PW_TRY(flash.Erase(range));

  // Fill the first page with 0x42 bytes.
  auto buf = pw::bytes::Initialized<std::byte, kPageSize>(0x42);
  PW_TRY(flash.Write(range, buf));
  // DOCSTAG: [pw_flash-write-example]

  return pw::OkStatus();
}

pw::Status WriteExamplePartial(pw::flash::Flash& flash) {
  // DOCSTAG: [pw_flash-write-partial-example]
  // Implementations may support writing less than a full page.

  // You can only write to erased pages.
  PW_TRY(flash.Initialize());
  PW_TRY(flash.Erase(pw::flash::Range{0, 1 * kPageSize}));

  const pw::flash::FlashParams params = flash.GetFlashParameters();
  // `params.write_block_size` will contain the minimum writable size. This
  // can be used to check if the implementation can handle partial writes.

  // Fill the first 64 bytes of the first page with 0x42, and the following
  // 64 bytes with 0x24.
  constexpr std::size_t kWriteSize = 64;
  std::vector<std::byte> data(kWriteSize, (std::byte)0x42);
  std::vector<std::byte> atad(kWriteSize, (std::byte)0x24);
  PW_TRY(flash.Write(pw::flash::Range{0, kWriteSize}, data));
  PW_TRY(flash.Write(pw::flash::Range{64, kWriteSize}, atad));

  // DOCSTAG: [pw_flash-write-partial-example]

  (void)params;
  return pw::OkStatus();
}

pw::Status ReadExample(pw::flash::Flash& flash) {
  // DOCSTAG: [pw_flash-read-example]
  // All implementations of `pw::flash::Flash` must support reading single
  // pages.
  PW_TRY(flash.Initialize());

  std::array<std::byte, kPageSize> page_buf;
  PW_TRY(flash.Read(pw::flash::Range{0, kPageSize}, page_buf));

  // DOCSTAG: [pw_flash-read-example]

  return pw::OkStatus();
}

pw::Status ReadManyExample(pw::flash::Flash& flash) {
  // DOCSTAG: [pw_flash-read-many-example]
  // Implementations may support reading many pages within a single `Read()`
  // call.
  PW_TRY(flash.Initialize());

  std::array<std::byte, 4 * kPageSize> page_buf;
  PW_TRY(flash.Read(pw::flash::Range{0, 4 * kPageSize}, page_buf));

  // DOCSTAG: [pw_flash-read-many-example]

  return pw::OkStatus();
}

}  // namespace examples