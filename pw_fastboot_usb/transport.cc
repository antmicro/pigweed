#include "pw_fastboot_usb/transport.h"

#include <pw_fastboot_usb/packet.h>

#include <cstdio>
#include <mutex>

#include "pw_bytes/span.h"
#include "pw_log/log.h"
#include "pw_sync/thread_notification.h"

pw::fastboot::UsbTransport::UsbTransport(
    std::unique_ptr<pw::fastboot::UsbPacketInterface> interface)
    : packet_interface_(std::move(interface)), send_in_progress_(false) {
  packet_interface_->SetPacketSentFunction(
      [this](pw::ConstByteSpan buf) { this->OnPacketSent(buf); });
  packet_interface_->SetPacketReceivedFunction(
      [this](pw::ConstByteSpan buf) { this->OnPacketReceived(buf); });

  pw::sync::ThreadNotification transport_init_notification;
  packet_interface_->Init([&transport_init_notification](bool success) {
    transport_init_notification.release();
    (void)success;
  });
  transport_init_notification.acquire();
}

std::ptrdiff_t pw::fastboot::UsbTransport::Read(pw::ByteSpan buf) {
  auto* byte_data = (std::byte*)buf.data();
  size_t bytes_read_total = 0;
  while (bytes_read_total < buf.size()) {
    // Limit read to maximum we can receive from a USB packet
    const auto bytes_to_read =
        std::min(buf.size() - bytes_read_total, kFastbootUsbMaxPacketSize);
    const auto bytes_read_now =
        this->DoRead(pw::ByteSpan{byte_data, bytes_to_read});
    if (bytes_read_now < 0) {
      return bytes_read_total == 0 ? -1 : bytes_read_total;
    }
    bytes_read_total += bytes_read_now;
    byte_data += bytes_read_now;
    if (static_cast<size_t>(bytes_read_now) < bytes_to_read) {
      break;
    }
  }
  return bytes_read_total;
}

std::ptrdiff_t pw::fastboot::UsbTransport::Write(pw::ConstByteSpan buf) {
  auto const* byte_data = (std::byte const*)buf.data();
  size_t bytes_written_total = 0;
  while (bytes_written_total < buf.size()) {
    // Limit write to maximum we can write in a USB packet
    const auto bytes_to_write =
        std::min(buf.size() - bytes_written_total, kFastbootUsbMaxPacketSize);
    const auto bytes_written_now =
        this->DoWrite(pw::ConstByteSpan{byte_data, bytes_to_write});
    if (bytes_written_now < 0) {
      return bytes_written_total == 0 ? -1 : bytes_written_total;
    }
    bytes_written_total += bytes_written_now;
    byte_data += bytes_written_now;
    if (static_cast<size_t>(bytes_written_now) < bytes_to_write) {
      break;
    }
  }
  return bytes_written_total;
}

int pw::fastboot::UsbTransport::Close() {
  PW_LOG_WARN("UNIMPLEMENTED: pw::fastboot::UsbTransport::Close");
  return 0;
}

int pw::fastboot::UsbTransport::Reset() {
  PW_LOG_WARN("UNIMPLEMENTED: pw::fastboot::UsbTransport::Reset");
  return 0;
}

std::ptrdiff_t pw::fastboot::UsbTransport::DoRead(pw::ByteSpan buf) {
  receive_ring_.WaitForData();
  std::lock_guard lg{state_lock_};
  return receive_ring_.Dequeue(buf);
}

std::ptrdiff_t pw::fastboot::UsbTransport::DoWrite(pw::ConstByteSpan buf) {
  std::ptrdiff_t n;
  {
    std::lock_guard lg{state_lock_};
    // If we have an inflight packet, queue it to be sent later
    if (send_in_progress_) {
      n = send_ring_.Enqueue(buf);
    } else {
      n = packet_interface_->QueuePacket(buf);
      send_in_progress_ = n > 0;
    }
  }
  return n;
}

void pw::fastboot::UsbTransport::OnPacketSent(pw::ConstByteSpan) {
  std::lock_guard lg{state_lock_};
  send_in_progress_ = false;

  const auto n = send_ring_.Dequeue(pw::ByteSpan{send_buffer_});
  if (n > 0) {
    const auto queued = packet_interface_->QueuePacket(
        pw::ConstByteSpan{send_buffer_, (size_t)n});
    send_in_progress_ = queued > 0;
  }
}

void pw::fastboot::UsbTransport::OnPacketReceived(pw::ConstByteSpan buf) {
  std::lock_guard lg{state_lock_};
  receive_ring_.Enqueue(buf);
}
