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

#include "pw_digital_io_mcuxpresso/digital_io_gpio.h"

#include <array>
#include <mutex>

#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_reset.h"
#include "pw_assert/check.h"
#include "pw_digital_io/digital_io.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"

namespace pw::digital_io {
namespace {

constexpr std::array kGpioClocks = GPIO_CLOCKS;
constexpr std::array kGpioResets = GPIO_RSTS;
constexpr uint32_t kNumGpioPorts = kGpioResets.size();

// This lock is used to prevent simultaneous access to the list of registered
// interrupt handlers and the underlying interrupt hardware.
sync::InterruptSpinLock port_interrupts_lock;

// The HS GPIO block culminates all pin interrupts into single interrupt
// vectors. Each GPIO port has a corresponding interrupt register and status
// register.
//
// It would be expensive from a memory perspective to statically define a
// handler pointer for each pin so a linked list used instead. It's added to
// whenever a handler is registered with DoSetInterruptHandler() and removed
// whenever a handler is de-registered with DoSetInterruptHandler().
//
// To improve handler lookup performance, an array of linked lists is created
// below, 1 per port. This allows for traversal of smaller linked lists, and
// only the linked lists that have active interrupts.
std::array<pw::IntrusiveForwardList<McuxpressoDigitalInOutInterrupt>,
           kNumGpioPorts>
    port_interrupts PW_GUARDED_BY(port_interrupts_lock);

}  // namespace

McuxpressoDigitalOut::McuxpressoDigitalOut(GPIO_Type* base,
                                           uint32_t port,
                                           uint32_t pin,
                                           pw::digital_io::State initial_state)
    : base_(base), port_(port), pin_(pin), initial_state_(initial_state) {
  PW_CHECK(base != nullptr);
  PW_CHECK(port < kGpioClocks.size());
  PW_CHECK(port < kGpioResets.size());
}

pw::Status McuxpressoDigitalOut::DoEnable(bool enable) {
  if (enable) {
    if (is_enabled()) {
      return pw::OkStatus();
    }

    gpio_pin_config_t config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = static_cast<uint8_t>(
            initial_state_ == pw::digital_io::State::kActive ? 1U : 0U),
    };
    GPIO_PinInit(base_, pin_, &config);

  } else {
    // Set to input on disable.
    gpio_pin_config_t config = {
        .pinDirection = kGPIO_DigitalInput,
        .outputLogic = 0,
    };

    GPIO_PinInit(base_, pin_, &config);

    // Can't disable clock as other users on same port may be active.
  }
  enabled_ = enable;
  return pw::OkStatus();
}

pw::Status McuxpressoDigitalOut::DoSetState(pw::digital_io::State state) {
  if (!is_enabled()) {
    return pw::Status::FailedPrecondition();
  }
  GPIO_PinWrite(base_, pin_, state == pw::digital_io::State::kActive ? 1 : 0);
  return pw::OkStatus();
}

McuxpressoDigitalIn::McuxpressoDigitalIn(GPIO_Type* base,
                                         uint32_t port,
                                         uint32_t pin)
    : base_(base), port_(port), pin_(pin) {
  PW_CHECK(base != nullptr);
  PW_CHECK(port < kGpioClocks.size());
  PW_CHECK(port < kGpioResets.size());
}

pw::Status McuxpressoDigitalIn::DoEnable(bool enable) {
  if (enable) {
    if (is_enabled()) {
      return pw::OkStatus();
    }
  } else {
    enabled_ = false;
    // Can't disable clock as other users on same port may be active.
    return pw::OkStatus();
  }

  gpio_pin_config_t config = {
      .pinDirection = kGPIO_DigitalInput,
      .outputLogic = 0,
  };
  GPIO_PinInit(base_, pin_, &config);

  enabled_ = enable;
  return pw::OkStatus();
}

pw::Result<pw::digital_io::State> McuxpressoDigitalIn::DoGetState() {
  if (!is_enabled()) {
    return pw::Status::FailedPrecondition();
  }
  uint32_t value = GPIO_PinRead(base_, pin_);
  return value == 1 ? pw::digital_io::State::kActive
                    : pw::digital_io::State::kInactive;
}

McuxpressoDigitalInOutInterrupt::McuxpressoDigitalInOutInterrupt(
    GPIO_Type* base, uint32_t port, uint32_t pin, bool output)
    : base_(base), port_(port), pin_(pin), output_(output) {
  PW_CHECK(base != nullptr);
  PW_CHECK(port < kGpioClocks.size());
  PW_CHECK(port < kGpioResets.size());
  PW_CHECK(port < port_interrupts.size());
}

pw::Status McuxpressoDigitalInOutInterrupt::DoEnable(bool enable) {
  if (enable) {
    if (is_enabled()) {
      return pw::OkStatus();
    }
  } else {
    enabled_ = false;
    // Can't disable clock as other users on same port may be active.
    return pw::OkStatus();
  }

  gpio_pin_config_t config = {
      .pinDirection = (output_ ? kGPIO_DigitalOutput : kGPIO_DigitalInput),
      .outputLogic = 0,
  };
  GPIO_PinInit(base_, pin_, &config);

  enabled_ = enable;
  return pw::OkStatus();
}

pw::Result<pw::digital_io::State>
McuxpressoDigitalInOutInterrupt::DoGetState() {
  if (!is_enabled()) {
    return pw::Status::FailedPrecondition();
  }
  uint32_t value = GPIO_PinRead(base_, pin_);
  return value == 1 ? pw::digital_io::State::kActive
                    : pw::digital_io::State::kInactive;
}

pw::Status McuxpressoDigitalInOutInterrupt::DoSetState(
    pw::digital_io::State state) {
  if (!is_enabled()) {
    return pw::Status::FailedPrecondition();
  }
  GPIO_PinWrite(base_, pin_, state == pw::digital_io::State::kActive ? 1 : 0);

  return pw::OkStatus();
}

}  // namespace pw::digital_io
