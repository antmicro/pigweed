// Copyright 2024 The Pigweed Authors
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

#include <memory>
#include <optional>

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gap/gap.h"
#include "pw_bluetooth_sapphire/internal/host/gap/legacy_pairing_state.h"
#include "pw_bluetooth_sapphire/internal/host/gap/pairing_delegate.h"
#include "pw_bluetooth_sapphire/internal/host/gap/secure_simple_pairing_state.h"
#include "pw_bluetooth_sapphire/internal/host/gap/types.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci/bredr_connection.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"
#include "pw_bluetooth_sapphire/internal/host/transport/error.h"

namespace bt::gap {

// In order to support BR/EDR Legacy Pairing, each BrEdrConnection must manage
// either a LegacyPairingState or SecureSimplePairingState object since the two
// pairing processes differ. The PairingStateManager class's purpose is to
// abstract this management logic out of BrEdrConnection and
// BrEdrConnectionManager.
//
// PairingStateManager exists for each BrEdrConnection and routes the events
// received by the connection to either SecureSimplePairingState or
// LegacyPairingState.
//
// Sometimes we can receive pairing events before the L2CAP connection is
// complete (i.e. before interrogation can occur/is complete). In this case, we
// don’t know if the peer supports SSP, so we don’t know whether to use SSP or
// LP. PairingStateManager defaults to instantiating LegacyPairingState since LP
// can happen before L2CAP channel is open and handles the logic to switch to
// SSP if needed (i.e. on an HCI_IO_Capability* event).
//
// Only one of the two pairing states will ever be instantiated at a time.
class PairingStateManager final {
 public:
  // Used to report the status of each pairing procedure on this link. |status|
  // will contain HostError::kNotSupported if the pairing procedure does not
  // proceed in the order of events expected.
  using StatusCallback =
      fit::function<void(hci_spec::ConnectionHandle, hci::Result<>)>;

  // Constructs a PairingStateManager for the ACL connection |link| to
  // |peer_id|. |outgoing_connection| should be true if this device connected,
  // and false if it was an incoming connection. This object will receive
  // "encryption change" callbacks associate with |peer_id|. Successful pairing
  // is reported through |status_cb| after encryption is enabled. When errors
  // occur, this object will be put in a "failed" state and the owner shall
  // disconnect the link and destroy its PairingStateManager.  When destroyed,
  // status callbacks for any waiting pairings are called. |status_cb| is not
  // called on destruction.
  //
  // |auth_cb| will be called to indicate that the caller should send an
  // Authentication Request for this peer.
  //
  // |link| must be valid for the lifetime of this object.
  //
  // If |legacy_pairing_state| is non-null, this means we were responding to
  // a Legacy Pairing request before the ACL connection between the two devices
  // was complete. |legacy_pairing_state| is transferred to the
  // PairingStateManager.
  PairingStateManager(Peer::WeakPtrType peer,
                      WeakPtr<hci::BrEdrConnection> link,
                      std::unique_ptr<LegacyPairingState> legacy_pairing_state,
                      bool outgoing_connection,
                      fit::closure auth_cb,
                      StatusCallback status_cb);
  PairingStateManager(PairingStateManager&&) = default;
  PairingStateManager& operator=(PairingStateManager&&) = default;

  // Set a handler for user-interactive authentication challenges. If not set or
  // set to nullptr, all pairing requests will be rejected, but this does not
  // cause a fatal error and should not result in link disconnection.
  //
  // If the delegate indicates passkey display capabilities, then it will always
  // be asked to confirm pairing, even when Core Spec v5.0, Vol 3, Part C,
  // Section 5.2.2.6 indicates "automatic confirmation."
  void SetPairingDelegate(const PairingDelegate::WeakPtrType& pairing_delegate) {
    if (secure_simple_pairing_state_) {
      secure_simple_pairing_state_->SetPairingDelegate(pairing_delegate);
    }
  }

  // Starts pairing against the peer, if pairing is not already in progress.
  // If not, this device becomes the pairing initiator. If pairing is in
  // progress, the request will be queued until the current pairing completes or
  // an additional pairing that upgrades the link key succeeds or fails.
  //
  // If no PairingDelegate is available, |status_cb| is immediately called with
  // HostError::kNotReady, but the PairingStateManager status callback (provided
  // in the ctor) is not called.
  //
  // When pairing completes or errors out, the |status_cb| of each call to this
  // function will be invoked with the result.
  void InitiatePairing(BrEdrSecurityRequirements security_requirements,
                       StatusCallback status_cb);

  // Event handlers. Caller must ensure that the event is addressed to the link
  // for this SecureSimplePairingState.

  // Returns value for IO Capability Request Reply, else std::nullopt for IO
  // Capability Negative Reply.
  //
  // TODO(fxbug.dev/42138242): Indicate presence of out-of-band (OOB) data.
  [[nodiscard]] std::optional<pw::bluetooth::emboss::IoCapability>
  OnIoCapabilityRequest();

  // Caller is not expected to send a response.
  void OnIoCapabilityResponse(pw::bluetooth::emboss::IoCapability peer_iocap);

  // |cb| is called with: true to send User Confirmation Request Reply, else
  // for to send User Confirmation Request Negative Reply. It may be called from
  // a different thread than the one that called OnUserConfirmationRequest.
  using UserConfirmationCallback = fit::callback<void(bool confirm)>;
  void OnUserConfirmationRequest(uint32_t numeric_value,
                                 UserConfirmationCallback cb);

  // |cb| is called with: passkey value to send User Passkey Request Reply, else
  // std::nullopt to send User Passkey Request Negative Reply. It may not be
  // called from the same thread that called OnUserPasskeyRequest.
  using UserPasskeyCallback =
      fit::callback<void(std::optional<uint32_t> passkey)>;
  void OnUserPasskeyRequest(UserPasskeyCallback cb);

  // Caller is not expected to send a response.
  void OnUserPasskeyNotification(uint32_t numeric_value);

  // Caller is not expected to send a response.
  void OnSimplePairingComplete(pw::bluetooth::emboss::StatusCode status_code);

  // Caller should send the returned link key in a Link Key Request Reply (or
  // Link Key Request Negative Reply if the returned value is null).
  [[nodiscard]] std::optional<hci_spec::LinkKey> OnLinkKeyRequest();

  // Caller is not expected to send a response.
  void OnLinkKeyNotification(const UInt128& link_key,
                             hci_spec::LinkKeyType key_type,
                             bool local_secure_connections_supported = false);

  // Caller is not expected to send a response.
  void OnAuthenticationComplete(pw::bluetooth::emboss::StatusCode status_code);

  // Handler for hci::Connection::set_encryption_change_callback.
  void OnEncryptionChange(hci::Result<bool> result);

  sm::SecurityProperties& security_properties() {
    return secure_simple_pairing_state_->security_properties();
  }

  // Sets the BR/EDR Security Mode of the pairing state - see enum definition
  // for details of each mode. If a security upgrade is in-progress, only takes
  // effect on the next security upgrade.
  void set_security_mode(gap::BrEdrSecurityMode mode) {
    secure_simple_pairing_state_->set_security_mode(mode);
  }

  // Attach pairing state inspect node named |name| as a child of |parent|.
  void AttachInspect(inspect::Node& parent, std::string name);

 private:
  std::unique_ptr<SecureSimplePairingState> secure_simple_pairing_state_;
  std::unique_ptr<LegacyPairingState> legacy_pairing_state_;

  Peer::WeakPtrType peer_;

  // The BR/EDR link whose pairing is being driven by this object.
  WeakPtr<hci::BrEdrConnection> link_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingStateManager);
};

}  // namespace bt::gap
