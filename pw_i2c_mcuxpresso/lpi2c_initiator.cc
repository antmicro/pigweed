// Copyright 2022 The Pigweed Authors
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
#include "pw_i2c_mcuxpresso/lpi2c_initiator.h"

#include <mutex>

#include "fsl_lpi2c.h"
#include "pw_chrono/system_clock.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::i2c {
namespace {

Status HalStatusToPwStatus(status_t status) {
  switch (status) {
    case kStatus_Success:
      return OkStatus();
    case kStatus_LPI2C_Nak:
      return Status::Unavailable();
    case kStatus_LPI2C_Timeout:
      return Status::DeadlineExceeded();
    default:
      return Status::Unknown();
  }
}
}  // namespace

// inclusive-language: disable
void McuxpressoLpI2cInitiator::Enable() {
  std::lock_guard lock(mutex_);

  lpi2c_master_config_t master_config;
  LPI2C_MasterGetDefaultConfig(&master_config);
  master_config.baudRate_Hz = config_.baud_rate_hz;
  LPI2C_MasterInit(base_, &master_config, CLOCK_GetFreq(config_.clock_name));

  // Create the handle for the non-blocking transfer and register callback.
  LPI2C_MasterTransferCreateHandle(
      base_,
      &handle_,
      McuxpressoLpI2cInitiator::TransferCompleteCallback,
      this);

  enabled_ = true;
}

void McuxpressoLpI2cInitiator::Disable() {
  std::lock_guard lock(mutex_);
  LPI2C_MasterDeinit(base_);
  enabled_ = false;
}

McuxpressoLpI2cInitiator::~McuxpressoLpI2cInitiator() { Disable(); }

void McuxpressoLpI2cInitiator::TransferCompleteCallback(LPI2C_Type*,
                                                        lpi2c_master_handle_t*,
                                                        status_t status,
                                                        void* initiator_ptr) {
  McuxpressoLpI2cInitiator& initiator =
      *static_cast<McuxpressoLpI2cInitiator*>(initiator_ptr);
  initiator.callback_isl_.lock();
  initiator.transfer_status_ = status;
  initiator.callback_isl_.unlock();
  initiator.callback_complete_notification_.release();
}

Status McuxpressoLpI2cInitiator::InitiateNonBlockingTransfer(
    chrono::SystemClock::duration rw_timeout,
    lpi2c_master_transfer_t* transfer) {
  const status_t status =
      LPI2C_MasterTransferNonBlocking(base_, &handle_, transfer);
  if (status != kStatus_Success) {
    return HalStatusToPwStatus(status);
  }

  if (!callback_complete_notification_.try_acquire_for(rw_timeout)) {
    LPI2C_MasterTransferAbort(base_, &handle_);
    return Status::DeadlineExceeded();
  }

  callback_isl_.lock();
  const status_t transfer_status = transfer_status_;
  callback_isl_.unlock();

  return HalStatusToPwStatus(transfer_status);
}

// Performs non-blocking I2C write, read and read-after-write depending on the
// tx and rx buffer states.
Status McuxpressoLpI2cInitiator::DoWriteReadFor(
    Address device_address,
    ConstByteSpan tx_buffer,
    ByteSpan rx_buffer,
    chrono::SystemClock::duration timeout) {
  if (timeout <= chrono::SystemClock::duration::zero()) {
    return Status::DeadlineExceeded();
  }

  const uint8_t address = device_address.GetSevenBit();
  std::lock_guard lock(mutex_);

  if (!enabled_) {
    return Status::FailedPrecondition();
  }

  if (!tx_buffer.empty() && rx_buffer.empty()) {
    lpi2c_master_transfer_t transfer{kLPI2C_TransferDefaultFlag,
                                     address,
                                     kLPI2C_Write,
                                     0,
                                     0,
                                     const_cast<std::byte*>(tx_buffer.data()),
                                     tx_buffer.size()};
    return InitiateNonBlockingTransfer(timeout, &transfer);
  } else if (tx_buffer.empty() && !rx_buffer.empty()) {
    lpi2c_master_transfer_t transfer{kLPI2C_TransferDefaultFlag,
                                     address,
                                     kLPI2C_Read,
                                     0,
                                     0,
                                     rx_buffer.data(),
                                     rx_buffer.size()};
    return InitiateNonBlockingTransfer(timeout, &transfer);
  } else if (!tx_buffer.empty() && !rx_buffer.empty()) {
    lpi2c_master_transfer_t w_transfer{kLPI2C_TransferNoStopFlag,
                                       address,
                                       kLPI2C_Write,
                                       0,
                                       0,
                                       const_cast<std::byte*>(tx_buffer.data()),
                                       tx_buffer.size()};
    const chrono::SystemClock::time_point deadline =
        chrono::SystemClock::TimePointAfterAtLeast(timeout);
    PW_TRY(InitiateNonBlockingTransfer(timeout, &w_transfer));
    lpi2c_master_transfer_t r_transfer{kLPI2C_TransferRepeatedStartFlag,
                                       address,
                                       kLPI2C_Read,
                                       0,
                                       0,
                                       rx_buffer.data(),
                                       rx_buffer.size()};
    const chrono::SystemClock::duration time_remaining =
        deadline - chrono::SystemClock::now();
    if (time_remaining <= chrono::SystemClock::duration::zero()) {
      // Abort transfer in an unlikely scenario of timeout even with
      // successful write.
      LPI2C_MasterTransferAbort(base_, &handle_);
      return Status::DeadlineExceeded();
    }
    return InitiateNonBlockingTransfer(time_remaining, &r_transfer);
  } else {
    return Status::InvalidArgument();
  }
}
// inclusive-language: enable
}  // namespace pw::i2c
