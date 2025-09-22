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
#include "pw_spi_mcuxpresso/lp_responder.h"

#include <cinttypes>

#include "fsl_lpflexcomm.h"
#include "fsl_lpspi.h"
#include "fsl_lpspi_edma.h"
#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_span/cast.h"
#include "pw_status/try.h"

// Vendor terminology requires this to be disabled.
// inclusive-language: disable

namespace pw::spi {
namespace {

Status ToPwStatus(status_t status) {
  switch (status) {
    case kStatus_Success:
      [[fallthrough]];
    case kStatus_LPSPI_Idle:
      return OkStatus();

    case kStatus_ReadOnly:
      return Status::PermissionDenied();
    case kStatus_OutOfRange:
      return Status::OutOfRange();
    case kStatus_InvalidArgument:
      return Status::InvalidArgument();
    case kStatus_Timeout:
      return Status::DeadlineExceeded();
    case kStatus_NoTransferInProgress:
      return Status::FailedPrecondition();

    case kStatus_Fail:
      [[fallthrough]];
    default:
      PW_LOG_ERROR("Mcuxpresso SPI unknown error code: %d",
                   static_cast<int>(status));
      return Status::Unknown();
  }
}

Status SetSdkConfig(const McuxpressoLpResponder::Config& config,
                    lpspi_slave_config_t& sdk_config) {
  switch (config.polarity) {
    case ClockPolarity::kActiveLow:
      sdk_config.cpol = kLPSPI_ClockPolarityActiveLow;
      break;
    case ClockPolarity::kActiveHigh:
      sdk_config.cpol = kLPSPI_ClockPolarityActiveHigh;
      break;
    default:
      return Status::InvalidArgument();
  }

  switch (config.phase) {
    case ClockPhase::kRisingEdge:
      sdk_config.cpha = kLPSPI_ClockPhaseFirstEdge;
      break;
    case ClockPhase::kFallingEdge:
      sdk_config.cpha = kLPSPI_ClockPhaseSecondEdge;
      break;
    default:
      return Status::InvalidArgument();
  }

  switch (config.bit_order) {
    case BitOrder::kMsbFirst:
      sdk_config.direction = kLPSPI_MsbFirst;
      break;
    case BitOrder::kLsbFirst:
      sdk_config.direction = kLPSPI_LsbFirst;
      break;
    default:
      return Status::InvalidArgument();
  }

  const auto bits_per_word = config.bits_per_word();
  if (bits_per_word < 8 || bits_per_word > 4096) {
    return Status::InvalidArgument();
  }
  sdk_config.bitsPerFrame = bits_per_word;

  return OkStatus();
}

//
// Helpful things missing from the SDK
//

bool LPSPI_RxError(LPSPI_Type* base) { return (base->SR & LPSPI_SR_REF_MASK); }

bool LPSPI_TxError(LPSPI_Type* base) { return (base->SR & LPSPI_SR_TEF_MASK); }

}  // namespace

Status McuxpressoLpResponder::Initialize() {
  lpspi_slave_config_t sdk_config;
  lpspi_slave_edma_transfer_callback_t callback;

  LPSPI_SlaveGetDefaultConfig(&sdk_config);
  PW_TRY(SetSdkConfig(config_, sdk_config));

  // Hard coded for now, till added to Config
  sdk_config.whichPcs = kLPSPI_Pcs0;
  sdk_config.pcsActiveHighOrLow = kLPSPI_PcsActiveLow;

  LPSPI_SlaveInit(base_, &sdk_config);

  // Without CS deassertion, we use the SPI driver callback (invoked by DMA
  // IRQ) to complete transfers.
  callback = McuxpressoLpResponder::SdkCallback;

  // Enable the DMA channel interrupts.
  // These are enabled by default by DMA_CreateHandle(), but re-enable them
  // anyway in case they were disabled for some reason.
  rx_dma_.EnableInterrupts();
  tx_dma_.EnableInterrupts();

  LPSPI_SlaveTransferCreateHandleEDMA(
      base_, &handle_, callback, this, rx_dma_.handle(), tx_dma_.handle());

  return OkStatus();
}

void McuxpressoLpResponder::TransferComplete(Status status,
                                             size_t bytes_transferred) {
  // Abort the DMA transfer (if active).
  LPSPI_SlaveTransferAbortEDMA(base_, &handle_);

  // Check for TX underflow / RX overflow
  //
  // Ideally we want to check for FIFO under/overflow only *while* the transfer
  // is running. But if the initiator sent more bytes than the DMA was set up
  // to tx/rx, both of these errors will happen (after the DMA is complete).
  //
  // To do this without risk of false positives, we would need to find a way to
  // capture this status immediately when the DMA is complete, or otherwise
  // monitor it during the transfer. Perhaps with a custom DMA chain, we could
  // capture the status registers via DMA, but that might still be too late.
  if (status.ok() && config_.check_fifo_error != FifoErrorCheck::kNone) {
    if (LPSPI_RxError(base_)) {
      PW_LOG_ERROR("RX FIFO overflow detected!");
      if (config_.check_fifo_error == FifoErrorCheck::kError) {
        status = Status::DataLoss();
      }
    }
    if (LPSPI_TxError(base_)) {
      PW_LOG_ERROR("TX FIFO underflow detected!");
      if (config_.check_fifo_error == FifoErrorCheck::kError) {
        status = Status::DataLoss();
      }
    }
  }

  // Empty the FIFOs.
  // If the initiator sent more bytes than the DMA was set up to receive, the
  // RXFIFO will have the residue. This isn't strictly necessary since they'll
  // be cleared on the next call to SPI_SlaveTransferDMA(), but we do it anyway
  // for cleanliness.
  LPSPI_FlushFifo(base_, true, true);

  // Clear the FIFO DMA request signals.
  //
  // From IMXRT500RM 53.4.2.1.2 DMA operation:
  // "A DMA request is provided for each SPI direction, and can be used instead
  // of interrupts for transferring data... The DMA controller provides an
  // acknowledgement signal that clears the related request when it (the DMA
  // controller) completes handling that request."
  //
  // If the initiator sent more bytes than the DMA was set up to receive, this
  // request signal will remain latched on, even after the FIFO is emptied.
  // This would cause a subsequent transfer to receive one stale residual byte
  // from this prior transfer.
  //
  // We force if off here by disabling the DMA request signal.
  // It will be re-enabled on the next transfer.
  LPSPI_DisableDMA(base_, kLPSPI_TxDmaEnable | kLPSPI_RxDmaEnable);

  // Invoke the callback
  auto received = current_transaction_.rx_data.subspan(0, bytes_transferred);
  current_transaction_ = {};
  completion_callback_(received, status);
}

void McuxpressoLpResponder::SdkCallback(LPSPI_Type* base,
                                        lpspi_slave_edma_handle_t* handle,
                                        status_t sdk_status,
                                        void* userData) {
  // WARNING: This is called in IRQ context.
  auto* responder = static_cast<McuxpressoLpResponder*>(userData);
  PW_CHECK_PTR_EQ(base, responder->base_);
  PW_CHECK_PTR_EQ(handle, &responder->handle_);

  return responder->DmaComplete(sdk_status);
}

void McuxpressoLpResponder::DmaComplete(status_t sdk_status) {
  // WARNING: This is called in IRQ context.

  // Move to idle state.
  if (State prev; !TryChangeState(State::kBusy, State::kIdle, &prev)) {
    // Spurious callback? Or race condition in DoWriteReadAsync()?
    PW_LOG_WARN("DmaComplete not in busy state, but %u",
                static_cast<unsigned int>(prev));
    return;
  }

  // Transfer complete.
  auto status = ToPwStatus(sdk_status);
  size_t bytes_transferred =
      status.ok() ? current_transaction_.rx_data.size() : 0;
  TransferComplete(status, bytes_transferred);
}

Status McuxpressoLpResponder::DoWriteReadAsync(ConstByteSpan tx_data,
                                               ByteSpan rx_data) {
  if (!TryChangeState(State::kIdle, State::kBusy)) {
    PW_LOG_ERROR("Transaction already started");
    return Status::FailedPrecondition();
  }
  PW_CHECK(!current_transaction_);

  // TODO(jrreinhart): There is a race here. If DoCancel() is called, it will
  // move to kIdle, and invoke the callback with CANCELLED. But then we will
  // still go on to perform the transfer anyway. When the transfer completes,
  // SdkCallback will see kIdle and skip the callback. We avoid this problem
  // by saying that DoWriteReadAsync() and DoCancel() should not be called from
  // different threads, thus we only have to worry about DoCancel() racing the
  // hardware / IRQ.

  lpspi_transfer_t transfer = {};

  pw::span<const uint8_t> tx = span_cast<const uint8_t>(tx_data);
  pw::span<uint8_t> rx = span_cast<uint8_t>(rx_data);

  if (!tx.empty() && !rx.empty()) {
    // spi_transfer_t has only a single dataSize member, so tx and
    // rx must be the same size. Separate rx/tx data sizes could
    // theoretically be handled, but the SDK doesn't support it.
    //
    // TODO(jrreinhart) Support separate rx/tx data sizes.
    // For non-DMA, it's a pretty simple patch.
    // It should be doable for DMA also, but I haven't looked into it.
    if (tx.size() != rx.size()) {
      return Status::InvalidArgument();
    }

    transfer.txData = const_cast<uint8_t*>(tx.data());
    transfer.rxData = rx.data();
    transfer.dataSize = rx.size();
  } else if (!tx.empty()) {
    transfer.txData = const_cast<uint8_t*>(tx.data());
    transfer.dataSize = tx.size();
  } else if (!rx.empty()) {
    transfer.rxData = rx.data();
    transfer.dataSize = rx.size();
  } else {
    return Status::InvalidArgument();
  }

  // PCS hard coded for now, till added to Config
  transfer.configFlags = kLPSPI_SlavePcs0 | kLPSPI_SlaveByteSwap;

  current_transaction_ = {
      .rx_data = rx_data,
  };

  status_t sdk_status = LPSPI_SlaveTransferEDMA(base_, &handle_, &transfer);
  if (sdk_status != kStatus_Success) {
    PW_LOG_ERROR("SPI_SlaveTransferDMA failed: %d", sdk_status);
    return ToPwStatus(sdk_status);
  }

  return OkStatus();
}

void McuxpressoLpResponder::DoCancel() {
  if (!TryChangeState(State::kBusy, State::kIdle)) {
    return;
  }
  TransferComplete(Status::Cancelled(), 0);
}

}  // namespace pw::spi
