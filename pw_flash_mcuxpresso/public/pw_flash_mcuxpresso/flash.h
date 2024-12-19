#pragma once
#include <cstddef>

#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_flash/flash.h"
#include "pw_status/status.h"

namespace pw::flash {

class McuxpressoFlash : public pw::flash::Flash {
 public:
  /// Initialize the flash
  pw::Status Initialize() override;

  /// Read data from the flash
  pw::Status Read(Range, ByteSpan) override;

  /// Erase the given flash range
  pw::Status Erase(Range) override;

  /// Write the given byte span to a range in flash
  /// The range must have been previously erased for this to succeed.
  /// The byte span must be equal in length to the range.
  pw::Status Write(Range, ConstByteSpan) override;

  /// Get basic flash parameters
  FlashParams GetFlashParameters() override;

  /// Get physical page layout of the flash.
  /// Each entry in the vector represents a region of the flash, which can
  /// consist of one or more pages of a given size. This can be used to
  /// represent flash with uniform and non-uniform page sizes.
  pw::Vector<PageLayout> const& GetPageLayout() override;

 private:
};

}  // namespace pw::flash
