#pragma once
#include <cstddef>

#include "pw_bytes/span.h"
#include "pw_flash/flash.h"
#include "pw_status/status.h"

namespace pw::flash {

class McuxpressoFlash : public pw::flash::Flash {
 public:
  /// Initialize the flash
  pw::Status Initialize() override;

  /// Erase the given flash range
  pw::Status Erase(Range) override;

  /// Write the given byte span to a range in flash
  /// The range must have been previously erased for this to succeed.
  /// The byte span must be equal in length to the range.
  pw::Status Program(Range, ConstByteSpan) override;

 private:
};

}  // namespace pw::flash
