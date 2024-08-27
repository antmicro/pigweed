// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream_manager.h"

#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::iso {

// Connection handles, must be in the range [0x0000, 0x0eff]
constexpr hci_spec::ConnectionHandle kAclConnectionHandleId1 = 0x123;
constexpr hci_spec::ConnectionHandle kAclConnectionHandleId2 = 0x222;
constexpr hci_spec::ConnectionHandle kCisHandleId = 0xe09;

constexpr hci_spec::CigIdentifier kCigId = 0x11;
constexpr hci_spec::CisIdentifier kCisId = 0x18;
const bt::iso::CigCisIdentifier kCigCisId(kCigId, kCisId);

using MockControllerTestBase =
    bt::testing::FakeDispatcherControllerTest<bt::testing::MockController>;

class IsoStreamManagerTest : public MockControllerTestBase {
 public:
  IsoStreamManagerTest() = default;
  ~IsoStreamManagerTest() override = default;

  IsoStreamManager* iso_stream_manager() { return iso_stream_manager_.get(); }

  void SetUp() override {
    MockControllerTestBase::SetUp();
    iso_stream_manager_ = std::make_unique<IsoStreamManager>(
        kAclConnectionHandleId1, cmd_channel()->AsWeakPtr());
  }

  AcceptCisStatus CallAcceptCis(CigCisIdentifier id,
                                bool* cb_invoked = nullptr) {
    AcceptCisStatus status;
    if (cb_invoked) {
      *cb_invoked = false;
    }
    status = iso_stream_manager_->AcceptCis(
        id,
        [id, cb_invoked, this](
            pw::bluetooth::emboss::StatusCode status,
            std::optional<WeakSelf<IsoStream>::WeakPtrType> iso_weak_ptr,
            const std::optional<CisEstablishedParameters>& cis_parameters) {
          if (cb_invoked) {
            *cb_invoked = true;
          }
          if (iso_weak_ptr.has_value()) {
            iso_streams_[id] = (*iso_weak_ptr);
          }
        });
    return status;
  }

 protected:
  std::unordered_map<bt::iso::CigCisIdentifier, WeakSelf<IsoStream>::WeakPtrType>
      iso_streams_;

 private:
  std::unique_ptr<IsoStreamManager> iso_stream_manager_;
};

// Verify that we ignore a CIS request whose ACL connection handle doesn't match
// ours.
TEST_F(IsoStreamManagerTest, IgnoreIncomingWrongConnection) {
  DynamicByteBuffer request_packet = testing::LECisRequestEventPacket(
      kAclConnectionHandleId2, kCisHandleId, kCigId, kCisId);
  test_device()->SendCommandChannelPacket(request_packet);
}

// Verify that we reject a CIS request whose ACL connection handle matches ours,
// but that we are not waiting for.
TEST_F(IsoStreamManagerTest, RejectIncomingConnection) {
  const auto le_reject_cis_packet = testing::LERejectCisRequestCommandPacket(
      kCisHandleId, pw::bluetooth::emboss::StatusCode::UNSPECIFIED_ERROR);
  EXPECT_CMD_PACKET_OUT(test_device(), le_reject_cis_packet);

  DynamicByteBuffer request_packet = testing::LECisRequestEventPacket(
      kAclConnectionHandleId1, kCisHandleId, kCigId, kCisId);
  test_device()->SendCommandChannelPacket(request_packet);
}

static DynamicByteBuffer LECisEstablishedPacketWithDefaultValues(
    hci_spec::ConnectionHandle connection_handle) {
  return testing::LECisEstablishedEventPacket(
      pw::bluetooth::emboss::StatusCode::SUCCESS,
      connection_handle,
      /*cig_sync_delay_us=*/0x123456,  // Must be in [0x0000ea, 0x7fffff]
      /*cis_sync_delay_us=*/0x7890ab,  // Must be in [0x0000ea, 0x7fffff]
      /*transport_latency_c_to_p_us=*/0x654321,  // Must be in [0x0000ea,
                                                 // 0x7fffff]
      /*transport_latency_p_to_c_us=*/0x0fedcb,  // Must be in [0x0000ea,
                                                 // 0x7fffff]
      /*phy_c_to_p=*/pw::bluetooth::emboss::IsoPhyType::LE_2M,
      /*phy_c_to_p=*/pw::bluetooth::emboss::IsoPhyType::LE_CODED,
      /*nse=*/0x10,               // Must be in [0x01, 0x1f]
      /*bn_c_to_p=*/0x05,         // Must be in [0x00, 0x0f]
      /*bn_p_to_c=*/0x0f,         // Must be in [0x00, 0x0f]
      /*ft_c_to_p=*/0x01,         // Must be in [0x01, 0xff]
      /*ft_p_to_c=*/0xff,         // Must be in [0x01, 0xff]
      /*max_pdu_c_to_p=*/0x0042,  // Must be in [0x0000, 0x00fb]
      /*max_pdu_p_to_c=*/0x00fb,  // Must be in [0x0000, 0x00fb]
      /*iso_interval=*/0x0222     // Must be in [0x0004, 0x0c80]
  );
}

// We should be able to wait on multiple CIS requests simultaneously, as long as
// they do not have the identical CIG/CIS values.
TEST_F(IsoStreamManagerTest, MultipleCISAcceptRequests) {
  const CigCisIdentifier kId1(0x14, 0x04);
  const CigCisIdentifier kId2(0x14, 0x05);

  // We should not be waiting on these connections
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId1));
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId2));

  // Start waiting
  bool cb1_invoked, cb2_invoked;
  EXPECT_EQ(CallAcceptCis(kId1, &cb1_invoked), AcceptCisStatus::kSuccess);
  ASSERT_TRUE(iso_stream_manager()->HandlerRegistered(kId1));
  EXPECT_EQ(CallAcceptCis(kId2, &cb2_invoked), AcceptCisStatus::kSuccess);
  ASSERT_TRUE(iso_stream_manager()->HandlerRegistered(kId2));

  // When a duplicate request comes in, we should decline it but continue to
  // wait
  EXPECT_EQ(CallAcceptCis(kId1), AcceptCisStatus::kAlreadyExists);
  ASSERT_TRUE(iso_stream_manager()->HandlerRegistered(kId1));

  // When a matching request arrives (for kId2), we should accept it and stop
  // waiting on it
  auto le_accept_cis_packet =
      testing::LEAcceptCisRequestCommandPacket(kCisHandleId);
  auto le_accept_cis_complete_packet = testing::CommandCompletePacket(
      hci_spec::kLEAcceptCISRequest,
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(), le_accept_cis_packet, &le_accept_cis_complete_packet);
  DynamicByteBuffer request_packet = testing::LECisRequestEventPacket(
      kAclConnectionHandleId1, kCisHandleId, kId2.cig_id(), kId2.cis_id());
  test_device()->SendCommandChannelPacket(request_packet);
  RunUntilIdle();
  ASSERT_TRUE(iso_stream_manager()->HandlerRegistered(kId1));
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId2));

  // Matching request arrives for kId1, accept it and stop waiting on it
  const hci_spec::ConnectionHandle kAltCisHandleId = kCisHandleId + 1;
  le_accept_cis_packet =
      testing::LEAcceptCisRequestCommandPacket(kAltCisHandleId);
  EXPECT_CMD_PACKET_OUT(
      test_device(), le_accept_cis_packet, &le_accept_cis_complete_packet);
  request_packet = testing::LECisRequestEventPacket(
      kAclConnectionHandleId1, kAltCisHandleId, kId1.cig_id(), kId1.cis_id());
  test_device()->SendCommandChannelPacket(request_packet);
  RunUntilIdle();
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId1));

  // Establish the stream and make sure the correct callback is invoked
  DynamicByteBuffer le_cis_established_packet =
      LECisEstablishedPacketWithDefaultValues(kCisHandleId);
  test_device()->SendCommandChannelPacket(le_cis_established_packet);
  RunUntilIdle();
  EXPECT_FALSE(cb1_invoked);
  EXPECT_TRUE(cb2_invoked);

  // Establish the stream and make sure the correct callback is invoked
  le_cis_established_packet =
      LECisEstablishedPacketWithDefaultValues(kAltCisHandleId);
  test_device()->SendCommandChannelPacket(le_cis_established_packet);
  RunUntilIdle();
  EXPECT_TRUE(cb1_invoked);

  // Because we've already accepted the request, we should disallow any requests
  // that have the same CIG/CIS as an existing stream
  EXPECT_EQ(CallAcceptCis(kId1), AcceptCisStatus::kAlreadyExists);
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId1));
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId2));

  // If we close out the stream associated with kId1, we should now be able to
  // start waiting again
  ASSERT_EQ(iso_streams_.count(kId1), 1u);
  iso_streams_[kId1]->Close();
  EXPECT_EQ(CallAcceptCis(kId1), AcceptCisStatus::kSuccess);
  ASSERT_TRUE(iso_stream_manager()->HandlerRegistered(kId1));
}

}  // namespace bt::iso
