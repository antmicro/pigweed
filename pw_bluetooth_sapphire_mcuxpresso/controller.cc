#include "pw_bluetooth_sapphire_mcuxpresso/controller.h"

#include <functional>
#include <utility>

#include "bluetooth/bluetooth.h"
#include "bluetooth/hci.h"
#include "pw_bluetooth/controller.h"

template <typename T>
struct CallbackCaster;

template <typename Ret, typename... Params>
struct CallbackCaster<Ret(Params...)> {
  template <typename... Args>
  static Ret callback(Args... args) {
    return func(args...);
  }
  static std::function<Ret(Params...)> func;
};

template <typename Ret, typename... Params>
std::function<Ret(Params...)> CallbackCaster<Ret(Params...)>::func;

typedef void (*callback_t)(int);

namespace pw::bluetooth {
void Mimxrt595Controller::SetEventFunction(DataFunction func) {
    (void)func;
}

void Mimxrt595Controller::SetReceiveAclFunction(DataFunction func) {
    (void)func;
}

void Mimxrt595Controller::SetReceiveScoFunction(DataFunction func) {
    (void)func;
}

// void Mimxrt595Controller::SetReceiveIsoFunction(DataFunction /*func*/) {}

void Mimxrt595Controller::Initialize(Callback<void(Status)> complete_callback,
                                     Callback<void(Status)> error_callback) {
  initialize_complete_callback_ = std::move(complete_callback);
  initialize_error_callback_ = std::move(error_callback);

  CallbackCaster<void(int)>::func =
      std::bind(&Mimxrt595Controller::bt_ready_cb, this, std::placeholders::_1);
  callback_t func =
      static_cast<callback_t>(CallbackCaster<void(int)>::callback);

  bt_enable(func);
}

void Mimxrt595Controller::Close(Callback<void(Status)> callback) {
  // MIMXRT595 SDK currently doesn't implement a close function
  // so we just call the callback with OK
  callback(Status());
}

void Mimxrt595Controller::SendCommand(span<const std::byte> command) {
  uint16_t opcode = (uint16_t)(command[0]) | (uint16_t)(command[1] << 8);
  struct net_buf* buf =
      bt_hci_cmd_create(opcode, sizeof(std::byte) * command.size() - 2);
  std::byte* payload =
      (std::byte*)net_buf_add(buf, sizeof(std::byte) * command.size() - 2);
  std::copy(command.begin() + 2, command.end(), payload);

  bt_hci_cmd_send(opcode, buf);
}

void Mimxrt595Controller::SendAclData(span<const std::byte> data) {
    (void)data;
}

void Mimxrt595Controller::SendScoData(span<const std::byte> data) {
    (void)data;
}

// void Mimxrt595Controller::SendIsoData(span<const std::byte> /*data*/) {}

void Mimxrt595Controller::ConfigureSco(ScoCodingFormat coding_format,
                                       ScoEncoding encoding,
                                       ScoSampleRate sample_rate,
                                       Callback<void(Status)> callback) {
    (void)coding_format;
    (void)encoding;
    (void)sample_rate;
    (void)callback;
}

void Mimxrt595Controller::ResetSco(Callback<void(Status)> callback) {
    (void)callback;
}

void Mimxrt595Controller::GetFeatures(Callback<void(FeaturesBits)> callback) {
  constexpr FeaturesBits features =
      FeaturesBits::kHciSco | FeaturesBits::kHciIso;
  callback(features);
}

void Mimxrt595Controller::EncodeVendorCommand(
    VendorCommandParameters parameters,
    Callback<void(Result<span<const std::byte>>)> callback) {
    (void)parameters;
    (void)callback;
}

void Mimxrt595Controller::bt_ready_cb(int err) {
  if (err == 0) {
    initialize_complete_callback_(Status());
  } else {
    initialize_error_callback_(Status::Unknown());
  }
}
};  // namespace pw::bluetooth
