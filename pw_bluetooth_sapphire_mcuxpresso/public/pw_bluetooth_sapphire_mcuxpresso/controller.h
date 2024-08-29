#pragma once

#include "pw_bluetooth/controller.h"

namespace pw::bluetooth {

class Mimxrt595Controller final : public pw::bluetooth::Controller {

  // Sets a function that will be called with HCI event packets received from
  // the controller. This should  be called before `Initialize` or else incoming
  // packets will be dropped. The lifetime of data passed to `func` is only
  // guaranteed for the lifetime of the function call.
  void SetEventFunction(DataFunction func) override;

  // Sets a function that will be called with ACL data packets received from the
  // controller. This should be called before `Initialize` or else incoming
  // packets will be dropped. The lifetime of data passed to `func` is only
  // guaranteed for the lifetime of the function call.
  void SetReceiveAclFunction(DataFunction func) override;

  // Sets a function that will be called with SCO packets received from the
  // controller. On Classic and Dual Mode stacks, this should be called before
  // `Initialize` or else incoming packets will be dropped. The lifetime of data
  // passed to `func` is only guaranteed for the lifetime of the function call.
  void SetReceiveScoFunction(DataFunction func) override;

  // Sets a function that will be called with ISO packets received from the
  // controller. This should be called before an ISO data path is setup or else
  // incoming packets may be dropped. The lifetime of data passed to `func` is
  // only guaranteed for the lifetime of the function call.
  void SetReceiveIsoFunction(DataFunction /*func*/) {}

  // Initializes the controller interface and starts processing packets.
  // `complete_callback` will be called with the result of initialization.
  // `error_callback` will be called for fatal errors that occur after
  // initialization. After a fatal error, this object is invalid. `Close` should
  // be called to ensure a safe clean up.
  void Initialize(Callback<void(Status)> complete_callback,
                          Callback<void(Status)> error_callback) override;

  // Closes the controller interface, resetting all state. `callback` will be
  // called when closure is complete. After this method is called, this object
  // should be considered invalid and no other methods should be called
  // (including `Initialize`).
  // `callback` will be called with:
  // OK - the controller interface was successfully closed, or is already closed
  // INTERNAL - the controller interface could not be closed
  void Close(Callback<void(Status)> callback) override;

  // Sends an HCI command packet to the controller.
  void SendCommand(span<const std::byte> command) override;

  // Sends an ACL data packet to the controller.
  void SendAclData(span<const std::byte> data) override;

  // Sends a SCO data packet to the controller.
  void SendScoData(span<const std::byte> data) override;

  // Sends an ISO data packet to the controller.
  void SendIsoData(span<const std::byte> /*data*/) {}

  // Configure the HCI for a SCO connection with the indicated parameters.
  // `SetReceiveScoFunction` must be called before calling this method.
  // `callback will be called with:
  // OK - success, packets can be sent/received.
  // UNIMPLEMENTED - the implementation/controller does not support SCO over HCI
  // ALREADY_EXISTS - a SCO connection is already configured
  // INTERNAL - an internal error occurred
  void ConfigureSco(ScoCodingFormat coding_format,
                            ScoEncoding encoding,
                            ScoSampleRate sample_rate,
                            Callback<void(Status)> callback) override;

  // Releases the resources held by an active SCO connection. This should be
  // called when a SCO connection is closed. `ConfigureSco` must be called
  // before calling this method.
  // `callback will be called with:
  // OK - success, the SCO configuration was reset.
  // UNIMPLEMENTED - the implementation/controller does not support SCO over HCI
  // INTERNAL - an internal error occurred
  void ResetSco(Callback<void(Status)> callback) override;

  // Calls `callback` with a bitmask of features supported by the controller.
  void GetFeatures(Callback<void(FeaturesBits)> callback) override;

  // Encodes the vendor command indicated by `parameters`.
  // `callback` will be called with the result of the encoding request.
  // The lifetime of data passed to `callback` is only guaranteed for the
  // lifetime of the function call.
  void EncodeVendorCommand(
      VendorCommandParameters parameters,
      Callback<void(Result<span<const std::byte>>)> callback) override;
private:
    void bt_ready_cb(int err);

    Callback<void(Status)> initialize_complete_callback_;
    Callback<void(Status)> initialize_error_callback_;

};

}
