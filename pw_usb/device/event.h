#pragma once

namespace pw::usb_device {
enum class Event {
  BusReset,
  Detach,
  Attach,
};
}
