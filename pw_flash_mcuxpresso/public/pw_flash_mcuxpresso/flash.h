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

#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_flash/flash.h"
#include "pw_status/status.h"

namespace pw::flash {

class McuxpressoFlash : public pw::flash::Flash {
 public:
  /// Initialize the flash.
  ///
  /// Performs initialization required to access the flash. `Initialize()` must
  /// be called before calling any other method on the `pw::flash::Flash`
  /// device.
  ///
  /// WARNING: Initialization causes the FlexSPI peripheral to be reset. When
  /// running in XIP mode, this reset will cause an unrecoverable fault (as the
  /// CPU is currently running from the very same FlexSPI that is being reset).
  /// Any code using `McuxpressoFlash` must be executed entirely from SRAM.
  pw::Status Initialize() override;

  /// Read data from the flash.
  ///
  /// Read data from flash into the specified buffer. The start of the read
  /// range must be aligned to 4 bytes, and its size must be multiple of 4
  /// bytes.
  ///
  /// @param[in]  range   flash range to read from, relative to start of flash.
  /// @param[in]  buf     buffer to read to.
  pw::Status Read(Range, ByteSpan) override;

  /// Erase the given flash range.
  ///
  /// Erases the specified page(s). You can pass a range specifying many pages
  /// to be erased within a single `Erase()` call.
  ///
  /// The page size depends on the device and can be queried using
  /// `GetPageLayout()`.
  ///
  /// @param[in]  range   flash range to erase, relative to start of flash.
  pw::Status Erase(Range) override;

  /// Write the given byte span to a range in flash.
  ///
  /// Write block(s) of data to the flash. The range must have been previously
  /// erased for this to succeed. The byte span must be equal in length to the
  /// range.
  ///
  /// The minimum writable block depends on the device and can be queried using
  /// `GetFlashParameters()`.
  ///
  /// @param[in]  range   flash range to write, relative to start of flash.
  /// @param[in]  data    data to write.
  pw::Status Write(Range, ConstByteSpan) override;

  /// Get basic flash parameters
  FlashParams GetFlashParameters() override;

  /// Get physical page layout of the flash.
  ///
  /// Each entry in the vector represents a region of the flash, which consists
  /// of one or more pages of a given size. This can be used to represent flash
  /// with uniform and non-uniform page sizes.
  pw::Vector<PageLayout> const& GetPageLayout() override;

 private:
};

}  // namespace pw::flash
