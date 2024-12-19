#pragma once
#include <cstddef>
#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_status/status.h"

namespace pw::flash {

struct Range {
  std::size_t start;
  std::size_t size;
};

/// Basic parameters of the underlying flash.
struct FlashParams {
  const size_t write_block_size;
  const uint8_t erase_value;
};

/// Represents the page layout of a region in flash.
/// A region consists of `page_count` consecutive pages, each
/// of size `page_size`.
struct PageLayout {
  std::size_t page_count;
  std::size_t page_size;
};

class Flash {
 public:
  constexpr Flash() = default;
  virtual ~Flash() = default;

  /// Initialize the flash
  virtual pw::Status Initialize() = 0;

  /// Read data from the flash
  virtual pw::Status Read(Range, ByteSpan) = 0;

  /// Erase the given flash range
  virtual pw::Status Erase(Range) = 0;

  /// Write the given byte span to a range in flash
  /// The range must have been previously erased for this to succeed.
  /// The byte span must be equal in length to the range.
  virtual pw::Status Write(Range, ConstByteSpan) = 0;

  /// Get basic flash parameters
  virtual FlashParams GetFlashParameters() = 0;

  /// Get physical page layout of the flash.
  /// Each entry in the vector represents a region of the flash, which can
  /// consist of one or more pages of a given size. This can be used to
  /// represent flash with uniform and non-uniform page sizes.
  virtual pw::Vector<PageLayout> const& GetPageLayout() = 0;

 private:
};

}  // namespace pw::flash
