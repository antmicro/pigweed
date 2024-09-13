#pragma once

#include <array>
#include <cstddef>
#include <cstdio>

#include "pw_bytes/span.h"
#include "pw_ring_buffer/prefixed_entry_ring_buffer.h"
#include "pw_status/status.h"
#include "pw_sync/counting_semaphore.h"

template <size_t QueueSizeInBytes>
class RingBuffer {
 public:
  RingBuffer() {
    (void)ring_.SetBuffer(buffer_);
    (void)ring_.AttachReader(reader_);
  }

  /*  Enqueue a ConstByteSpan to the ring
   *
   *  This operation always succeeds as long as ring size >= buffer size.
   *  If overflow occurs, old data is overridden. On success, callers to
   *  `WaitForData` are notified and unblocked.
   *  Returns size of data write, or -1 on error.
   */
  std::ptrdiff_t Enqueue(pw::ConstByteSpan data) {
    const auto err = ring_.PushBack(data);
    if (err == PW_STATUS_OK) {
      notification_.release();
      return data.size();
    }
    return -1;
  }

  /*  Try dequeueing data from the ring into the specified ByteSpan.
   *
   *  Returns size of data read, or -1 on error.
   */
  std::ptrdiff_t Dequeue(pw::ByteSpan destination) {
    size_t bytes_read = 0;
    const auto err = reader_.PeekFront(destination, &bytes_read);
    if (err == PW_STATUS_OK || err == PW_STATUS_RESOURCE_EXHAUSTED) {
      (void)reader_.PopFront();
      return bytes_read;
    }
    return -1;
  }

  /*  Wait for data to be enqueued to the ring
   *
   *  This function will block until data is available for consumption
   *  in the ring.
   */
  void WaitForData() { notification_.acquire(); }

 private:
  std::array<std::byte, QueueSizeInBytes> buffer_;
  pw::ring_buffer::PrefixedEntryRingBufferMulti ring_;
  pw::ring_buffer::PrefixedEntryRingBufferMulti::Reader reader_;
  pw::sync::CountingSemaphore notification_;
};
