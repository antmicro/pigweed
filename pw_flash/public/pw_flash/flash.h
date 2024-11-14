#pragma once
#include "pw_bytes/span.h"
#include "pw_status/status.h"

namespace pw::flash {

struct Range {
  std::size_t start;
  std::size_t size;
};

class Flash {
 public:
  constexpr Flash() = default;
  virtual ~Flash() = default;

  /// Initialize the flash
  virtual pw::Status Initialize() = 0;

  /// Erase the given flash range
  virtual pw::Status Erase(Range) = 0;

  /// Write the given byte span to a range in flash
  /// The range must have been previously erased for this to succeed.
  /// The byte span must be equal in length to the range.
  virtual pw::Status Program(Range, ConstByteSpan) = 0;

 private:
};

}  // namespace pw::flash
