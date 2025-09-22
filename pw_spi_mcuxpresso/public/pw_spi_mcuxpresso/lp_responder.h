// Copyright 2025 The Pigweed Authors
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

#include <atomic>
#include <cstdint>

#include "fsl_lpspi.h"
#include "fsl_lpspi_edma.h"
#include "lib/stdcompat/utility.h"
#include "pw_assert/check.h"
#include "pw_dma_mcuxpresso/edma.h"
#include "pw_spi/initiator.h"  // Reuse config defs
#include "pw_spi/responder.h"
#include "pw_status/status.h"

// Vendor terminology requires this to be disabled.
// inclusive-language: disable

namespace pw::spi {

enum class FifoErrorCheck {
  kNone,     // Don't check for FIFO error.
  kLogOnly,  // Only log on FIFO error.
  kError,    // Log and return DATA_LOSS.
};

class McuxpressoLpResponder : public Responder {
 public:
  struct Config {
    ClockPolarity polarity;
    ClockPhase phase;
    BitsPerWord bits_per_word;
    BitOrder bit_order;
    uint32_t base_address;  // Flexcomm peripheral base address.

    // If enabled, the FIFO status registers are checked for error
    // (underflow/overflow) upon transfer completion, returning DATA_LOSS if
    // detected.
    //
    // NOTE: A false positive could be triggered if this is enabled and the
    // initiator clocks more bytes than the transfer is set up to send+receive.
    FifoErrorCheck check_fifo_error = FifoErrorCheck::kNone;
  };

  McuxpressoLpResponder(const Config config,
                        dma::McuxpressoDmaChannel& tx_dma,
                        dma::McuxpressoDmaChannel& rx_dma)
      : config_(config),
        base_(reinterpret_cast<LPSPI_Type*>(config.base_address)),
        tx_dma_(tx_dma),
        rx_dma_(rx_dma) {}

  ~McuxpressoLpResponder() override { PW_CRASH("Destruction not supported"); }

  McuxpressoLpResponder(const McuxpressoLpResponder&) = delete;
  McuxpressoLpResponder& operator=(const McuxpressoLpResponder&) = delete;
  McuxpressoLpResponder(const McuxpressoLpResponder&&) = delete;
  McuxpressoLpResponder& operator=(const McuxpressoLpResponder&&) = delete;

  Status Initialize();

 private:
  // pw::spi::Responder impl.
  void DoSetCompletionHandler(
      Function<void(ByteSpan, Status)> callback) override {
    completion_callback_ = std::move(callback);
  };
  Status DoWriteReadAsync(ConstByteSpan tx_data, ByteSpan rx_data) override;
  void DoCancel() override;

  static void SdkCallback(LPSPI_Type* base,
                          lpspi_slave_edma_handle_t* handle,
                          status_t sdk_status,
                          void* userData);
  void DmaComplete(status_t sdk_status);

  void TransferComplete(Status status, size_t bytes_transferred);

  const Config config_;
  LPSPI_Type* base_;
  lpspi_slave_edma_handle_t handle_;
  dma::McuxpressoDmaChannel& tx_dma_;
  dma::McuxpressoDmaChannel& rx_dma_;
  Function<void(ByteSpan, Status)> completion_callback_;

  // Current WriteReadAsync
  struct Transaction {
    ByteSpan rx_data;

    explicit operator bool() const noexcept { return !rx_data.empty(); }
  } current_transaction_;

  enum class State : uint32_t {
    // No transaction in progress.
    //
    // DoWriteReadAsync: Move to kBusy
    // SDK callback: Nothing (erroneous?)
    // Cancel: Nothing
    kIdle = 0,

    // Transaction started, waiting for SDK callback or cancellation.
    //
    // DoWriteReadAsync: return error
    // SDK callback: Complete, call callback and move to kIdle
    // Cancel: Cancel, call callback and move to kIdle
    kBusy = 1,
  };
  std::atomic<State> state_ = State::kIdle;

  bool TryChangeState(State expected, State desired, State* old = nullptr) {
    if (state_.compare_exchange_strong(expected, desired)) {
      return true;
    }
    if (old) {
      *old = expected;
    }
    return false;
  }
};

}  // namespace pw::spi
