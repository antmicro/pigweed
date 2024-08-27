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

#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_connection.h"

#include <pw_bytes/endian.h>

#include <cinttypes>

#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

namespace bt::hci {

LowEnergyConnection::LowEnergyConnection(
    hci_spec::ConnectionHandle handle,
    const DeviceAddress& local_address,
    const DeviceAddress& peer_address,
    const hci_spec::LEConnectionParameters& params,
    pw::bluetooth::emboss::ConnectionRole role,
    const Transport::WeakPtrType& hci)
    : AclConnection(handle, local_address, peer_address, role, hci),
      WeakSelf(this),
      parameters_(params) {
  BT_ASSERT(local_address.type() != DeviceAddress::Type::kBREDR);
  BT_ASSERT(peer_address.type() != DeviceAddress::Type::kBREDR);
  BT_ASSERT(hci.is_alive());

  le_ltk_request_id_ = hci->command_channel()->AddLEMetaEventHandler(
      hci_spec::kLELongTermKeyRequestSubeventCode,
      fit::bind_member<&LowEnergyConnection::OnLELongTermKeyRequestEvent>(
          this));
}

LowEnergyConnection::~LowEnergyConnection() {
  // Unregister HCI event handlers.
  if (hci().is_alive()) {
    hci()->command_channel()->RemoveEventHandler(le_ltk_request_id_);
  }
}

bool LowEnergyConnection::StartEncryption() {
  if (state() != Connection::State::kConnected) {
    bt_log(DEBUG, "hci", "connection closed; cannot start encryption");
    return false;
  }
  if (role() != pw::bluetooth::emboss::ConnectionRole::CENTRAL) {
    bt_log(DEBUG, "hci", "only the central can start encryption");
    return false;
  }
  if (!ltk().has_value()) {
    bt_log(DEBUG, "hci", "connection has no LTK; cannot start encryption");
    return false;
  }

  auto cmd = EmbossCommandPacket::New<
      pw::bluetooth::emboss::LEEnableEncryptionCommandWriter>(
      hci_spec::kLEStartEncryption);
  auto params = cmd.view_t();
  params.connection_handle().Write(handle());
  params.random_number().Write(ltk()->rand());
  params.encrypted_diversifier().Write(ltk()->ediv());
  params.long_term_key().CopyFrom(
      pw::bluetooth::emboss::LinkKeyView(&ltk()->value()));

  auto event_cb = [self = GetWeakPtr(), handle = handle()](
                      auto id, const EventPacket& event) {
    if (!self.is_alive()) {
      return;
    }

    Result<> result = event.ToResult();
    if (bt_is_error(result,
                    ERROR,
                    "hci-le",
                    "could not set encryption on link %#.04x",
                    handle)) {
      if (self->encryption_change_callback()) {
        self->encryption_change_callback()(result.take_error());
      }
      return;
    }
    bt_log(DEBUG, "hci-le", "requested encryption start on %#.04x", handle);
  };
  if (!hci().is_alive()) {
    return false;
  }
  return hci()->command_channel()->SendCommand(
      std::move(cmd), std::move(event_cb), hci_spec::kCommandStatusEventCode);
}

void LowEnergyConnection::HandleEncryptionStatus(Result<bool> result,
                                                 bool /*key_refreshed*/) {
  // "On an authentication failure, the connection shall be automatically
  // disconnected by the Link Layer." (HCI_LE_Start_Encryption, Vol 2, Part E,
  // 7.8.24). We make sure of this by telling the controller to disconnect.
  if (result.is_error()) {
    Disconnect(pw::bluetooth::emboss::StatusCode::AUTHENTICATION_FAILURE);
  }

  if (!encryption_change_callback()) {
    bt_log(DEBUG,
           "hci",
           "%#.4x: no encryption status callback assigned",
           handle());
    return;
  }
  encryption_change_callback()(result);
}

CommandChannel::EventCallbackResult
LowEnergyConnection::OnLELongTermKeyRequestEvent(const EventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);
  BT_ASSERT(event.params<hci_spec::LEMetaEventParams>().subevent_code ==
            hci_spec::kLELongTermKeyRequestSubeventCode);

  auto* params =
      event.subevent_params<hci_spec::LELongTermKeyRequestSubeventParams>();
  if (!params) {
    bt_log(WARN, "hci", "malformed LE LTK request event");
    return CommandChannel::EventCallbackResult::kContinue;
  }

  hci_spec::ConnectionHandle handle = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, params->connection_handle);

  // Silently ignore the event as it isn't meant for this connection.
  if (handle != this->handle()) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  uint64_t rand =
      pw::bytes::ConvertOrderFrom(cpp20::endian::little, params->random_number);
  uint16_t ediv = pw::bytes::ConvertOrderFrom(cpp20::endian::little,
                                              params->encrypted_diversifier);

  bt_log(DEBUG,
         "hci",
         "LE LTK request - ediv: %#.4x, rand: %#.16" PRIx64,
         ediv,
         rand);

  if (!hci().is_alive()) {
    return CommandChannel::EventCallbackResult::kRemove;
  }

  auto status_cb = [](auto id, const EventPacket& event) {
    hci_is_error(event, TRACE, "hci-le", "failed to reply to LTK request");
  };

  if (ltk() && ltk()->rand() == rand && ltk()->ediv() == ediv) {
    auto cmd = EmbossCommandPacket::New<
        pw::bluetooth::emboss::LELongTermKeyRequestReplyCommandWriter>(
        hci_spec::kLELongTermKeyRequestReply);
    auto view = cmd.view_t();
    view.connection_handle().Write(handle);
    view.long_term_key().CopyFrom(
        pw::bluetooth::emboss::LinkKeyView(&ltk()->value()));
    hci()->command_channel()->SendCommand(cmd, std::move(status_cb));
  } else {
    bt_log(DEBUG, "hci-le", "LTK request rejected");

    auto cmd = EmbossCommandPacket::New<
        pw::bluetooth::emboss::LELongTermKeyRequestNegativeReplyCommandWriter>(
        hci_spec::kLELongTermKeyRequestNegativeReply);
    auto view = cmd.view_t();
    view.connection_handle().Write(handle);
    hci()->command_channel()->SendCommand(cmd, std::move(status_cb));
  }

  return CommandChannel::EventCallbackResult::kContinue;
}

}  // namespace bt::hci
