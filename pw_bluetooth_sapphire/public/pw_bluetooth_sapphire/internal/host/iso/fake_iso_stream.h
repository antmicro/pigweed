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

#include <optional>

#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream.h"

namespace bt::iso::testing {

// Testing replacement for IsoStream with functionality built up as needed.
class FakeIsoStream : public IsoStream {
 public:
  FakeIsoStream() : weak_self_(this) {}

  bool OnCisEstablished(const hci::EmbossEventPacket& event) override {
    return true;
  }

  void SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection direction,
      const bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter>& codec_id,
      std::optional<std::vector<uint8_t>> codec_configuration,
      uint32_t controller_delay_usecs,
      fit::function<void(IsoStream::SetupDataPathError)> cb) override {
    cb(setup_data_path_status_);
  }

  void Close() override {}

  IsoStream::WeakPtrType GetWeakPtr() override { return weak_self_.GetWeakPtr(); }

  void SetSetupDataPathReturnStatus(IsoStream::SetupDataPathError status) {
    setup_data_path_status_ = status;
  }

 protected:
  IsoStream::SetupDataPathError setup_data_path_status_ =
      IsoStream::SetupDataPathError::kSuccess;

 private:
  WeakSelf<FakeIsoStream> weak_self_;
};

}  // namespace bt::iso::testing
