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

#include "fsl_clock.h"
#include "fsl_device_registers.h"
#include "pw_clock_tree/clock_tree.h"

#if defined(MIMXRT798S_cm33_core0_SERIES) || \
    defined(MIMXRT798S_cm33_core1_SERIES)

#include "pw_clock_tree_mcuxpresso/internal/clock_tree_rt798.h"

#elif defined(MIMXRT595S_cm33_SERIES) || defined(MIMXRT595S_dsp_SERIES)

#include "pw_clock_tree_mcuxpresso/internal/clock_tree_rt595.h"

#else

#error "Unsupported MCUXpresso chipset"

#endif  // defined(MIMXRT798S_cm33_core0_SERIES) ||
        // defined(MIMXRT798S_cm33_core1_SERIES)

namespace pw::clock_tree {

/// @module{pw_clock_tree_mcuxpresso}

/// Class template implementing the MCLK IN clock source.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoMclk final : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the MCLK IN clock frequency in Hz and
  /// the dependent clock tree element to enable the MCLK clock source.
  constexpr ClockMcuxpressoMclk(ElementType& source, uint32_t frequency)
      : DependentElement<ElementType>(source), frequency_(frequency) {}

 private:
  /// Set MCLK IN clock frequency.
  Status DoEnable() final {
    // Set global that stores external MCLK IN clock frequency.
    CLOCK_SetMclkFreq(frequency_);
    return OkStatus();
  }

  /// Set MCLK IN clock frequency to 0 Hz.
  Status DoDisable() final {
    // Set global that stores external MCLK IN clock frequency to zero.
    CLOCK_SetMclkFreq(0);
    return OkStatus();
  }

  /// MCLK IN frequency in Hz.
  uint32_t frequency_;
};

/// Alias for a blocking MCLK IN clock tree element.
/// This class should be used if the MCLK IN clock source depends on
/// another blocking clock tree element to enable the MCLK IN clock source.
using ClockMcuxpressoMclkBlocking = ClockMcuxpressoMclk<ElementBlocking>;

/// Alias for a non-blocking MCLK IN clock tree element where updates cannot
/// fail.
using ClockMcuxpressoMclkNonBlocking =
    ClockMcuxpressoMclk<ElementNonBlockingCannotFail>;

/// Class template implementing the clock selector element.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoSelector : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the source clock and the selector value
  /// when the selector should get enabled, and the selector value when
  /// the selector should get disabled to save power.
  constexpr ClockMcuxpressoSelector(ElementType& source,
                                    clock_attach_id_t selector_enable,
                                    clock_attach_id_t selector_disable)
      : DependentElement<ElementType>(source),
        selector_enable_(selector_enable),
        selector_disable_(selector_disable) {}

 private:
  /// Enable selector.
  Status DoEnable() final {
    CLOCK_AttachClk(selector_enable_);
    return OkStatus();
  }

  /// Disable selector.
  Status DoDisable() final {
    CLOCK_AttachClk(selector_disable_);
    return OkStatus();
  }

  /// Enable selector value.
  clock_attach_id_t selector_enable_;
  /// Disable selector value.
  clock_attach_id_t selector_disable_;
};

/// Alias for a blocking clock selector clock tree element.
using ClockMcuxpressoSelectorBlocking =
    ClockMcuxpressoSelector<ElementBlocking>;

/// Alias for a non-blocking clock selector clock tree element where updates
/// cannot fail.
using ClockMcuxpressoSelectorNonBlocking =
    ClockMcuxpressoSelector<ElementNonBlockingCannotFail>;

/// Class template implementing the clock divider element.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoDivider final : public ClockDividerElement<ElementType> {
 public:
  /// Constructor specifying the source clock, the name of the divder and
  /// the divider setting.
  constexpr ClockMcuxpressoDivider(ElementType& source,
                                   clock_div_name_t divider_name,
                                   uint32_t divider)
      : ClockDividerElement<ElementType>(source, divider),
        divider_name_(divider_name) {}

 private:
  /// Set the divider configuration.
  Status DoEnable() final {
    CLOCK_SetClkDiv(divider_name_, this->divider());
    return OkStatus();
  }

  /// Name of divider.
  clock_div_name_t divider_name_;
};

/// Alias for a blocking clock divider clock tree element.
using ClockMcuxpressoDividerBlocking = ClockMcuxpressoDivider<ElementBlocking>;

/// Alias for a non-blocking clock divider clock tree element where updates
/// cannot fail.
using ClockMcuxpressoDividerNonBlocking =
    ClockMcuxpressoDivider<ElementNonBlockingCannotFail>;

/// Class template implementing the `clock_ip_name_t` clocks.
/// Managing `clock_ip_name_t` clocks with the clock tree allows to
/// save power when `FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL` is set.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoClockIp final : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the dependent clock tree element to enable the
  /// `clock_ip_name_t` clock source.
  constexpr ClockMcuxpressoClockIp(ElementType& source, clock_ip_name_t clock)
      : DependentElement<ElementType>(source), clock_(clock) {}

 private:
  /// Enable the clock.
  Status DoEnable() final {
    CLOCK_EnableClock(clock_);
    return OkStatus();
  }

  /// Disable the clock.
  Status DoDisable() final {
    CLOCK_DisableClock(clock_);
    return OkStatus();
  }

  clock_ip_name_t clock_;
};

/// Alias for a blocking ClockIp clock tree element.
/// This class should be used if the ClockIp clock source depends on
/// another blocking clock tree element to enable the ClockIp clock source.
using ClockMcuxpressoClockIpBlocking = ClockMcuxpressoClockIp<ElementBlocking>;

/// Alias for a non-blocking ClockIp clock tree element where updates
/// cannot fail.
using ClockMcuxpressoClockIpNonBlocking =
    ClockMcuxpressoClockIp<ElementNonBlockingCannotFail>;
}  // namespace pw::clock_tree
