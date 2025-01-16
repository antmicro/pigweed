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
#include <cstddef>

#include "pw_bytes/array.h"
#include "pw_flash_mcuxpresso/flash.h"
#include "pw_status/try.h"

namespace examples {

pw::Status FlashExample() {
  // DOCSTAG: [pw_flash_mcuxpresso-example]
  constexpr std::size_t kPageSize = 4096;
  constexpr pw::flash::Range range{0x0, 1 * kPageSize};
  pw::flash::McuxpressoFlash flash{};
  PW_TRY(flash.Initialize());
  PW_TRY(flash.Erase(range));
  auto buf = pw::bytes::Initialized<std::byte, kPageSize>(0x42);
  PW_TRY(flash.Write(range, pw::ConstByteSpan{buf}));
  // DOCSTAG: [pw_flash_mcuxpresso-example]

  return pw::OkStatus();
}
}  // namespace examples