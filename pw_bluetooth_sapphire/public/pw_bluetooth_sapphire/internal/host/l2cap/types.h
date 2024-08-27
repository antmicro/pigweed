// Copyright 2023 The Pigweed Authors
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
#include <lib/fit/function.h>
#include <pw_string/to_string.h>

#include <chrono>
#include <optional>
#include <variant>

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/le_connection_parameters.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/sm/error.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bt::l2cap {

class Channel;
// Callback invoked when a channel has been created or when an error occurs
// during channel creation (in which case the channel will be nullptr).
using ChannelCallback = fit::function<void(WeakSelf<Channel>::WeakPtrType)>;

// Callback invoked when a logical link should be closed due to an error.
using LinkErrorCallback = fit::closure;

// Callback called to notify LE preferred connection parameters during the "LE
// Connection Parameter Update" procedure.
using LEConnectionParameterUpdateCallback =
    fit::function<void(const hci_spec::LEPreferredConnectionParameters&)>;

// Callback called when response received to LE signaling channel Connection
// Parameters Update Request. |accepted| indicates whether the parameters were
// accepted by the peer.
using ConnectionParameterUpdateRequestCallback =
    fit::function<void(bool accepted)>;

// Callback used to deliver LE fixed channels that are created when a LE link is
// registered with L2CAP.
using LEFixedChannelsCallback = fit::function<void(
    WeakSelf<Channel>::WeakPtrType att, WeakSelf<Channel>::WeakPtrType smp)>;

// Callback used to request a security upgrade for an active logical link.
// Invokes its |callback| argument with the result of the operation.
using SecurityUpgradeCallback =
    fit::function<void(hci_spec::ConnectionHandle ll_handle,
                       sm::SecurityLevel level,
                       sm::ResultFunction<> callback)>;

// A variant that can hold any channel mode. While the
// `CreditBasedFlowControlMode` codes do not intersect with the
// `RetransmissionAndFlowControlMode` retransmission and flow control codes,
// that is not a property that is guaranteed to hold for all future versions,
// and the request-based codes would not be valid in a configuration packet,
// unlike the "classic" modes. This type allows us to treat them as separate
// namespaces and access each through the variant. Note: Equality comparison
// with enum values for either enum are supported.
using AnyChannelMode =
    std::variant<RetransmissionAndFlowControlMode, CreditBasedFlowControlMode>;

bool operator==(const AnyChannelMode& any,
                RetransmissionAndFlowControlMode mode);
bool operator==(RetransmissionAndFlowControlMode mode,
                const AnyChannelMode& any);
bool operator==(const AnyChannelMode& any, CreditBasedFlowControlMode mode);
bool operator==(CreditBasedFlowControlMode mode, const AnyChannelMode& any);
bool operator!=(const AnyChannelMode& any,
                RetransmissionAndFlowControlMode mode);
bool operator!=(RetransmissionAndFlowControlMode mode,
                const AnyChannelMode& any);
bool operator!=(const AnyChannelMode& any, CreditBasedFlowControlMode mode);
bool operator!=(CreditBasedFlowControlMode mode, const AnyChannelMode& any);
std::string AnyChannelModeToString(const AnyChannelMode& mode);
pw::StatusWithSize AnyChannelModeToPwString(const AnyChannelMode& mode,
                                            pw::span<char> span);

// Channel configuration parameters specified by higher layers.
struct ChannelParameters {
  std::optional<AnyChannelMode> mode;
  // MTU
  std::optional<uint16_t> max_rx_sdu_size;

  std::optional<pw::chrono::SystemClock::duration> flush_timeout;

  bool operator==(const ChannelParameters& rhs) const {
    return mode == rhs.mode && max_rx_sdu_size == rhs.max_rx_sdu_size &&
           flush_timeout == rhs.flush_timeout;
  }

  std::string ToString() const {
    auto mode_string = mode.has_value() ? AnyChannelModeToString(*mode)
                                        : std::string("nullopt");
    auto sdu_string =
        max_rx_sdu_size.has_value()
            ? bt_lib_cpp_string::StringPrintf("%hu", *max_rx_sdu_size)
            : std::string("nullopt");
    auto flush_timeout_string =
        flush_timeout
            ? bt_lib_cpp_string::StringPrintf(
                  "%lldms",
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      *flush_timeout)
                      .count())
            : std::string("nullopt");
    return bt_lib_cpp_string::StringPrintf(
        "ChannelParameters{mode: %s, max_rx_sdu_size: %s, flush_timeout: %s}",
        mode_string.c_str(),
        sdu_string.c_str(),
        flush_timeout_string.c_str());
  }
};

// Convenience struct for passsing around information about an opened channel.
// For example, this is useful when describing the L2CAP channel underlying a
// zx::socket.
struct ChannelInfo {
  ChannelInfo() = default;

  static ChannelInfo MakeBasicMode(
      uint16_t max_rx_sdu_size,
      uint16_t max_tx_sdu_size,
      std::optional<Psm> psm = std::nullopt,
      std::optional<pw::chrono::SystemClock::duration> flush_timeout =
          std::nullopt) {
    return ChannelInfo(RetransmissionAndFlowControlMode::kBasic,
                       max_rx_sdu_size,
                       max_tx_sdu_size,
                       0,
                       0,
                       0,
                       psm,
                       flush_timeout);
  }

  static ChannelInfo MakeEnhancedRetransmissionMode(
      uint16_t max_rx_sdu_size,
      uint16_t max_tx_sdu_size,
      uint8_t n_frames_in_tx_window,
      uint8_t max_transmissions,
      uint16_t max_tx_pdu_payload_size,
      std::optional<Psm> psm = std::nullopt,
      std::optional<pw::chrono::SystemClock::duration> flush_timeout =
          std::nullopt) {
    return ChannelInfo(
        RetransmissionAndFlowControlMode::kEnhancedRetransmission,
        max_rx_sdu_size,
        max_tx_sdu_size,
        n_frames_in_tx_window,
        max_transmissions,
        max_tx_pdu_payload_size,
        psm,
        flush_timeout);
  }

  ChannelInfo(AnyChannelMode mode,
              uint16_t max_rx_sdu_size,
              uint16_t max_tx_sdu_size,
              uint8_t n_frames_in_tx_window,
              uint8_t max_transmissions,
              uint16_t max_tx_pdu_payload_size,
              std::optional<Psm> psm = std::nullopt,
              std::optional<pw::chrono::SystemClock::duration> flush_timeout =
                  std::nullopt)
      : mode(mode),
        max_rx_sdu_size(max_rx_sdu_size),
        max_tx_sdu_size(max_tx_sdu_size),
        n_frames_in_tx_window(n_frames_in_tx_window),
        max_transmissions(max_transmissions),
        max_tx_pdu_payload_size(max_tx_pdu_payload_size),
        psm(psm),
        flush_timeout(flush_timeout) {}

  AnyChannelMode mode;
  uint16_t max_rx_sdu_size;
  uint16_t max_tx_sdu_size;

  // For Enhanced Retransmission Mode only. See Core Spec v5.0 Vol 3, Part A,
  // Sec 5.4 for details on each field. Values are not meaningful if mode =
  // RetransmissionAndFlowControlMode::kBasic.
  uint8_t n_frames_in_tx_window;
  uint8_t max_transmissions;
  uint16_t max_tx_pdu_payload_size;

  // PSM of the service the channel is used for.
  std::optional<Psm> psm;

  // If present, the channel's packets will be marked as flushable. The value
  // will be used to configure the link's automatic flush timeout.
  std::optional<pw::chrono::SystemClock::duration> flush_timeout;
};

// Data stored for services registered by higher layers.
template <typename ChannelCallbackT>
struct ServiceInfo {
  ServiceInfo(ChannelParameters params, ChannelCallbackT cb)
      : channel_params(params), channel_cb(std::move(cb)) {}
  ServiceInfo(ServiceInfo&&) = default;
  ServiceInfo& operator=(ServiceInfo&&) = default;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ServiceInfo);

  // Preferred channel configuration parameters for new channels for this
  // service.
  ChannelParameters channel_params;

  // Callback for forwarding new channels to locally-hosted service.
  ChannelCallbackT channel_cb;
};

}  // namespace bt::l2cap

namespace pw {
// AnyChannelMode supports pw::ToString.
template <>
StatusWithSize ToString(const bt::l2cap::AnyChannelMode& mode,
                        span<char> buffer);
}  // namespace pw
