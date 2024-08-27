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
#include "pw_bluetooth_sapphire/internal/host/hci/connection.h"

namespace bt::hci {
class ScoConnection : public Connection, public WeakSelf<ScoConnection> {
 public:
  ScoConnection(hci_spec::ConnectionHandle handle,
                const DeviceAddress& local_address,
                const DeviceAddress& peer_address,
                const hci::Transport::WeakPtrType& hci);

 private:
  // This method must be static since it may be invoked after the connection
  // associated with it is destroyed.
  static void OnDisconnectionComplete(hci_spec::ConnectionHandle handle,
                                      const hci::Transport::WeakPtrType& hci);
};

}  // namespace bt::hci
