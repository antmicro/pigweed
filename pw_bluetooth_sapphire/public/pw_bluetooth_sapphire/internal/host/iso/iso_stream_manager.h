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

#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/iso/iso_common.h"
#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"

namespace bt::iso {

// Responsible for owning and managing IsoStream objects associated with a
// single LE connection.
// When operating as a Central, establishes an outgoing streams. When operating
// as a Peripheral, processes incoming stream requests .
class IsoStreamManager final {
 public:
  explicit IsoStreamManager(hci_spec::ConnectionHandle handle,
                            hci::CommandChannel::WeakPtrType cmd_channel);
  ~IsoStreamManager();

  // Start waiting on an incoming request to create an Isochronous channel for
  // the specified CIG/CIS |id|. If we are already waiting on |id|, or if a
  // stream has already been established with the given |id|, returns
  // kAlreadyExists. |cb| will be invoked when we receive an incoming ISO
  // channel request with a matching CIG/CIS |id|, and will indicate the status
  // of establishing a channel and on success the associated channel parameters.
  [[nodiscard]] AcceptCisStatus AcceptCis(CigCisIdentifier id,
                                          CisEstablishedCallback cb);

  // Indicates if we are currently waiting on a connection for the specified
  // CIG/CIS combination
  bool HandlerRegistered(const CigCisIdentifier& id) const {
    return accept_handlers_.count(id) != 0;
  }

  using WeakPtrType = WeakSelf<IsoStreamManager>::WeakPtrType;
  IsoStreamManager::WeakPtrType GetWeakPtr() { return weak_self_.GetWeakPtr(); }

 private:
  // Process an incoming CIS request. Currently rejects all requests.
  void OnCisRequest(const hci::EmbossEventPacket& event);

  void AcceptCisRequest(
      const pw::bluetooth::emboss::LECISRequestSubeventView& event_view,
      CisEstablishedCallback cb);

  // Send a rejection in response to an incoming CIS request.
  void RejectCisRequest(
      const pw::bluetooth::emboss::LECISRequestSubeventView& event_view);

  hci_spec::ConnectionHandle acl_handle_;

  // LE event handler for incoming CIS requests
  hci::CommandChannel::EventHandlerId cis_request_handler_;

  hci::CommandChannel::WeakPtrType cmd_;

  // The streams that we are currently waiting on, and the associated callback
  // when the connection is resolved (either accepted and established, or failed
  // to establish).
  std::unordered_map<CigCisIdentifier, CisEstablishedCallback> accept_handlers_;

  // All of the allocated streams.
  std::unordered_map<CigCisIdentifier, std::unique_ptr<IsoStream>> streams_;

  WeakSelf<IsoStreamManager> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(IsoStreamManager);
};

}  // namespace bt::iso
