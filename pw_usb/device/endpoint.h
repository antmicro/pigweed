#pragma once

#include <cstdint>
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
