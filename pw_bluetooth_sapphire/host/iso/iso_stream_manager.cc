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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream_manager.h"

namespace bt::iso {

IsoStreamManager::IsoStreamManager(hci_spec::ConnectionHandle handle,
                                   hci::CommandChannel::WeakPtrType cmd_channel)
    : acl_handle_(handle), cmd_(cmd_channel), weak_self_(this) {
  if (!cmd_.is_alive()) {
    return;
  }
  auto self = GetWeakPtr();
  cis_request_handler_ = cmd_->AddLEMetaEventHandler(
      hci_spec::kLECISRequestSubeventCode,
      [self = std::move(self)](const hci::EmbossEventPacket& event) {
        if (!self.is_alive()) {
          return hci::CommandChannel::EventCallbackResult::kRemove;
        }
        self->OnCisRequest(event);
        return hci::CommandChannel::EventCallbackResult::kContinue;
      });
}

IsoStreamManager::~IsoStreamManager() {
  if (cmd_.is_alive()) {
    cmd_->RemoveEventHandler(cis_request_handler_);
  }
}

AcceptCisStatus IsoStreamManager::AcceptCis(CigCisIdentifier id,
                                            CisEstablishedCallback cb) {
  bt_log(INFO,
         "iso",
         "IsoStreamManager: preparing to accept incoming connection (CIG: %u, "
         "CIS: %u)",
         id.cig_id(),
         id.cis_id());

  if (accept_handlers_.count(id) != 0) {
    return AcceptCisStatus::kAlreadyExists;
  }

  if (streams_.count(id) != 0) {
    return AcceptCisStatus::kAlreadyExists;
  }

  accept_handlers_[id] = std::move(cb);
  return AcceptCisStatus::kSuccess;
}

void IsoStreamManager::OnCisRequest(const hci::EmbossEventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);

  auto event_view =
      event.view<pw::bluetooth::emboss::LECISRequestSubeventView>();
  BT_ASSERT(event_view.le_meta_event().subevent_code().Read() ==
            hci_spec::kLECISRequestSubeventCode);

  hci_spec::ConnectionHandle request_handle =
      event_view.acl_connection_handle().Read();
  uint8_t cig_id = event_view.cig_id().Read();
  uint8_t cis_id = event_view.cis_id().Read();
  CigCisIdentifier id(cig_id, cis_id);

  bt_log(INFO,
         "iso",
         "CIS request received for handle 0x%x (CIG: %u, CIS: %u)",
         request_handle,
         cig_id,
         cis_id);

  // Ignore any requests that are not intended for this connection.
  if (request_handle != acl_handle_) {
    bt_log(DEBUG,
           "iso",
           "ignoring incoming stream request for handle 0x%x (ours: 0x%x)",
           request_handle,
           acl_handle_);
    return;
  }

  // If we are not waiting on this request, reject it
  if (accept_handlers_.count(id) == 0) {
    bt_log(INFO, "iso", "Rejecting incoming request");
    RejectCisRequest(event_view);
    return;
  }

  bt_log(INFO, "iso", "Accepting incoming request");

  // We should not already have an established stream using this same CIG/CIS
  // permutation.
  BT_ASSERT_MSG(
      streams_.count(id) == 0, "(cig = %u, cis = %u)", cig_id, cis_id);
  CisEstablishedCallback cb = std::move(accept_handlers_[id]);
  accept_handlers_.erase(id);
  AcceptCisRequest(event_view, std::move(cb));
}

void IsoStreamManager::AcceptCisRequest(
    const pw::bluetooth::emboss::LECISRequestSubeventView& event_view,
    CisEstablishedCallback cb) {
  uint8_t cig_id = event_view.cig_id().Read();
  uint8_t cis_id = event_view.cis_id().Read();
  CigCisIdentifier id(cig_id, cis_id);

  hci_spec::ConnectionHandle cis_handle =
      event_view.cis_connection_handle().Read();

  BT_ASSERT(streams_.count(id) == 0);

  auto self = GetWeakPtr();
  auto on_closed_cb = [self, id]() {
    if (self.is_alive()) {
      self->streams_.erase(id);
    }
  };

  streams_[id] = IsoStream::Create(
      cig_id, cis_id, cis_handle, std::move(cb), cmd_, on_closed_cb);

  auto command = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LEAcceptCISRequestCommandWriter>(
      hci_spec::kLEAcceptCISRequest);
  auto cmd_view = command.view_t();
  cmd_view.connection_handle().Write(cis_handle);

  auto cmd_complete_cb = [cis_handle, id, self](auto cmd_id,
                                                const hci::EventPacket& event) {
    bt_log(INFO, "iso", "LE_Accept_CIS_Request command response received");
    if (hci_is_error(event,
                     WARN,
                     "bt-iso",
                     "accept CIS request failed for handle %#x",
                     cis_handle)) {
      if (self.is_alive()) {
        self->streams_.erase(id);
      }
    }
  };

  cmd_->SendCommand(std::move(command), cmd_complete_cb);
}

void IsoStreamManager::RejectCisRequest(
    const pw::bluetooth::emboss::LECISRequestSubeventView& event_view) {
  hci_spec::ConnectionHandle cis_handle =
      event_view.cis_connection_handle().Read();

  auto command = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LERejectCISRequestCommandWriter>(
      hci_spec::kLERejectCISRequest);
  auto cmd_view = command.view_t();
  cmd_view.connection_handle().Write(cis_handle);
  cmd_view.reason().Write(pw::bluetooth::emboss::StatusCode::UNSPECIFIED_ERROR);

  cmd_->SendCommand(std::move(command),
                    [cis_handle](auto id, const hci::EventPacket& event) {
                      bt_log(INFO, "iso", "LE_Reject_CIS_Request command sent");
                      hci_is_error(event,
                                   ERROR,
                                   "bt-iso",
                                   "reject CIS request failed for handle %#x",
                                   cis_handle);
                    });
}

}  // namespace bt::iso
