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
#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_connection.h"

namespace bt::hci::testing {

class FakeLowEnergyConnection final : public LowEnergyConnection {
 public:
  FakeLowEnergyConnection(hci_spec::ConnectionHandle handle,
                          const DeviceAddress& local_address,
                          const DeviceAddress& peer_address,
                          pw::bluetooth::emboss::ConnectionRole role,
                          const Transport::WeakPtrType& hci);

  // Triggers the encryption change callback.
  void TriggerEncryptionChangeCallback(hci::Result<bool> result);

  void TriggerPeerDisconnectCallback() {
    peer_disconnect_callback()(
        *this,
        pw::bluetooth::emboss::StatusCode::REMOTE_USER_TERMINATED_CONNECTION);
  }

  // LowEnergyConnection overrides:
  void Disconnect(pw::bluetooth::emboss::StatusCode reason) override {}
  bool StartEncryption() override;

  // Number of times StartEncryption() was called.
  int start_encryption_count() const { return start_encryption_count_; }

 private:
  int start_encryption_count_ = 0;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeLowEnergyConnection);
};

}  // namespace bt::hci::testing
