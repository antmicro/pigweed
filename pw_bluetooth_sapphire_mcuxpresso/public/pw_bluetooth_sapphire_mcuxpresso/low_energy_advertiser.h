#pragma once

#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_advertiser.h"

namespace bt::hci {

class LowEnergyAdvertiserMimxrt595 final : public LowEnergyAdvertiser {
 public:
  explicit LowEnergyAdvertiserMimxrt595(hci::Transport::WeakPtrType hci,
                                        uint16_t max_advertising_data_length)
      : LowEnergyAdvertiser(std::move(hci), max_advertising_data_length) {}

  ~LowEnergyAdvertiserMimxrt595() = default;

  void StartAdvertising(const DeviceAddress& address,
                        const AdvertisingData& data,
                        const AdvertisingData& scan_rsp,
                        const AdvertisingOptions& options,
                        ConnectionCallback connect_callback,
                        ResultFunction<> result_callback) override;

  // Stops advertisement on all currently advertising addresses. Idempotent and
  // asynchronous.
  void StopAdvertising();

  // Stops any advertisement currently active on |address|. Idempotent and
  // asynchronous.
  void StopAdvertising(const DeviceAddress& address,
                       bool extended_pdu) override;

  // Callback for an incoming LE connection. This function should be called in
  // reaction to any connection that was not initiated locally. This object will
  // determine if it was a result of an active advertisement and route the
  // connection accordingly.
  void OnIncomingConnection(
      hci_spec::ConnectionHandle handle,
      pw::bluetooth::emboss::ConnectionRole role,
      const DeviceAddress& peer_address,
      const hci_spec::LEConnectionParameters& conn_params) override;

  // Returns the maximum number of advertisements that can be supported
  size_t MaxAdvertisements() const override;

 protected:
  // Build the HCI command packet to enable advertising for the flavor of low
  // energy advertising being implemented.
  EmbossCommandPacket BuildEnablePacket(
      const DeviceAddress& address,
      pw::bluetooth::emboss::GenericEnableParam enable,
      bool extended_pdu) override;

  // Build the HCI command packet to set the advertising parameters for the
  // flavor of low energy advertising being implemented.
  std::optional<EmbossCommandPacket> BuildSetAdvertisingParams(
      const DeviceAddress& address,
      const AdvertisingEventProperties& properties,
      pw::bluetooth::emboss::LEOwnAddressType own_address_type,
      const AdvertisingIntervalRange& interval,
      bool extended_pdu) override;

  // Build the HCI command packet to set the advertising data for the flavor of
  // low energy advertising being implemented.
  std::vector<EmbossCommandPacket> BuildSetAdvertisingData(
      const DeviceAddress& address,
      const AdvertisingData& data,
      AdvFlags flags,
      bool extended_pdu) override;

  // Build the HCI command packet to delete the advertising parameters from the
  // controller for the flavor of low energy advertising being implemented. This
  // method is used when stopping an advertisement.
  EmbossCommandPacket BuildUnsetAdvertisingData(const DeviceAddress& address,
                                                bool extended_pdu) override;

  // Build the HCI command packet to set the data sent in a scan response (if
  // requested) for the flavor of low energy advertising being implemented.
  std::vector<EmbossCommandPacket> BuildSetScanResponse(
      const DeviceAddress& address,
      const AdvertisingData& scan_rsp,
      bool extended_pdu) override;

  // Build the HCI command packet to delete the advertising parameters from the
  // controller for the flavor of low energy advertising being implemented.
  EmbossCommandPacket BuildUnsetScanResponse(const DeviceAddress& address,
                                             bool extended_pdu) override;

  // Build the HCI command packet to remove the advertising set entirely from
  // the controller's memory for the flavor of low energy advertising being
  // implemented.
  EmbossCommandPacket BuildRemoveAdvertisingSet(const DeviceAddress& address,
                                                bool extended_pdu) override;

  // Called when the command packet created with BuildSetAdvertisingParams
  // returns with a result
  void OnSetAdvertisingParamsComplete(const EventPacket& event) {}

  // Called when a sequence of HCI commands that form a single operation (e.g.
  // start advertising, stop advertising) completes in its entirety. Subclasses
  // can override this method to be notified when the HCI command runner is
  // available once again.
  void OnCurrentOperationComplete() {}

  bool AllowsRandomAddressChange() const override;

 private:
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyAdvertiserMimxrt595);
};

}  // namespace bt::hci
