#include "pw_bluetooth_sapphire_mcuxpresso/low_energy_advertiser.h"

#include "bluetooth/addr.h"
#include "bluetooth/bluetooth.h"

namespace bt::hci {

void LowEnergyAdvertiserMimxrt595::StartAdvertising(
    const DeviceAddress& address,
    const AdvertisingData& data,
    const AdvertisingData& scan_rsp,
    const AdvertisingOptions& options,
    ConnectionCallback connect_callback,
    ResultFunction<> result_callback) {
  (void)address;
  (void)data;
  (void)scan_rsp;
  (void)options;
  (void)connect_callback;
  (void)result_callback;
  // TODO: support setting address
  // bt_addr_t addr;
  // std::copy(address.value().bytes().cbegin(),
  //          address.value().bytes().cend(),
  //          addr.val);
  // bt_addr_le_t peer{.type = BT_ADDR_LE_RANDOM, .a = addr};
  // if (address.type() == DeviceAddress::Type::kLEPublic) {
  //  peer.type = BT_ADDR_LE_PUBLIC_ID;
  //}
  // if (address.type() == DeviceAddress::Type::kBREDR) {
  //  peer.type = BT_ADDR_LE_RANDOM_ID;
  //} else {
  //  PW_LOG_CRITICAL("Unhandled address type");
  //}
  struct bt_le_adv_param params{.id = BT_ID_DEFAULT,
                                .sid = 0,
                                .secondary_max_skip = 0,
                                .options = options.flags,
                                .interval_min = options.interval.min(),
                                .interval_max = options.interval.max(),
                                .peer = (NULL)};
  //.peer = &peer};
  std::vector<struct bt_data> ad;
  for (const auto& company_id : data.manufacturer_data_ids()) {
    // TODO: check if data.size fits uint8_t
    ad.push_back(
        {.type = BT_DATA_MANUFACTURER_DATA,
         .data_len = (uint8_t)data.manufacturer_data(company_id).size(),
         .data = data.manufacturer_data(company_id).data()});
  }
  std::optional<AdvertisingData::LocalName> local_name = data.local_name();
  AdvertisingData::LocalName name =
      local_name.value_or(AdvertisingData::LocalName("unknown", true));
  // TODO: check if name.name.length fits uint8_t
  struct bt_data sd{.type = BT_DATA_NAME_COMPLETE,
                    .data_len = (uint8_t)name.name.length(),
                    .data = (uint8_t*)name.name.data()};
  auto err = bt_le_adv_start(&params, &ad[0], 1, &sd, 1);
  if (err) {
    PW_LOG_CRITICAL("Advertising failed to start (err %d)", err);
  }

  // TODO: Handle connect callback, result callback
}

// Stops advertisement on all currently advertising addresses. Idempotent and
// asynchronous.
void LowEnergyAdvertiserMimxrt595::StopAdvertising() { bt_le_adv_stop(); }

// Stops any advertisement currently active on |address|. Idempotent and
// asynchronous.
void LowEnergyAdvertiserMimxrt595::StopAdvertising(const DeviceAddress& address,
                                                   bool extended_pdu) {
  (void)address;
  (void)extended_pdu;
  PW_LOG_CRITICAL("UNIMPLEMENTED: StopAdvertising");
}

// Callback for an incoming LE connection. This function should be called in
// reaction to any connection that was not initiated locally. This object will
// determine if it was a result of an active advertisement and route the
// connection accordingly.
void LowEnergyAdvertiserMimxrt595::OnIncomingConnection(
    hci_spec::ConnectionHandle handle,
    pw::bluetooth::emboss::ConnectionRole role,
    const DeviceAddress& peer_address,
    const hci_spec::LEConnectionParameters& conn_params) {
  (void)handle;
  (void)role;
  (void)peer_address;
  (void)conn_params;
  PW_LOG_CRITICAL("UNIMPLEMENTED: OnIncomingConnection");
}

// Returns the maximum number of advertisements that can be supported
size_t LowEnergyAdvertiserMimxrt595::MaxAdvertisements() const {
  PW_LOG_CRITICAL("UNIMPLEMENTED: MaxAdvertisements");
  return 100;
}

// Build the HCI command packet to enable advertising for the flavor of low
// energy advertising being implemented.
EmbossCommandPacket LowEnergyAdvertiserMimxrt595::BuildEnablePacket(
    const DeviceAddress& address,
    pw::bluetooth::emboss::GenericEnableParam enable,
    bool extended_pdu) {
  PW_LOG_CRITICAL("UNIMPLEMENTED: BuildEnablePacket");
  (void)address;
  (void)enable;
  (void)extended_pdu;
  return EmbossCommandPacket::New<
      pw::bluetooth::emboss::DisconnectCommandWriter>(hci_spec::kDisconnect);
}

// Build the HCI command packet to set the advertising parameters for the
// flavor of low energy advertising being implemented.
std::optional<EmbossCommandPacket>
LowEnergyAdvertiserMimxrt595::BuildSetAdvertisingParams(
    const DeviceAddress& address,
    const AdvertisingEventProperties& properties,
    pw::bluetooth::emboss::LEOwnAddressType own_address_type,
    const AdvertisingIntervalRange& interval,
    bool extended_pdu) {
  (void)address;
  (void)properties;
  (void)own_address_type;
  (void)interval;
  (void)extended_pdu;
  PW_LOG_CRITICAL("UNIMPLEMENTED: BuildSetAdvertisingParams");
  return std::nullopt;
}

// Build the HCI command packet to set the advertising data for the flavor of
// low energy advertising being implemented.
std::vector<EmbossCommandPacket>
LowEnergyAdvertiserMimxrt595::BuildSetAdvertisingData(
    const DeviceAddress& address,
    const AdvertisingData& data,
    AdvFlags flags,
    bool extended_pdu) {
  (void)address;
  (void)data;
  (void)flags;
  (void)extended_pdu;
  PW_LOG_CRITICAL("UNIMPLEMENTED: BuildSetAdvertisingData");
  return std::vector<EmbossCommandPacket>();
}

// Build the HCI command packet to delete the advertising parameters from the
// controller for the flavor of low energy advertising being implemented. This
// method is used when stopping an advertisement.
EmbossCommandPacket LowEnergyAdvertiserMimxrt595::BuildUnsetAdvertisingData(
    const DeviceAddress& address, bool extended_pdu) {
  (void)address;
  (void)extended_pdu;
  PW_LOG_CRITICAL("UNIMPLEMENTED: BuildUnsetAdvertisingData");
  return EmbossCommandPacket::New<
      pw::bluetooth::emboss::DisconnectCommandWriter>(hci_spec::kDisconnect);
}

// Build the HCI command packet to set the data sent in a scan response (if
// requested) for the flavor of low energy advertising being implemented.
std::vector<EmbossCommandPacket>
LowEnergyAdvertiserMimxrt595::BuildSetScanResponse(
    const DeviceAddress& address,
    const AdvertisingData& scan_rsp,
    bool extended_pdu) {
  (void)address;
  (void)scan_rsp;
  (void)extended_pdu;
  PW_LOG_CRITICAL("UNIMPLEMENTED: BuildSetScanResponse");
  std::vector<EmbossCommandPacket> packets;
  packets.push_back(
      EmbossCommandPacket::New<pw::bluetooth::emboss::DisconnectCommandWriter>(
          hci_spec::kDisconnect));
  return packets;
}

// Build the HCI command packet to delete the advertising parameters from the
// controller for the flavor of low energy advertising being implemented.
EmbossCommandPacket LowEnergyAdvertiserMimxrt595::BuildUnsetScanResponse(
    const DeviceAddress& address, bool extended_pdu) {
  (void)address;
  (void)extended_pdu;
  PW_LOG_CRITICAL("UNIMPLEMENTED: BuildUnsetScanResponse");
  return EmbossCommandPacket::New<
      pw::bluetooth::emboss::DisconnectCommandWriter>(hci_spec::kDisconnect);
}

// Build the HCI command packet to remove the advertising set entirely from
// the controller's memory for the flavor of low energy advertising being
// implemented.
EmbossCommandPacket LowEnergyAdvertiserMimxrt595::BuildRemoveAdvertisingSet(
    const DeviceAddress& address, bool extended_pdu) {
  (void)address;
  (void)extended_pdu;
  PW_LOG_CRITICAL("UNIMPLEMENTED: BuildRemoveAdvertisingSet");
  return EmbossCommandPacket::New<
      pw::bluetooth::emboss::DisconnectCommandWriter>(hci_spec::kDisconnect);
}
bool LowEnergyAdvertiserMimxrt595::AllowsRandomAddressChange() const {
  PW_LOG_CRITICAL("UNIMPLEMENTED: AllowsRandomAddressChange");
  return false;
}
};  // namespace bt::hci
