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
#pragma once
#include <cstddef>
#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_status/status.h"

namespace pw::flash {

/// Represents a range in flash, relative to its start.
struct Range {
  std::size_t start;
  std::size_t size;
};

/// Basic parameters of the underlying flash.
struct FlashParams {
  /// Specifies the minimum block size for a write operation to flash.
  const size_t write_block_size;
  /// Specifies the value that is read back when reading a previously erased.
  /// page.
  const uint8_t erase_value;
};

/// Represents the page layout of a region in flash.
///
/// A region consists of `page_count` consecutive pages, each
/// of size `page_size`. An individual page can be freely erased.
struct PageLayout {
  /// Count of pages within the region
  std::size_t page_count;
  /// Size of each page within the region
  std::size_t page_size;
};

class Flash {
 public:
  constexpr Flash() = default;
  virtual ~Flash() = default;

  /// Initialize the flash.
  ///
  /// Performs initialization required to access the flash. `Initialize()` must
  /// be called before calling any other method on the `pw::flash::Flash`
  /// device.
  virtual Status Initialize() = 0;

  /// Read data from the flash.
  ///
  /// @param[in]  range   flash range to read from, relative to start of flash.
  /// @param[in]  buf     buffer to read to.
  virtual Status Read(Range range, ByteSpan buf) = 0;

  /// Erase the given flash range.
  ///
  /// @param[in]  range   flash range to erase, relative to start of flash.
  virtual Status Erase(Range range) = 0;

  /// Write the given byte span to a range in flash.
  ///
  /// The range must have been previously erased for this to succeed.
  /// The byte span must be equal in length to the range.
  /// @param[in]  range   flash range to write, relative to start of flash.
  /// @param[in]  data    data to write.
  virtual Status Write(Range range, ConstByteSpan data) = 0;

  /// Get basic flash parameters.
  virtual FlashParams GetFlashParameters() = 0;

  /// Get physical page layout of the flash.
  ///
  /// Each entry in the vector represents a region of the flash, which consists
  /// of one or more pages of a given size. This can be used to represent flash
  /// with uniform and non-uniform page sizes.
  virtual Vector<PageLayout> const& GetPageLayout() = 0;

 private:
};

}  // namespace pw::flash
