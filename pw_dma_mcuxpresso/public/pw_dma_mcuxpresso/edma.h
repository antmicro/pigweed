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

#include <cstdint>

#include "fsl_edma.h"
#include "pw_assert/check.h"
#include "pw_status/status.h"

namespace pw::dma {

class McuxpressoDmaController;

#if defined(CPU_MIMXRT798SGFOA_cm33_core0) || \
    defined(CPU_MIMXRT798SGFOA_cm33_core1)

enum EDMAController : uintptr_t {
#if defined(CPU_MIMXRT798SGFOA_cm33_core0)
  EDMA0 = DMA0_BASE,
  EDMA1 = DMA1_BASE,
#endif
#if defined(CPU_MIMXRT798SGFOA_cm33_core1)
  EDMA2 = DMA2_BASE,
  EDMA3 = DMA3_BASE,
#endif
};

#endif
// Represents a single channel of a DMA controller.
//
// NOTE: Because the SDK maintains a permanent reference to this class's
// members, these objects must have static lifetime at the time Init() is
// called and ever after. The destructor will intentionally crash.
class McuxpressoDmaChannel {
  friend McuxpressoDmaController;

 public:
  McuxpressoDmaChannel(const McuxpressoDmaChannel&) = delete;
  McuxpressoDmaChannel& operator=(const McuxpressoDmaChannel&) = delete;

  ~McuxpressoDmaChannel() {
    if (initialized_) {
      PW_CRASH("Destruction of initialized McuxpressoDmaChannel not supported");
    }
  }

  // NOTE: No locks are required for per-channel operations (since b/326468328).

  void Init() {
    if (initialized_) {
      return;
    }

    // NOTE: EDMA_CreateHandle() registers the handle in a global array
    // (s_DMAHandle) which is referenced by the DMA IRQ handler, and there
    // unfortunately no way to unregister it, so this object must have static
    // lifetime. The destructor will call PW_CRASH to try and enforce that.
    EDMA_ResetChannel(controller_base(), channel_);
    EDMA_CreateHandle(handle(), controller_base(), channel_);
    EDMA_ClearChannelStatusFlags(handle(), channel_, 0xFFFFFFFF);
    EDMA_EnableChannelInterrupts(
        controller_base(),
        channel_,
        kEDMA_MajorInterruptEnable | kEDMA_ErrorInterruptEnable);

    // Note: This automatically enables channel interrupts.
    initialized_ = true;
  }

  void Enable() { EDMA_EnableChannelRequest(controller_base(), channel_); }

  void Disable() { EDMA_DisableChannelRequest(controller_base(), channel_); }

  void SetPriority(uint32_t priority) {
    edma_channel_Preemption_config_t config = {
        .enableChannelPreemption = false,
        .enablePreemptAbility = false,
        .channelPriority = static_cast<uint8_t>(priority)};
    EDMA_SetChannelPreemptionConfig(controller_base(), channel_, &config);
  }

  // "A DMA channel is considered active when a DMA operation has been started
  // but not yet fully completed."
  bool IsActive() {
    uint32_t status = EDMA_GetChannelStatusFlags(controller_base(), channel_);
    return !(status & kEDMA_DoneFlag);
  }

  // "A DMA channel is considered busy when there is any operation related to
  // that channel in the DMA controller’s internal pipeline. This information
  // can be used after a DMA channel is disabled by software (but still
  // active), allowing confirmation that there are no remaining operations in
  // progress for that channel."
  bool IsBusy() {
    return EDMA_GetRemainingMajorLoopCount(controller_base(), channel_) > 0;
  }

  void EnableInterrupts() {
    EDMA_EnableChannelInterrupts(
        controller_base(),
        channel_,
        kEDMA_MajorInterruptEnable | kEDMA_ErrorInterruptEnable);
  }

  void DisableInterrupts() {
    EDMA_DisableChannelInterrupts(
        controller_base(),
        channel_,
        kEDMA_MajorInterruptEnable | kEDMA_ErrorInterruptEnable);
  }

  edma_handle_t* handle() { return &handle_; }

 private:
  explicit McuxpressoDmaChannel(McuxpressoDmaController& controller,
                                uint32_t channel)
      : controller_(controller), channel_(channel) {}

  DMA_Type* controller_base() const;

  McuxpressoDmaController& controller_;
  uint32_t const channel_;
  edma_handle_t handle_;
  bool initialized_ = false;
};

// Represents a DMA Controller.
class McuxpressoDmaController {
 public:
  constexpr McuxpressoDmaController(uintptr_t base_address)
      : base_address_(base_address) {}

  McuxpressoDmaController(const McuxpressoDmaController&) = delete;
  McuxpressoDmaController& operator=(const McuxpressoDmaController&) = delete;

  Status Init() {
    edma_config_t config;
    EDMA_GetDefaultConfig(&config);
    EDMA_Init(base(), &config);
    return OkStatus();
  }

  // Get a channel object for the given channel number.
  //
  // NOTE: You must call Init() on the resulting object.
  //
  // NOTE: The resulting object *must* have static lifetime when Init() is
  // called, and ever after.
  McuxpressoDmaChannel GetChannel(uint32_t channel) {
    return McuxpressoDmaChannel(*this, channel);
  }
  DMA_Type* base() const { return reinterpret_cast<DMA_Type*>(base_address_); }

 private:
  uintptr_t const base_address_;
};

inline DMA_Type* McuxpressoDmaChannel::controller_base() const {
  return controller_.base();
}

}  // namespace pw::dma
