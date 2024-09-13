#pragma once

#include <cstddef>
#include <cstdio>

#include "pw_bytes/span.h"
#include "pw_function/function.h"

namespace pw::fastboot {

/*  Maximum size of a fastboot packet
 *
 *  This should be equal to the maximum packet size specified
 *  in the device descriptor, which directly depends on the USB
 *  link speed. 64 bytes for full-speed, 512 bytes for high-speed
 *  and 1024 bytes for Super Speed USB. This needs to be as big
 *  as the highest supported link speed.
 */
static constexpr size_t kFastbootUsbMaxPacketSize = 512U;

/*  Interface for generic send/receive operations over an USB endpoint
 *
 *  In the context of pw::fastboot, a derived implementation is expected
 *  to configure its USB stack to report a fastboot-compatible VID/PID and
 *  interface and provide events of packet send/receive, along with the
 *  ability to send a USB packet over the endpoint.
 */
class UsbPacketInterface {
 public:
  UsbPacketInterface() = default;
  virtual ~UsbPacketInterface() = default;

  /*  Callback for initialization status update
   *
   *  Initialization status, either success or failure, is communicated
   *  by invoking this callback.
   */
  using InitStatusUpdateCb = pw::Function<void(bool)>;
  /*  Initialize the interface
   *
   *  After initialization is completed (signaled by an initialization status
   *  callback), the implementation shall start providing packet sent/received
   *  events using `OnPacketReceived` and `OnPacketSent`.
   */
  virtual void Init(InitStatusUpdateCb) = 0;
  /*  Deinitialize the interface
   */
  virtual void Deinit() = 0;
  /*  Enqueue a packet to be sent
   *
   *  The lifetime of the passed span is limited to this call only,
   *  concrete implementations are expected to copy the contents to
   *  their own buffer.
   */
  virtual std::ptrdiff_t QueuePacket(pw::ConstByteSpan) = 0;

  /*  Callback for packet received events
   *
   *  The span contains the packet that was received. Lifetime of the
   *  passed span is assumed to be at least for the duration of the
   *  callback.
   */
  using PacketReceivedCb = pw::Function<void(pw::ConstByteSpan)>;
  /*  Callback for packet sent events
   *
   *  Called when a previously queued packet was sent out.
   */
  using PacketSentCb = pw::Function<void(pw::ConstByteSpan)>;

  void SetPacketReceivedFunction(PacketReceivedCb cb) {
    packet_received_ = std::move(cb);
  }
  void SetPacketSentFunction(PacketSentCb cb) { packet_sent_ = std::move(cb); }

  void OnPacketReceived(pw::ConstByteSpan buf) {
    if (packet_received_) {
      packet_received_(buf);
    }
  }
  void OnPacketSent(pw::ConstByteSpan buf) {
    if (packet_sent_) {
      packet_sent_(buf);
    }
  }

 private:
  PacketReceivedCb packet_received_;
  PacketSentCb packet_sent_;
};

}  // namespace pw::fastboot
