#include "pw_bluetooth_sapphire_mcuxpresso/controller.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <utility>

#include "controller/controller.h"
#include "pw_bluetooth/controller.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_containers/inline_deque.h"
#include "pw_containers/vector.h"
#include "pw_log/log.h"
#include "pw_preprocessor/util.h"
#include "pw_thread/detached_thread.h"
PW_EXTERN_C_START
#include "controller_hci_uart.h"
PW_EXTERN_C_END
namespace pw::bluetooth {

static pw::Vector<std::byte, 2048> EventPacketFromBytes(std::byte* data) {
  bt::hci_spec::EventHeader* eventHeader =
      reinterpret_cast<bt::hci_spec::EventHeader*>(data);
  // PW_CHECK_NULL(eventHeader);
  return std::move(
      pw::Vector<std::byte, 2048>(data,
                                  data + sizeof(bt::hci_spec::EventHeader) +
                                      eventHeader->parameter_total_size));
}

static pw::Vector<std::byte, 2048> ACLPacketFromBytes(std::byte* data) {
  bt::hci_spec::ACLDataHeader* eventHeader =
      reinterpret_cast<bt::hci_spec::ACLDataHeader*>(data);
  // PW_CHECK_NULL(eventHeader);
  return std::move(
      pw::Vector<std::byte, 2048>(data,
                                  data + sizeof(bt::hci_spec::ACLDataHeader) +
                                      eventHeader->data_total_length));
}

static void Mimxrt595Controller::hci_uart_transmit_cb(hal_uart_handle_t handle,
                                                      hal_uart_status_t status,
                                                      void* userData) {
  // TODO: make sure if this function doesn't need synchronization with
  // 'hci_uart_write_data'
  if (handle == nullptr) {
    return;
  }
  Mimxrt595Controller* controller_ptr =
      static_cast<Mimxrt595Controller*>(userData);
  if (controller_ptr == nullptr) {
    return;
  }

  switch (status) {
    case kStatus_HAL_UartTxIdle:
      break;
    case kStatus_HAL_UartRxIdle:
      controller_ptr->hci_uart_rx_bytes_ptr +=
          controller_ptr->hci_uart_rx.dataSize;
      ht_parse_packet(&controller_ptr->ht);

      if (controller_ptr->ht.packet_expected_len == 1) {
        PacketType packet_type = (PacketType)controller_ptr->hci_uart_rx_buf[0];
        std::byte* packet_data = controller_ptr->hci_uart_rx_buf.data() + 1;
        {
          std::lock_guard lock(controller_ptr->hci_uart_tx_queue_mutex);
          switch (packet_type) {
            case kHciEventData:
              controller_ptr->hci_uart_tx_queue.push(std::make_pair(
                  packet_type, EventPacketFromBytes(packet_data)));
              break;
            case kHciAclData:
              controller_ptr->hci_uart_tx_queue.push(
                  std::make_pair(packet_type, ACLPacketFromBytes(packet_data)));
              break;
            default:
              PW_LOG_CRITICAL("UNIMPLEMENTED: packet_type: %d", packet_type);
              break;
          }
        }

        controller_ptr->hci_uart_rx_bytes_ptr = 0U;
        controller_ptr->hci_uart_cond.release();
      }

      controller_ptr->hci_uart_rx.data =
          &controller_ptr
               ->hci_uart_rx_buf[controller_ptr->hci_uart_rx_bytes_ptr];
      controller_ptr->hci_uart_rx.dataSize =
          controller_ptr->ht.packet_expected_len;

      HAL_UartReceiveNonBlocking(
          (hal_uart_handle_t)controller_ptr->hci_uart_handle,
          (uint8_t*)controller_ptr->hci_uart_rx.data,
          controller_ptr->hci_uart_rx.dataSize);
      break;
    default:
      break;
  }
}

void Mimxrt595Controller::hci_uart_write_data(std::byte* buf, uint16_t length) {
  HAL_UartSendNonBlocking(
      (hal_uart_handle_t)hci_uart_handle, (uint8_t*)buf, length);
}

void Mimxrt595Controller::hci_uart_send_data(PacketType type,
                                             uint8_t* buf,
                                             uint16_t length) {
  hci_uart_tx_buf[0] = (std::byte)type;
  std::memcpy(&hci_uart_tx_buf[1], buf, length);

  hci_uart_write_data(hci_uart_tx_buf.data(), length + 1);
}

pw::Status Mimxrt595Controller::hci_uart_init() {
  hal_uart_config_t config;
  hal_uart_status_t ret;
  hal_uart_status_t status;

  controller_hci_uart_config_t getConfig;

  if (controller_hci_uart_get_configuration(&getConfig) != 0) {
    return pw::Status::InvalidArgument();
  }

  config.srcClock_Hz = getConfig.clockSrc;
  config.baudRate_Bps = getConfig.runningBaudrate;
  config.parityMode = kHAL_UartParityDisabled;
  config.stopBitCount = kHAL_UartOneStopBit;
  config.enableRx = 1U;
  config.enableTx = 1U;
  config.instance = getConfig.instance;
  config.enableRxRTS = getConfig.enableRxRTS;
  config.enableTxCTS = getConfig.enableTxCTS;

  if (HAL_UartInit((hal_uart_handle_t)hci_uart_handle, &config) !=
      kStatus_HAL_UartSuccess) {
    return pw::Status::InvalidArgument();
  }

  if (HAL_UartInstallCallback((hal_uart_handle_t)hci_uart_handle,
                              hci_uart_transmit_cb,
                              this) != kStatus_HAL_UartSuccess) {
    return pw::Status::Unknown();
  }

  ht_parse_packet_init(&ht);
  hci_uart_rx_bytes_ptr = 0U;
  ht.packet = (uint8_t*)&hci_uart_rx_buf[0U];

  hci_uart_rx.data = &hci_uart_rx_buf[hci_uart_rx_bytes_ptr];
  hci_uart_rx.dataSize = ht.packet_expected_len;

  if (HAL_UartReceiveNonBlocking((hal_uart_handle_t)hci_uart_handle,
                                 (uint8_t*)hci_uart_rx.data,
                                 hci_uart_rx.dataSize) !=
      kStatus_HAL_UartSuccess) {
    return pw::Status::Unknown();
  }

  pw::thread::DetachedThread(
      pw::thread::freertos::Options()
          .set_name("EtherMind UART Task")
          .set_priority(BT_TASK_PRIORITY + 2U),
      [this]() {
        while (true) {
          hci_uart_cond.acquire();

          while (!hci_uart_tx_queue.empty()) {
            hci_uart_tx_queue_mutex.lock();
            auto [packet_type, packet_data] = hci_uart_tx_queue.front();
            hci_uart_tx_queue.pop();
            hci_uart_tx_queue_mutex.unlock();

            switch (packet_type) {
              case kHciEventData:
                event_function_(pw::span<const std::byte>(packet_data.data(),
                                                          packet_data.size()));
                break;
              case kHciAclData:
                acl_function_(pw::span<const std::byte>(packet_data.data(),
                                                        packet_data.size()));
                break;
              default:
                PW_LOG_CRITICAL("UNKNOWN: packet_type: %d", packet_type);
                break;
            }
          }
        }
      });

  return pw::Status();
}

void Mimxrt595Controller::SetEventFunction(DataFunction func) {
  event_function_ = std::move(func);
}

void Mimxrt595Controller::SetReceiveAclFunction(DataFunction func) {
  acl_function_ = std::move(func);
}

void Mimxrt595Controller::SetReceiveScoFunction(DataFunction func) {
  (void)func;
  PW_LOG_CRITICAL("UNIMPLEMENTED: SetReceiveScoFunction");
}

// void Mimxrt595Controller::SetReceiveIsoFunction(DataFunction /*func*/) {}

void Mimxrt595Controller::Initialize(Callback<void(Status)> complete_callback,
                                     Callback<void(Status)> error_callback) {
  error_callback_ = std::move(error_callback);
  pw::thread::DetachedThread(
      pw::thread::freertos::Options().set_name(
          "Mimxrt595Controller Initialize Thread"),
      [complete_callback_ = std::move(complete_callback), this]() mutable {
        controller_init();

        complete_callback_(hci_uart_init());

        PW_LOG_INFO("Bluetooth initialized");
      });
}

void Mimxrt595Controller::Close(Callback<void(Status)> callback) {
  // TODO: Previously used API didn't had a close function
  // now, we are using bare HCI API, check again that it still doesn't allow
  // to close
  callback(Status());
}

void Mimxrt595Controller::SendCommand(span<const std::byte> command) {
  auto command_buffer = bt::DynamicByteBuffer(bt::BufferView(command));
  hci_uart_send_data(kHciCommand, command_buffer.data(), command_buffer.size());
}

void Mimxrt595Controller::SendAclData(span<const std::byte> data) {
  auto data_buffer = bt::DynamicByteBuffer(bt::BufferView(data));
  hci_uart_send_data(kHciAclData, data_buffer.data(), data_buffer.size());
}

void Mimxrt595Controller::SendScoData(span<const std::byte> data) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: SendScoData");
  (void)data;
}

// void Mimxrt595Controller::SendIsoData(span<const std::byte> /*data*/) {}

void Mimxrt595Controller::ConfigureSco(ScoCodingFormat coding_format,
                                       ScoEncoding encoding,
                                       ScoSampleRate sample_rate,
                                       Callback<void(Status)> callback) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: ConfigureSco");
  (void)coding_format;
  (void)encoding;
  (void)sample_rate;
  (void)callback;
}

void Mimxrt595Controller::ResetSco(Callback<void(Status)> callback) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: ResetSco");
  (void)callback;
}

void Mimxrt595Controller::GetFeatures(Callback<void(FeaturesBits)> callback) {
  constexpr FeaturesBits features = FeaturesBits::kHciSco |
                                    FeaturesBits::kHciIso |
                                    FeaturesBits::kSetAclPriorityCommand;
  callback(features);
}

void Mimxrt595Controller::EncodeVendorCommand(
    VendorCommandParameters parameters,
    Callback<void(Result<span<const std::byte>>)> callback) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: EncodeVendorCommand");
  (void)parameters;
  (void)callback;
}

}  // namespace pw::bluetooth
