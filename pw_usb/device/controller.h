#pragma once

#include "endpoint.h"
#include "event.h"
#include "pw_function/function.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::usb_device {

// clang-format off
using EndpointDataFunction = Function<void(pw::usb_device::EndpointAddress, span<const std::byte>)>;
using EventFunction = Function<void(pw::usb_device::Event)>;
using StatusUpdateFunction = Function<void(pw::Status)>;
// clang-format on

// Controller class allows platform-agnostic access to the bottom
// layer of an USB device as defined in USB 2.0 Specification, Ch.9
// (USB Device Framework).
// See also:
//  - USB 2.0 Specification, Ch.5.2.2 (USB Devices).
//  - USB 2.0 Specification, Figure 5.9
class Controller {
 public:
  /// Initialize the controller.
  /// This must be called before calling any other method of the `Controller`.
  // `complete_callback` will be called with the result of initialization.
  // `error_callback` will be called for fatal errors that occur after
  // initialization. After a fatal error, this object is invalid. `Close` should
  // be called to ensure a safe clean up.
  virtual void Initialize(StatusUpdateFunction complete_callback,
                          StatusUpdateFunction error_callback) = 0;
  /// Deinitialize the controller.
  /// The given function is called when deinitialization of the controller was
  /// completed.
  virtual void Close(StatusUpdateFunction) = 0;

  /// Fully enable the USB controller and start sending out events.
  /// Called after `Initialize` and configuration of all required
  /// endpoints/event functions.
  ///
  /// This is separate from initialization, as it may involve enabling
  /// USB clocks.
  ///
  /// The given function is called when enabling the controller was
  /// completed.
  virtual void Run(StatusUpdateFunction) = 0;
  /// Disable the USB controller and stop sending out events
  /// This may disable the USB clock to lower power consumption.
  /// The controller may be reenabled by calling `Run` again.
  ///
  /// The given function is called when enabling the controller was
  /// completed.
  virtual void Stop(StatusUpdateFunction) = 0;

  /// Set an event handler function to be called when certain
  /// low-level USB events occur. This should be called before
  /// calling `Initialize` to ensure no events are missed.
  virtual void SetEventFunction(EventFunction) = 0;

  /// Initialize a given endpoint
  /// This must be called before any data can be received or sent to
  /// the endpoint.
  /// `callback` will be called with the result of the operation.
  virtual void InitEndpoint(pw::usb_device::EndpointAddress endpoint,
                            StatusUpdateFunction callback) = 0;
  /// Deinitialize a given endpoint
  /// This must be called when an endpoint is not used anymore (for example,
  /// as a result of interface or configuration change).
  /// `callback` will be called with the result of the operation.
  virtual void DeinitEndpoint(pw::usb_device::EndpointAddress endpoint,
                              StatusUpdateFunction callback) = 0;
  /// Stall a given endpoint
  /// `callback` will be called with the result of the operation.
  virtual void StallEndpoint(pw::usb_device::EndpointAddress endpoint,
                             StatusUpdateFunction callback) = 0;
  /// Unstall a given endpoint
  /// `callback` will be called with the result of the operation.
  virtual void UnstallEndpoint(pw::usb_device::EndpointAddress endpoint,
                               StatusUpdateFunction callback) = 0;

  /// Send data over an endpoint asynchronously
  /// The given endpoint must have been initialized beforehand using
  /// `InitEndpoint`. The endpoint must be an IN endpoint (direction
  /// bit set).
  virtual void Send(pw::usb_device::EndpointAddress endpoint,
                    span<const std::byte> data) = 0;
  /// Set a function to be called when data is received
  /// The given function is called when data is received on any endpoint.
  /// Data passed to the function is only valid for the duration of the
  /// call.
  virtual void SetReceiveFunction(EndpointDataFunction) = 0;

 private:
};
}  // namespace pw::usb_device
