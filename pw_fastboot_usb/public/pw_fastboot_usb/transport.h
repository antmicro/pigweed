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
#pragma once

#include "pw_bytes/span.h"
#include "pw_fastboot/transport.h"
#include "pw_fastboot_usb/packet.h"
#include "pw_fastboot_usb/ring_buffer.h"
#include "pw_sync/interrupt_spin_lock.h"

#define FASTBOOT_USB_SEND_RING_SIZE 512
#define FASTBOOT_USB_RECEIVE_RING_SIZE 16384

namespace pw::fastboot {
/*  Fastboot transport implementation for USB
 *
 *  This expects a UsbPacketInterface to be provided, which must allow
 *  for reading/writing data from the fastboot endpoints. It is the interface's
 *  responsibility to make sure that the device is detectable as a
 *  fastboot-compatible device by the host (by providing correct VID/PID/IF
 *  classes)
 */
class UsbTransport : public pw::fastboot::Transport {
 public:
  UsbTransport(std::unique_ptr<pw::fastboot::UsbPacketInterface> interface);

  std::ptrdiff_t Read(pw::ByteSpan) override;
  std::ptrdiff_t Write(pw::ConstByteSpan) override;
  int Close() override;
  int Reset() override;

 private:
  std::ptrdiff_t DoRead(pw::ByteSpan);
  std::ptrdiff_t DoWrite(pw::ConstByteSpan);

  void OnPacketSent(pw::ConstByteSpan);
  void OnPacketReceived(pw::ConstByteSpan);

  std::unique_ptr<pw::fastboot::UsbPacketInterface> packet_interface_;
  pw::sync::InterruptSpinLock state_lock_;
  RingBuffer<FASTBOOT_USB_SEND_RING_SIZE> send_ring_;
  RingBuffer<FASTBOOT_USB_RECEIVE_RING_SIZE> receive_ring_;
  std::byte send_buffer_[kFastbootUsbMaxPacketSize];
  bool send_in_progress_;
};
}  // namespace pw::fastboot
