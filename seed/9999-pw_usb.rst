-------
Summary
-------
Platform-agnostic interface for interacting with USB bus interfaces.
This proposal only defines the framework for device interactions, i.e.
when the target device appears as a peripheral when connected to a
different host device. This does not define any functionality related
to USB host.

----------
Motivation
----------
Pigweed already provides an abstract interface for interacting with Bluetooth
controllers via the `pw_bluetooth` module. No such interface exists for USB
however. The ubiquity of the interface makes it an attractive target for
enhancing the functionality of products. However, support for USB is often
heavily platform-specific and unportable. Vendor-provided USB samples for
different boards/targets are often heavily tied to the board's SDK, making
porting functionality built on top of such stacks harder. This proposal aims
to reduce this difficulty by providing platform-agnostic interfaces to the
USB Bus Interface (defined in: USB Specification Rev2.0, Chapter 9) layer of
a USB device.

-----------------
Areas of Interest
-----------------

The USB Device Framework can be divided into three main layers:

#. Interfacing with the underlying hardware controller (USB Bus Interface)
   to transmit and receive packets

#. Routing data received from the USB controller and logical endpoints of
   the device

#. High-level functionality implemented by various USB classes

See also: Universal Serial Bus Specification Revision 2.0, Chapter 9: USB Device
Framework.

This proposal only concerns itself with the very first layer of the USB Device
Framework, e.g access to the physical medium.

Layer 2 is responsible for fundamental data flow between the lower and higher layers
of the USB Device Framework. This layer is responsible for handling standard
USB Device Requests which all specification-compliant USB devices must support.
Routing of data between various USB endpoints (logical sources/sinks of data) happens
here.

Layer 3 utilizes the data flow model provided by Layer 2 to implement the target
functionality of the device. This can be for example:

* USB Human Interface Device

* USB Communications Device Class

* USB Mass Storage Class

* ...

This proposal concerns itself only with the lowest layer - **Layer 1** - of the
Device Framework. The higher layers are usually not as coupled to the underlying
platform compared to access to the medium, and as such they will not be considered.
Pigweed implementation of the higher layers is left as a potential area of
improvement for future proposals.

-----------------
Proposal (Design)
-----------------

The following `pw::usb_device::Controller` interface would allow for generic reading
and writing from USB endpoints using a given USB controller.
As USB host support is not a concern at this time, the `pw::usb_device` namespace is
used to clarify that this can only be used for implementing an USB peripheral device.

.. code-block:: c++

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


.. code-block:: c++

    namespace pw::usb_device {
    /// Represents an endpoint number
    /// The USB specification allows for 16 unique endpoints.
    using EndpointNumber = uint8_t;

    /// Represents the address of an USB endpoint
    /// The bit format of the address is as follows:
    /// Bit   | 7 6 5 4 3 2 1 0
    /// Value | D 0 0 0 N N N N
    /// Where D represents the direction of the endpoint (0 - OUT, 1 - IN)
    /// and N the endpoint number. As such, 32 endpoints can be addressed,
    /// or 16 pipes - each one consisting of an IN and OUT endpoint.
    /// The direction is host-centric, so OUT means data outgoing from
    /// a host to the device, while IN signifies data inbound from device
    /// to the host.
    using EndpointAddress = uint8_t;
    }  // namespace pw::usb_device


.. code-block:: c++

    namespace pw::usb_device {
    enum class Event {
    BusReset,
    Detach,
    Attach,
    };
    }
