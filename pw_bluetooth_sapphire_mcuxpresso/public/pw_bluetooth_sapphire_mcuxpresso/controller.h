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

#include <array>
#include <atomic>
#include <cstddef>

#include "pw_bluetooth/controller.h"
#include "pw_bluetooth_sapphire_mcuxpresso/config.h"
#include "pw_bytes/span.h"
#include "pw_ring_buffer/prefixed_entry_ring_buffer.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/thread_notification.h"
#include "pw_thread/options.h"

PW_EXTERN_C_START
#include "fsl_adapter_uart.h"
PW_EXTERN_C_END

namespace pw::bluetooth {

class McuxpressoUartController final : public pw::bluetooth::Controller {
 public:
  McuxpressoUartController(const hal_uart_config_t& config,
                           const thread::Options& uart_thread_options)
      : uart_thread_options_(uart_thread_options), hci_uart_config_(config) {}

  // Sets a function that will be called with HCI event packets received from
  // the controller. This should  be called before `Initialize` or else incoming
  // packets will be dropped. The lifetime of data passed to `func` is only
  // guaranteed for the lifetime of the function call.
  void SetEventFunction(DataFunction func) override;

  // Sets a function that will be called with ACL data packets received from the
  // controller. This should be called before `Initialize` or else incoming
  // packets will be dropped. The lifetime of data passed to `func` is only
  // guaranteed for the lifetime of the function call.
  void SetReceiveAclFunction(DataFunction func) override;

  // Sets a function that will be called with SCO packets received from the
  // controller. On Classic and Dual Mode stacks, this should be called before
  // `Initialize` or else incoming packets will be dropped. The lifetime of data
  // passed to `func` is only guaranteed for the lifetime of the function call.
  void SetReceiveScoFunction(DataFunction func) override;

  // Sets a function that will be called with ISO packets received from the
  // controller. This should be called before an ISO data path is setup or else
  // incoming packets may be dropped. The lifetime of data passed to `func` is
  // only guaranteed for the lifetime of the function call.
  void SetReceiveIsoFunction(DataFunction /*func*/) override {}

  // Initializes the controller interface and starts processing packets.
  // `complete_callback` will be called with the result of initialization.
  // `error_callback` will be called for fatal errors that occur after
  // initialization. After a fatal error, this object is invalid. `Close` should
  // be called to ensure a safe clean up.
  void Initialize(Callback<void(Status)> complete_callback,
                  Callback<void(Status)> error_callback) override;

  // Closes the controller interface, resetting all state. `callback` will be
  // called when closure is complete. After this method is called, this object
  // should be considered invalid and no other methods should be called
  // (including `Initialize`).
  // `callback` will be called with:
  // OK - the controller interface was successfully closed, or is already closed
  // INTERNAL - the controller interface could not be closed
  void Close(Callback<void(Status)> callback) override;

  // Sends an HCI command packet to the controller.
  void SendCommand(span<const std::byte> command) override;

  // Sends an ACL data packet to the controller.
  void SendAclData(span<const std::byte> data) override;

  // Sends a SCO data packet to the controller.
  void SendScoData(span<const std::byte> data) override;

  // Sends an ISO data packet to the controller.
  void SendIsoData(span<const std::byte> /*data*/) override {}

  // Configure the HCI for a SCO connection with the indicated parameters.
  // `SetReceiveScoFunction` must be called before calling this method.
  // `callback will be called with:
  // OK - success, packets can be sent/received.
  // UNIMPLEMENTED - the implementation/controller does not support SCO over HCI
  // ALREADY_EXISTS - a SCO connection is already configured
  // INTERNAL - an internal error occurred
  void ConfigureSco(ScoCodingFormat coding_format,
                    ScoEncoding encoding,
                    ScoSampleRate sample_rate,
                    Callback<void(Status)> callback) override;

  // Releases the resources held by an active SCO connection. This should be
  // called when a SCO connection is closed. `ConfigureSco` must be called
  // before calling this method.
  // `callback will be called with:
  // OK - success, the SCO configuration was reset.
  // UNIMPLEMENTED - the implementation/controller does not support SCO over HCI
  // INTERNAL - an internal error occurred
  void ResetSco(Callback<void(Status)> callback) override;

  // Calls `callback` with a bitmask of features supported by the controller.
  void GetFeatures(Callback<void(FeaturesBits)> callback) override;

  // Encodes the vendor command indicated by `parameters`.
  // `callback` will be called with the result of the encoding request.
  // The lifetime of data passed to `callback` is only guaranteed for the
  // lifetime of the function call.
  void EncodeVendorCommand(
      VendorCommandParameters parameters,
      Callback<void(Result<span<const std::byte>>)> callback) override;

 private:
  enum PacketType : uint8_t {
    kHciCommand = 0x01,
    kHciAclData = 0x02,
    kHciScoData = 0x03,
    kHciEventData = 0x04,
    kHciIsoData = 0x05,
  };

  void OnH4PacketComplete();
  void OnHciUartRxComplete();
  void OnHciUartTxComplete();
  static void SdkHciUartTransmitCb(hal_uart_handle_t handle,
                                   hal_uart_status_t status,
                                   void* userData);
  void HciUartReadAsync(pw::ByteSpan buf);
  void HciUartWriteAsync(pw::ByteSpan buf);
  void HciUartSendPacketAsync(PacketType type, pw::ConstByteSpan data);
  pw::Status HciUartInit();
  void HciUartThread();

  Callback<void(Status)> complete_callback_;
  Callback<void(Status)> error_callback_;
  DataFunction event_function_;
  DataFunction acl_function_;
  const pw::thread::Options& uart_thread_options_;

  // Temporary buffer owned by the UART thread.
  std::array<std::byte, 1024> hci_thread_pktbuf_{};

  // Temporary buffer to store data currently being received by the HAL.
  std::array<std::byte, 1024> hci_uart_rx_buf_{};
  // Indicates the amount of received bytes in `hci_uart_rx_buf_`.
  size_t hci_uart_rx_bytes_read_{};
  // Indicates the size of the current in-progress UART read.
  size_t hci_uart_rx_pending_read_size_{};
  // Used to signal the processing thread that new packets are available.
  pw::sync::ThreadNotification hci_uart_cond_{};

  UART_HANDLE_DEFINE(hci_uart_handle_) {};
  hal_uart_config_t hci_uart_config_{};

  // Get a span representing the data that was received from the UART so far.
  inline pw::ConstByteSpan GetRxPacketData() {
    return pw::ConstByteSpan{hci_uart_rx_buf_.data(), hci_uart_rx_bytes_read_};
  }

  // Packets to be transmitted over the HCI UART. The preamble is used to store
  // the H4 packet type indicator.
  pw::ring_buffer::PrefixedEntryRingBuffer hci_uart_tx_queue_{
      /*user_preamble=*/true};
  // Backing storage for hci_uart_tx_queue_.
  std::array<std::byte, PW_BT_MCUXPRESSO_TX_QUEUE_SIZE>
      hci_uart_tx_queue_storage_{};
  // Temporary buffer to store packets currently being transmitted by the HAL.
  std::array<std::byte, 1024> hci_uart_tx_buf_{};
  // Protects hci_uart_tx_queue.
  pw::sync::InterruptSpinLock hci_uart_tx_queue_mutex_{};
  // Indicates that the UART is currently busy sending data.
  std::atomic_bool hci_uart_tx_busy_{false};

  // Packets received over the HCI UART. The preamble is used to store the H4
  // packet type indicator.
  pw::ring_buffer::PrefixedEntryRingBuffer hci_uart_rx_queue_{
      /*user_preamble=*/true};
  // Backing storage for hci_uart_rx_queue.
  std::array<std::byte, PW_BT_MCUXPRESSO_RX_QUEUE_SIZE>
      hci_uart_rx_queue_storage_{};
  // Protects hci_uart_rx_queue.
  pw::sync::InterruptSpinLock hci_uart_rx_queue_mutex_{};
};

}  // namespace pw::bluetooth
