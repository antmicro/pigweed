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

#include "pw_bluetooth_sapphire/internal/host/hci/legacy_low_energy_scanner.h"

#include <pw_preprocessor/compiler.h>

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_report_parser.h"

namespace bt::hci {
namespace pwemb = pw::bluetooth::emboss;

LegacyLowEnergyScanner::LegacyLowEnergyScanner(
    LocalAddressDelegate* local_addr_delegate,
    Transport::WeakPtrType transport,
    pw::async::Dispatcher& pw_dispatcher)
    : LowEnergyScanner(
          local_addr_delegate, std::move(transport), pw_dispatcher),
      weak_self_(this) {
  auto self = weak_self_.GetWeakPtr();
  event_handler_id_ = hci()->command_channel()->AddLEMetaEventHandler(
      hci_spec::kLEAdvertisingReportSubeventCode,
      [self](const EmbossEventPacket& event) {
        if (!self.is_alive()) {
          return hci::CommandChannel::EventCallbackResult::kRemove;
        }

        self->OnAdvertisingReportEvent(event);
        return hci::CommandChannel::EventCallbackResult::kContinue;
      });
}

LegacyLowEnergyScanner::~LegacyLowEnergyScanner() {
  // This object is probably being destroyed because the stack is shutting down,
  // in which case the HCI layer may have already been destroyed.
  if (!hci().is_alive() || !hci()->command_channel()) {
    return;
  }

  hci()->command_channel()->RemoveEventHandler(event_handler_id_);
  StopScan();
}

bool LegacyLowEnergyScanner::StartScan(const ScanOptions& options,
                                       ScanStatusCallback callback) {
  BT_ASSERT(options.interval >= hci_spec::kLEScanIntervalMin);
  BT_ASSERT(options.interval <= hci_spec::kLEScanIntervalMax);
  BT_ASSERT(options.window >= hci_spec::kLEScanIntervalMin);
  BT_ASSERT(options.window <= hci_spec::kLEScanIntervalMax);
  return LowEnergyScanner::StartScan(options, std::move(callback));
}

EmbossCommandPacket LegacyLowEnergyScanner::BuildSetScanParametersPacket(
    const DeviceAddress& local_address, const ScanOptions& options) {
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetScanParametersCommandWriter>(
      hci_spec::kLESetScanParameters);
  auto params = packet.view_t();

  params.le_scan_type().Write(pw::bluetooth::emboss::LEScanType::PASSIVE);
  if (options.active) {
    params.le_scan_type().Write(pw::bluetooth::emboss::LEScanType::ACTIVE);
  }

  params.le_scan_interval().Write(options.interval);
  params.le_scan_window().Write(options.window);
  params.scanning_filter_policy().Write(options.filter_policy);

  if (local_address.type() == DeviceAddress::Type::kLERandom) {
    params.own_address_type().Write(
        pw::bluetooth::emboss::LEOwnAddressType::RANDOM);
  } else {
    params.own_address_type().Write(
        pw::bluetooth::emboss::LEOwnAddressType::PUBLIC);
  }

  return packet;
}

EmbossCommandPacket LegacyLowEnergyScanner::BuildEnablePacket(
    const ScanOptions& options,
    pw::bluetooth::emboss::GenericEnableParam enable) {
  auto packet = EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetScanEnableCommandWriter>(
      hci_spec::kLESetScanEnable);
  auto params = packet.view_t();
  params.le_scan_enable().Write(enable);

  params.filter_duplicates().Write(
      pw::bluetooth::emboss::GenericEnableParam::DISABLE);
  if (options.filter_duplicates) {
    params.filter_duplicates().Write(
        pw::bluetooth::emboss::GenericEnableParam::ENABLE);
  }

  return packet;
}

void LegacyLowEnergyScanner::HandleScanResponse(const DeviceAddress& address,
                                                bool resolved,
                                                int8_t rssi,
                                                const ByteBuffer& data) {
  std::unique_ptr<PendingScanResult> pending = RemovePendingResult(address);
  if (!pending) {
    bt_log(DEBUG, "hci-le", "dropping unmatched scan response");
    return;
  }

  BT_DEBUG_ASSERT(address == pending->result().address());
  pending->result().AppendData(data);
  pending->result().set_resolved(resolved);
  pending->result().set_rssi(rssi);

  delegate()->OnPeerFound(pending->result());

  // The callback handler may stop the scan, destroying objects within the
  // LowEnergyScanner. Avoid doing anything more to prevent use after free
  // bugs.
}

// Extract all advertising reports from a given HCI LE Advertising Report event
std::vector<pw::bluetooth::emboss::LEAdvertisingReportDataView>
LegacyLowEnergyScanner::ParseAdvertisingReports(
    const EmbossEventPacket& event) {
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);
  BT_DEBUG_ASSERT(event.view<pw::bluetooth::emboss::LEMetaEventView>()
                      .subevent_code()
                      .Read() == hci_spec::kLEAdvertisingReportSubeventCode);

  auto params =
      event.view<pw::bluetooth::emboss::LEAdvertisingReportSubeventView>();
  uint8_t num_reports = params.num_reports().Read();
  std::vector<pw::bluetooth::emboss::LEAdvertisingReportDataView> reports;
  reports.reserve(num_reports);

  size_t bytes_read = 0;
  while (bytes_read < params.reports().BackingStorage().SizeInBytes()) {
    size_t min_size =
        pw::bluetooth::emboss::LEAdvertisingReportData::MinSizeInBytes();
    auto report_prefix = pw::bluetooth::emboss::MakeLEAdvertisingReportDataView(
        params.reports().BackingStorage().begin() + bytes_read, min_size);

    uint8_t data_length = report_prefix.data_length().Read();
    size_t actual_size = min_size + data_length;

    size_t bytes_left =
        params.reports().BackingStorage().SizeInBytes() - bytes_read;
    if (actual_size > bytes_left) {
      bt_log(WARN,
             "hci-le",
             "parsing advertising reports, next report size %zu bytes, but "
             "only %zu bytes left",
             actual_size,
             bytes_left);
      break;
    }

    auto report = pw::bluetooth::emboss::MakeLEAdvertisingReportDataView(
        params.reports().BackingStorage().begin() + bytes_read, actual_size);
    reports.push_back(report);

    bytes_read += actual_size;
  }

  return reports;
}

// Returns a DeviceAddress and whether or not that DeviceAddress has been
// resolved
static std::tuple<DeviceAddress, bool> BuildDeviceAddress(
    pw::bluetooth::emboss::LEAddressType report_type,
    pw::bluetooth::emboss::BdAddrView address_view) {
  std::optional<DeviceAddress::Type> address_type =
      DeviceAddress::LeAddrToDeviceAddr(report_type);
  BT_DEBUG_ASSERT(address_type);

  bool resolved = false;
  switch (report_type) {
    case pw::bluetooth::emboss::LEAddressType::PUBLIC_IDENTITY:
    case pw::bluetooth::emboss::LEAddressType::RANDOM_IDENTITY:
      resolved = true;
      break;
    case pw::bluetooth::emboss::LEAddressType::PUBLIC:
    case pw::bluetooth::emboss::LEAddressType::RANDOM:
      resolved = false;
      break;
  }

  DeviceAddress address =
      DeviceAddress(*address_type, DeviceAddressBytes(address_view));
  return std::make_tuple(address, resolved);
}

void LegacyLowEnergyScanner::OnAdvertisingReportEvent(
    const EmbossEventPacket& event) {
  if (!IsScanning()) {
    return;
  }

  std::vector<pw::bluetooth::emboss::LEAdvertisingReportDataView> reports =
      ParseAdvertisingReports(event);

  for (pw::bluetooth::emboss::LEAdvertisingReportDataView report : reports) {
    if (report.data_length().Read() > hci_spec::kMaxLEAdvertisingDataLength) {
      bt_log(WARN, "hci-le", "advertising data too long! Ignoring");
      continue;
    }

    const auto& [address, resolved] =
        BuildDeviceAddress(report.address_type().Read(), report.address());

    bool needs_scan_rsp = false;
    bool connectable = false;
    bool directed = false;

    PW_MODIFY_DIAGNOSTICS_PUSH();
    PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
    switch (report.event_type().Read()) {
      case pwemb::LEAdvertisingEventType::CONNECTABLE_DIRECTED: {
        directed = true;
        break;
      }
      case pwemb::LEAdvertisingEventType::
          CONNECTABLE_AND_SCANNABLE_UNDIRECTED: {
        connectable = true;
        [[fallthrough]];
      }
      case pwemb::LEAdvertisingEventType::SCANNABLE_UNDIRECTED: {
        if (IsActiveScanning()) {
          needs_scan_rsp = true;
        }
        break;
      }
      case pwemb::LEAdvertisingEventType::SCAN_RESPONSE: {
        if (IsActiveScanning()) {
          BufferView data = BufferView(report.data().BackingStorage().data(),
                                       report.data_length().Read());
          HandleScanResponse(address, resolved, report.rssi().Read(), data);
        }
        continue;
      }
      default: {
        break;
      }
    }
    PW_MODIFY_DIAGNOSTICS_POP();

    LowEnergyScanResult result(address, resolved, connectable);
    result.AppendData(BufferView(report.data().BackingStorage().data(),
                                 report.data_length().Read()));
    result.set_rssi(report.rssi().Read());

    if (directed) {
      delegate()->OnDirectedAdvertisement(result);
      continue;
    }

    if (!needs_scan_rsp) {
      delegate()->OnPeerFound(result);
      continue;
    }

    AddPendingResult(std::move(result));
  }
}

}  // namespace bt::hci
