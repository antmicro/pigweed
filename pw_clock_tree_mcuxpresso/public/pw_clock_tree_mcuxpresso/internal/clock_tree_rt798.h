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
#include "fsl_power.h"
#include "pw_clock_tree/clock_tree.h"

namespace pw::clock_tree {

/// Class implementing an FRO clock source.
class ClockMcuxpressoFro final
    : public ClockSource<ElementNonBlockingCannotFail> {
 public:
  /// Constructor specifying the FRO divider output to manage.
  constexpr ClockMcuxpressoFro(uintptr_t base, clock_fro_output_en_t fro_output)
      : base_(base), fro_output_(fro_output) {}

 private:
  /// Enable this FRO divider.
  Status DoEnable() final {
    CLOCK_EnableFroClkOutput(base(), base()->CSR.RW | fro_output_);
    return OkStatus();
  }

  /// Disable this FRO divider.
  Status DoDisable() final {
    CLOCK_EnableFroClkOutput(base(), base()->CSR.RW & ~fro_output_);
    return OkStatus();
  }

  inline FRO_Type* base() const { return reinterpret_cast<FRO_Type*>(base_); }
  /// Selected FRO.
  uintptr_t base_;
  /// FRO divider.
  const uint32_t fro_output_;
};

/// Class implementing the low power oscillator clock source.
class ClockMcuxpressoLpOsc final
    : public ClockSource<ElementNonBlockingCannotFail> {
 private:
  /// Enable low power oscillator.
  Status DoEnable() final {
    // Power up the 1MHz low power oscillator power domain.
    POWER_DisablePD(kPDRUNCFG_PD_LPOSC);
    // POWER_ApplyPD() is not necessary for LPOSC_PD.

    return OkStatus();
  }

  /// Disable low power oscillator.
  Status DoDisable() final {
    // Power down the 1MHz low power oscillator power domain.
    POWER_EnablePD(kPDRUNCFG_PD_LPOSC);
    // POWER_ApplyPD() is not necessary for LPOSC_PD.
    return OkStatus();
  }
};

/// Class template implementing the CLK IN pin clock source and selecting
/// it as an input source for OSC Clock source.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoClkIn final : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the CLK IN pin clock frequency in Hz and
  /// the dependent clock tree element to enable the CLK IN pin clock source.
  constexpr ClockMcuxpressoClkIn(ElementType& source, uint32_t frequency)
      : DependentElement<ElementType>(source), frequency_(frequency) {}

 private:
  /// Set CLK IN clock frequency.
  Status DoEnable() final {
    // Set global that stores external CLK IN pin clock frequency.
    CLOCK_SetClkinFreq(frequency_);

    // OSC clock source selector ClkIn.
    const uint8_t kCLOCK_OscClkIn = CLKCTL2_SYSOSCBYPASS_SEL(1);
    CLKCTL2->SYSOSCBYPASS = kCLOCK_OscClkIn;
    return OkStatus();
  }

  /// Set CLK IN clock frequency to 0 Hz.
  Status DoDisable() final {
    // Set global that stores external CLK IN pin clock frequency to zero.
    CLOCK_SetClkinFreq(0);

    // Restore default setting for the selector. RT700 does not support
    // completely gating osc_clk, you can only choose between osc_out and CLKIN.
    CLKCTL2->SYSOSCBYPASS = CLKCTL2_SYSOSCBYPASS_SEL(0);
    return OkStatus();
  }

  /// CLK IN frequency in Hz.
  uint32_t frequency_;
};

/// Alias for a blocking CLK IN pin clock tree element.
/// This class should be used if the CLK IN pin clock source depends on
/// another blocking clock tree element to enable the CLK IN pin clock source.
using ClockMcuxpressoClkInBlocking = ClockMcuxpressoClkIn<ElementBlocking>;

/// Alias for a non-blocking CLK IN pin clock tree element where updates cannot
/// fail.
using ClockMcuxpressoClkInNonBlocking =
    ClockMcuxpressoClkIn<ElementNonBlockingCannotFail>;

/// Class template implementing the audio pll clock element.
///
/// The Audio PLL can either operate in the enabled mode where the PLL
/// and the phase fractional divider are enabled, or it can operate in
/// bypass mode, where both PLL and phase fractional divider are
/// clock gated.
/// When the Audio PLL clock tree gets disabled, both PLL and phase fractional
/// divider will be clock gated.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoAudioPll : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the configuration for the enabled Audio PLL.
  constexpr ClockMcuxpressoAudioPll(ElementType& source,
                                    const clock_audio_pll_config_t& config,
                                    uint8_t audio_pfd_divider)
      : DependentElement<ElementType>(source),
        config_(&config),
        audio_pfd_divider_(audio_pfd_divider) {}

 private:
  /// Configures and enables the audio PLL if `config_` is set, otherwise places
  /// the audio PLL in bypass mode.
  Status DoEnable() override {
    // Configure Audio PLL clock source.
    CLOCK_InitAudioPll(config_);
    CLOCK_InitAudioPfd(kCLOCK_Pfd0, audio_pfd_divider_);
    return OkStatus();
  }

  /// Disables the audio PLL logic.
  Status DoDisable() override {
    // Clock gate the phase fractional divider PFD0.
    CLOCK_DeinitAudioPfd(kCLOCK_Pfd0);
    // Power down Audio PLL
    CLOCK_DeinitAudioPll();
    return OkStatus();
  }

  /// Optional audio PLL configuration.
  const clock_audio_pll_config_t* config_ = nullptr;

  /// Optional audio kCLOCK_Pfd0 clock divider value.
  const uint8_t audio_pfd_divider_ = 0;
};

/// Alias for a blocking audio PLL clock tree element.
using ClockMcuxpressoAudioPllBlocking =
    ClockMcuxpressoAudioPll<ElementBlocking>;

/// Alias for a non-blocking audio PLL clock tree element where updates
/// cannot fail.
using ClockMcuxpressoAudioPllNonBlocking =
    ClockMcuxpressoAudioPll<ElementNonBlockingCannotFail>;

/// Class template implementing the Rtc clock tree element.
///
/// Template argument `ElementType` can be of class `ElementBlocking` or
/// `ElementNonBlockingCannotFail`.
template <typename ElementType>
class ClockMcuxpressoRtc final : public DependentElement<ElementType> {
 public:
  /// Constructor specifying the dependent clock tree element to enable the
  /// Rtc clock source.
  constexpr ClockMcuxpressoRtc(ElementType& source,
                               clock_osc32k_config_t& config)
      : DependentElement<ElementType>(source), config_(config) {}

 private:
  /// Enable 32 kHz RTC oscillator
  Status DoEnable() final {
    // Enable 32kHZ output of RTC oscillator.
    CLOCK_EnableOsc32K(&config_);
    return OkStatus();
  }

  /// Disable 32 kHz RTS oscillator.
  Status DoDisable() final {
    // Disable 32KHz output of RTC oscillator.
    CLOCK_DisableOsc32K();
    return OkStatus();
  }

  clock_osc32k_config_t& config_;
};

/// Alias for a blocking Rtc clock tree element.
/// This class should be used if the Rtc clock source depends on
/// another blocking clock tree element to enable the Rtc clock source.
using ClockMcuxpressoRtcBlocking = ClockMcuxpressoRtc<ElementBlocking>;

/// Alias for a non-blocking Rtc clock tree element where updates
/// cannot fail.
using ClockMcuxpressoRtcNonBlocking =
    ClockMcuxpressoRtc<ElementNonBlockingCannotFail>;

}  // namespace pw::clock_tree
