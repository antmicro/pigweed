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
#include "pw_bluetooth_sapphire_mcuxpresso/controller.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>

#include "fsl_adapter_uart.h"
#include "pw_bluetooth/controller.h"
#include "pw_bluetooth/hci_util.h"
#include "pw_bluetooth_sapphire_mcuxpresso/config.h"
#include "pw_bytes/span.h"
#include "pw_hex_dump/log_bytes.h"
#include "pw_log/log.h"
#include "pw_span/cast.h"
#include "pw_span/span.h"
#include "pw_thread/detached_thread.h"

namespace {

//  Log all HCI data transmitted/received over the UART.
constexpr bool kDebugHciData =
    PW_BT_MCUXPRESSO_DUMP_HCI_DATA > 0 ? true : false;

}  // namespace

namespace pw::bluetooth {

constexpr char const* HalUartStatusToString(hal_uart_status_t status) {
  switch (status) {
    case kStatus_HAL_UartSuccess:
      return "success";
    case kStatus_HAL_UartTxBusy:
      return "tx busy";
    case kStatus_HAL_UartRxBusy:
      return "rx busy";
    case kStatus_HAL_UartTxIdle:
      return "tx idle";
    case kStatus_HAL_UartRxIdle:
      return "rx idle";
    case kStatus_HAL_UartBaudrateNotSupport:
      return "baudrate not supported";
    case kStatus_HAL_UartProtocolError:
      return "protocol error";
    case kStatus_HAL_UartError:
      return "HAL error";
    default:
      return "<INVALID>";
  }
}

// Calculate the size of the next UART read in order to read a full HCI packet.
// The caller is expected to re-call this function after each read completes.
// Completion of a full packet is signalled by the function returning zero.
static size_t GetHciUartNextReadSize(pw::ConstByteSpan data_received_so_far) {
  // If no data was read yet, read 1 byte to read the H4 packet type.
  if (data_received_so_far.empty()) {
    return 1;
  }
  const auto packet_type =
      static_cast<emboss::H4PacketType>(data_received_so_far[0]);

  // GetHciPayloadSize requires the full HCI header to be passed. If we don't
  // have the full header present, read the remaining bytes of the header.
  const auto header_size_for_type = GetHciHeaderSize(packet_type).value();
  const auto hci_packet = data_received_so_far.subspan(1);
  if (hci_packet.size_bytes() < header_size_for_type) {
    return header_size_for_type - hci_packet.size_bytes();
  }

  const auto payload_size = pw::bluetooth::GetHciPayloadSize(
      packet_type, pw::span_cast<const uint8_t>(hci_packet));
  const auto expected_packet_size = header_size_for_type + payload_size.value();
  // A full packet was already received.
  if (expected_packet_size < hci_packet.size_bytes()) {
    return 0;
  }
  return expected_packet_size - hci_packet.size_bytes();
}

StatusWithSize DecodeHciUartData(
    ConstByteSpan data,
    const Function<void(emboss::H4PacketType, pw::span<const std::byte>)>&
        packet_callback) {
  size_t bytes_consumed = 0;
  while (data.size_bytes() > 0) {
    const std::byte packet_indicator = data[0];
    data = data.subspan(1);  // Pop off the packet indicator byte.
    // Note that we do not yet claim the byte consumed until we know what it is,
    // as it may be a partial HCI packet which cannot be consumed until later.

    size_t packet_size_bytes = 0;
    auto payload_size =
        GetHciPayloadSize(static_cast<emboss::H4PacketType>(packet_indicator),
                          pw::span_cast<const uint8_t>(data));
    if (!payload_size.ok()) {
      // Unrecognized PacketIndicator type, we've lost synchronization!
      ++bytes_consumed;  // Consume the invalid packet indicator.
      return StatusWithSize::DataLoss(bytes_consumed);
    }
    auto header_size =
        GetHciHeaderSize(static_cast<emboss::H4PacketType>(packet_indicator))
            .value();
    // An illustrative H4 packet will look like this:
    //
    //    II HH HH PP PP PP .. .. ..
    //
    // Where: II - indicator byte, HH - packet header, PP - payload. Sapphire
    // requires us to pass everything starting from the packet header.
    packet_size_bytes = header_size + payload_size.value();

    auto packet_data = data.subspan(0, packet_size_bytes);
    packet_callback(static_cast<emboss::H4PacketType>(packet_indicator),
                    packet_data);

    data = data.subspan(packet_size_bytes);  // Pop off the HCI packet.
    // Consume the packet indicator and the packet.
    bytes_consumed += 1 + packet_size_bytes;
  }
  return StatusWithSize(bytes_consumed);
}

void McuxpressoUartController::OnH4PacketComplete() {
  bool new_packets_available = false;
  const auto bytes_consumed = DecodeHciUartData(
      GetRxPacketData(),
      [this, &new_packets_available](emboss::H4PacketType type,
                                     pw::span<const std::byte> packet_data) {
        hci_uart_rx_queue_.TryPushBack(packet_data,
                                       static_cast<uint32_t>(type));
        new_packets_available = true;
      });
  if (!bytes_consumed.ok()) {
    PW_LOG_ERROR("OnH4PacketComplete: synchronization lost");
  }
  if (new_packets_available) {
    hci_uart_cond_.release();
  }

  if (bytes_consumed.size() > 0) {
    // Data from the start of the buffer was consumed, move the remaining data
    // to the start of the rx buffer.
    const auto remaining_data =
        GetRxPacketData().subspan(bytes_consumed.size());
    std::memcpy(hci_uart_rx_buf_.data(),
                remaining_data.data(),
                remaining_data.size_bytes());
    hci_uart_rx_bytes_read_ = remaining_data.size_bytes();
  }
}

void McuxpressoUartController::OnHciUartRxComplete() {
  hci_uart_rx_bytes_read_ += hci_uart_rx_pending_read_size_;

  size_t next_read_size = GetHciUartNextReadSize(GetRxPacketData());
  if (next_read_size == 0) {
    // Prepare for reading the next H4 indicator byte
    next_read_size = 1;
    OnH4PacketComplete();
  }

  auto* read_pointer = hci_uart_rx_buf_.data() + hci_uart_rx_bytes_read_;
  HciUartReadAsync(pw::ByteSpan{read_pointer, next_read_size});
}

void McuxpressoUartController::OnHciUartTxComplete() {
  std::lock_guard lg{hci_uart_tx_queue_mutex_};
  hci_uart_tx_busy_ = false;

  auto command_buf = pw::ByteSpan{hci_uart_tx_buf_}.subspan(1);
  size_t packet_size = 0;
  uint32_t packet_type;
  const auto err = hci_uart_tx_queue_.PeekFrontWithPreamble(
      command_buf, packet_type, packet_size);
  if (!err.ok()) {
    return;
  }
  hci_uart_tx_queue_.PopFront();
  hci_uart_tx_busy_ = true;
  hci_uart_tx_buf_[0] = (std::byte)packet_type;
  auto packet = pw::ByteSpan{hci_uart_tx_buf_.data(), packet_size + 1};
  HciUartWriteAsync(packet);
}

void McuxpressoUartController::SdkHciUartTransmitCb(hal_uart_handle_t handle,
                                                    hal_uart_status_t status,
                                                    void* userData) {
  if (!handle || !userData) {
    return;
  }
  auto* controller_ptr = static_cast<McuxpressoUartController*>(userData);
  switch (status) {
    case kStatus_HAL_UartTxIdle:
      controller_ptr->OnHciUartTxComplete();
      break;
    case kStatus_HAL_UartRxIdle:
      controller_ptr->OnHciUartRxComplete();
      break;
    default:
      break;
  }
}

void McuxpressoUartController::HciUartWriteAsync(pw::ByteSpan buf) {
  const auto err = HAL_UartSendNonBlocking((hal_uart_handle_t)hci_uart_handle_,
                                           (uint8_t*)buf.data(),
                                           buf.size_bytes());
  if (err != kStatus_Success) {
    PW_LOG_ERROR("HciUartWriteAsync: HAL error (%s)",
                 HalUartStatusToString(err));
  }
}

void McuxpressoUartController::HciUartReadAsync(pw::ByteSpan buf) {
  hci_uart_rx_pending_read_size_ = buf.size_bytes();
  const auto err =
      HAL_UartReceiveNonBlocking((hal_uart_handle_t)hci_uart_handle_,
                                 (uint8_t*)buf.data(),
                                 hci_uart_rx_pending_read_size_);
  if (err != kStatus_HAL_UartSuccess) {
    PW_LOG_ERROR("HciUartReadAsync: HAL error (%s)",
                 HalUartStatusToString(err));
    hci_uart_rx_pending_read_size_ = 0;
  }
}

void McuxpressoUartController::HciUartSendPacketAsync(PacketType type,
                                                      pw::ConstByteSpan data) {
  // The required space is +1 to include the H4 packet type byte.
  PW_CHECK(data.size_bytes() + 1 <= hci_uart_tx_buf_.size());

  std::lock_guard lg{hci_uart_tx_queue_mutex_};
  if (hci_uart_tx_busy_) {
    hci_uart_tx_queue_.TryPushBack(data, type);
    return;
  }
  hci_uart_tx_busy_ = true;

  hci_uart_tx_buf_[0] = (std::byte)type;
  std::memcpy(&hci_uart_tx_buf_[1], data.data(), data.size_bytes());
  auto packet = pw::ByteSpan{hci_uart_tx_buf_.data(), data.size_bytes() + 1};

  if constexpr (kDebugHciData) {
    PW_LOG_DEBUG("Transmit HCI data:");
    pw::dump::LogBytes(PW_LOG_LEVEL_DEBUG, packet);
  }

  HciUartWriteAsync(packet);
}

void McuxpressoUartController::HciUartThread() {
  auto err = HciUartInit();
  if (!err.ok()) {
    complete_callback_(err);
    return;
  }
  complete_callback_(OkStatus());

  while (true) {
    hci_uart_cond_.acquire();

    while (hci_uart_rx_queue_.EntryCount() > 0) {
      hci_uart_rx_queue_mutex_.lock();

      uint32_t preamble;
      size_t packet_size;
      const auto err = hci_uart_rx_queue_.PeekFrontWithPreamble(
          hci_thread_pktbuf_, preamble, packet_size);
      if (!err.ok()) {
        hci_uart_rx_queue_mutex_.unlock();
        continue;
      }
      auto packet_type = static_cast<PacketType>(preamble);
      hci_uart_rx_queue_.PopFront();
      hci_uart_rx_queue_mutex_.unlock();

      auto packet_data =
          pw::span<const std::byte>(hci_thread_pktbuf_.data(), packet_size);

      if constexpr (kDebugHciData) {
        PW_LOG_INFO("Receive HCI data:");
        pw::dump::LogBytes(PW_LOG_LEVEL_INFO, packet_data);
      }

      switch (packet_type) {
        case kHciEventData:
          event_function_(packet_data);
          break;
        case kHciAclData:
          acl_function_(packet_data);
          break;
        default:
          PW_LOG_CRITICAL("UNKNOWN: packet_type: %d", packet_type);
          break;
      }
    }
  }
}

pw::Status McuxpressoUartController::HciUartInit() {
  hal_uart_status_t ret;
  hal_uart_status_t status;

  if (HAL_UartInit((hal_uart_handle_t)hci_uart_handle_, &hci_uart_config_) !=
      kStatus_HAL_UartSuccess) {
    return pw::Status::InvalidArgument();
  }

  if (HAL_UartInstallCallback((hal_uart_handle_t)hci_uart_handle_,
                              SdkHciUartTransmitCb,
                              this) != kStatus_HAL_UartSuccess) {
    return pw::Status::Unknown();
  }

  // Queue read of the first byte. In H4 protocol, this will contain the type of
  // the packet.
  HciUartReadAsync(pw::ByteSpan{hci_uart_rx_buf_.data(), 1});

  return pw::Status();
}

void McuxpressoUartController::SetEventFunction(DataFunction func) {
  event_function_ = std::move(func);
}

void McuxpressoUartController::SetReceiveAclFunction(DataFunction func) {
  acl_function_ = std::move(func);
}

void McuxpressoUartController::SetReceiveScoFunction(DataFunction func) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: SetReceiveScoFunction");
  (void)func;
}

// void Mimxrt595Controller::SetReceiveIsoFunction(DataFunction /*func*/) {}

void McuxpressoUartController::Initialize(
    Callback<void(Status)> complete_callback,
    Callback<void(Status)> error_callback) {
  hci_uart_rx_queue_.SetBuffer(hci_uart_rx_queue_storage_);
  hci_uart_tx_queue_.SetBuffer(hci_uart_tx_queue_storage_);
  error_callback_ = std::move(error_callback);
  complete_callback_ = std::move(complete_callback);

  pw::thread::DetachedThread(
      uart_thread_options_,
      pw::bind_member<&McuxpressoUartController::HciUartThread>(this));
}

void McuxpressoUartController::Close(Callback<void(Status)> callback) {
  HAL_UartAbortReceive(hci_uart_handle_);
  HAL_UartAbortSend(hci_uart_handle_);
  callback(Status());
}

void McuxpressoUartController::SendCommand(span<const std::byte> command) {
  HciUartSendPacketAsync(kHciCommand, command);
}

void McuxpressoUartController::SendAclData(span<const std::byte> data) {
  HciUartSendPacketAsync(kHciAclData, data);
}

void McuxpressoUartController::SendScoData(span<const std::byte> data) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: SendScoData");
  (void)data;
}

// void Mimxrt595Controller::SendIsoData(span<const std::byte> /*data*/) {}

void McuxpressoUartController::ConfigureSco(ScoCodingFormat coding_format,
                                            ScoEncoding encoding,
                                            ScoSampleRate sample_rate,
                                            Callback<void(Status)> callback) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: ConfigureSco");
  (void)coding_format;
  (void)encoding;
  (void)sample_rate;
  (void)callback;
}

void McuxpressoUartController::ResetSco(Callback<void(Status)> callback) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: ResetSco");
  (void)callback;
}

void McuxpressoUartController::GetFeatures(
    Callback<void(FeaturesBits)> callback) {
  constexpr FeaturesBits features = FeaturesBits::kHciSco |
                                    FeaturesBits::kHciIso |
                                    FeaturesBits::kSetAclPriorityCommand;
  callback(features);
}

void McuxpressoUartController::EncodeVendorCommand(
    VendorCommandParameters parameters,
    Callback<void(Result<span<const std::byte>>)> callback) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: EncodeVendorCommand");
  (void)parameters;
  (void)callback;
}

}  // namespace pw::bluetooth
