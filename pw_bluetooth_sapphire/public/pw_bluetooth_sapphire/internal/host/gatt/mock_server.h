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

#include "pw_bluetooth_sapphire/internal/host/gatt/gatt_defs.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/server.h"

namespace bt::gatt::testing {
using UpdateHandler = fit::function<void(IdType service_id,
                                         IdType chrc_id,
                                         BufferView value,
                                         IndicationCallback indicate_cb)>;

// A mock implementation of a gatt::Server object. Can be used to mock outbound
// notifications/ indications without a production att::Bearer in tests.
class MockServer : public Server {
 public:
  MockServer(PeerId peer_id, LocalServiceManager::WeakPtrType local_services);

  void set_update_handler(UpdateHandler handler) {
    update_handler_ = std::move(handler);
  }

  using WeakPtrType = WeakSelf<MockServer>::WeakPtrType;
  MockServer::WeakPtrType AsMockWeakPtr() { return weak_self_.GetWeakPtr(); }

  bool was_shut_down() const { return was_shut_down_; }

 private:
  // Server overrides:
  void SendUpdate(IdType service_id,
                  IdType chrc_id,
                  BufferView value,
                  IndicationCallback indicate_cb) override;
  void ShutDown() override { was_shut_down_ = true; }

  PeerId peer_id_;
  LocalServiceManager::WeakPtrType local_services_;
  UpdateHandler update_handler_ = nullptr;
  bool was_shut_down_ = false;
  WeakSelf<MockServer> weak_self_;
};

}  // namespace bt::gatt::testing
