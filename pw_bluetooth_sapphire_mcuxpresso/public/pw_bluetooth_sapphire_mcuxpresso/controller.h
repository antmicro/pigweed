#pragma once

#include "pw_bluetooth/controller.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_packet.h"
#include "pw_bluetooth_sapphire/internal/host/transport/control_packets.h"
#include "pw_containers/inline_queue.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/thread_notification.h"

PW_EXTERN_C_START
#include "fsl_adapter_uart.h"
#include "hci_transport.h"
PW_EXTERN_C_END

namespace pw::bluetooth {

class Mimxrt595Controller final : public pw::bluetooth::Controller {
public:
  /// Sets a function that will be called with HCI event packets received from
  /// the controller. This should  be called before @cpp_func{pw::bluetooth::Mimxrt595Controller::Initialize} or else incoming
  /// packets will be dropped. The lifetime of data passed to `func` is only
  /// guaranteed for the lifetime of the function call.
  ///
  /// @param[in] func The function to call with received HCI event packets.
  void SetEventFunction(DataFunction func) override;

  /// Sets a function that will be called with ACL data packets received from the
  /// controller. This should be called before @cpp_func{pw::bluetooth::Mimxrt595Controller::Initialize} or else incoming
  /// packets will be dropped. The lifetime of data passed to `func` is only
  /// guaranteed for the lifetime of the function call.
  ///
  /// @param[in] func The function to call with received HCI ACL packets.
  void SetReceiveAclFunction(DataFunction func) override;

  /// Sets a function that will be called with SCO packets received from the
  /// controller. On Classic and Dual Mode stacks, this should be called before
  /// @cpp_func{pw::bluetooth::Mimxrt595Controller::Initialize} or else incoming packets will be dropped. The lifetime of data
  /// passed to `func` is only guaranteed for the lifetime of the function call.
  ///
  /// @param[in] func The function to call with received HCI SCO packets.
  void SetReceiveScoFunction(DataFunction func) override;

  /// Sets a function that will be called with ISO packets received from the
  /// controller. This should be called before an ISO data path is setup or else
  /// incoming packets may be dropped. The lifetime of data passed to `func` is
  /// only guaranteed for the lifetime of the function call.
  ///
  /// @param[in] func The function to call with received HCI ISO packets.
  void SetReceiveIsoFunction(DataFunction /*func*/) {}

  /// Initializes the controller interface and starts processing packets.
  /// `complete_callback` will be called with the result of initialization.
  /// `error_callback` will be called for fatal errors that occur after
  /// initialization. After a fatal error, this object is invalid. `Close` should
  /// be called to ensure a safe clean up.
  ///
  /// @param[in] complete_callback The callback to call when initialization is complete.
  /// @param[in] error_callback The callback to call for fatal errors after initialization.
  void Initialize(Callback<void(Status)> complete_callback,
                  Callback<void(Status)> error_callback) override;

  /// Closes the controller interface, resetting all state. `callback` will be
  /// called when closure is complete. After this method is called, this object
  /// should be considered invalid and no other methods should be called
  /// (including `Initialize`).
  /// `callback` will be called with:
  /// OK - the controller interface was successfully closed, or is already closed
  /// INTERNAL - the controller interface could not be closed
  /// @param[in] callback The callback to call when closure is complete.
  void Close(Callback<void(Status)> callback) override;

  /// Sends an HCI command packet to the controller.
  /// @param[in] command The command packet to send.
  void SendCommand(span<const std::byte> command) override;

  /// Sends an ACL data packet to the controller.
  /// @param[in] data The ACL data packet to send.
  void SendAclData(span<const std::byte> data) override;

  /// Sends a SCO data packet to the controller.
  /// @param[in] data The SCO data packet to send.
  void SendScoData(span<const std::byte> data) override;

  /// Sends an ISO data packet to the controller.
  /// @param[in] data The ISO data packet to send.
  void SendIsoData(span<const std::byte> /*data*/) {}

  /// Configure the HCI for a SCO connection with the indicated parameters.
  /// @cpp_func{pw::bluetooth::Mimxrt595Controller::SetReceiveScoFunction} must be called before calling this method.
  /// `callback` will be called with:
  /// OK - success, packets can be sent/received.
  /// UNIMPLEMENTED - the implementation/controller does not support SCO over HCI
  /// ALREADY_EXISTS - a SCO connection is already configured
  /// INTERNAL - an internal error occurred
  /// @param[in] coding_format The coding format for the SCO connection.
  /// @param[in] encoding The encoding for the SCO connection.
  /// @param[in] sample_rate The sample rate for the SCO connection.
  /// @param[in] callback The callback to call with the result of the configuration request.
  void ConfigureSco(ScoCodingFormat coding_format,
                    ScoEncoding encoding,
                    ScoSampleRate sample_rate,
                    Callback<void(Status)> callback) override;

  /// Releases the resources held by an active SCO connection. This should be
  /// called when a SCO connection is closed. @cpp_func{pw::bluetooth::Mimxrt595Controller::ConfigureSco} must be called
  /// before calling this method.
  /// `callback` will be called with:
  /// OK - success, the SCO configuration was reset.
  /// UNIMPLEMENTED - the implementation/controller does not support SCO over HCI
  /// INTERNAL - an internal error occurred
  /// @param[in] callback The callback to call with the result of the reset request.
  void ResetSco(Callback<void(Status)> callback) override;

  /// Calls `callback` with a bitmask of features supported by the controller.
  /// @param[in] callback The callback to call with the bitmask of supported features.
  void GetFeatures(Callback<void(FeaturesBits)> callback) override;

  /// Encodes the vendor command indicated by `parameters`.
  /// `callback` will be called with the result of the encoding request.
  /// The lifetime of data passed to `callback` is only guaranteed for the
  /// lifetime of the function call.
  /// @param[in] parameters The parameters for the vendor command to encode.
  /// @param[in] callback The callback to call with the result of the encoding request.
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

  typedef struct _hci_uart_meta_data_ {
    std::byte* data;
    size_t dataSize;
  } hci_uart_meta_data;

  static void hci_uart_transmit_cb(hal_uart_handle_t handle,
                                   hal_uart_status_t status,
                                   void* userData);
  void hci_uart_write_data(std::byte* buf, uint16_t length);
  void hci_uart_send_data(PacketType type, unsigned char* buf, uint16_t length);
  pw::Status hci_uart_init();
  Callback<void(Status)> error_callback_;
  DataFunction event_function_;
  DataFunction acl_function_;

  hci_uart_meta_data hci_uart_rx;
  pw::Vector<std::byte, 1024> hci_uart_rx_buf;
  pw::Vector<std::byte, 2048> hci_uart_tx_buf;
  uint16_t hci_uart_rx_bytes_ptr;
  pw::sync::ThreadNotification hci_uart_cond;
  HT_PARSE ht;
  UART_HANDLE_DEFINE(hci_uart_handle);
  pw::sync::InterruptSpinLock hci_uart_tx_queue_mutex;
  pw::InlineQueue<std::pair<PacketType, pw::Vector<std::byte, 2048>>, 16>
      hci_uart_tx_queue;
};

}  // namespace pw::bluetooth
