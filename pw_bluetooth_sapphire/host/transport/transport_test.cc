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

#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/inspect.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

namespace bt::hci {

namespace {

using TransportTest =
    bt::testing::FakeDispatcherControllerTest<bt::testing::MockController>;
using TransportDeathTest = TransportTest;

TEST_F(TransportTest,
       CommandChannelTimeoutShutsDownChannelAndNotifiesClosedCallback) {
  CommandChannel::WeakPtrType cmd_chan_weak = cmd_channel()->AsWeakPtr();

  size_t closed_cb_count = 0;
  transport()->SetTransportErrorCallback([&] { closed_cb_count++; });

  constexpr pw::chrono::SystemClock::duration kCommandTimeout =
      std::chrono::seconds(12);

  StaticByteBuffer req_reset(LowerBits(hci_spec::kReset),
                             UpperBits(hci_spec::kReset),  // HCI_Reset opcode
                             0x00  // parameter_total_size
  );

  // Expect the HCI_Reset command but dont send a reply back to make the command
  // time out.
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset);

  size_t cb_count = 0;
  CommandChannel::TransactionId id1, id2;
  auto cb = [&cb_count](CommandChannel::TransactionId callback_id,
                        const EventPacket& event) { cb_count++; };

  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  id1 = cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id1);

  packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  id2 = cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id2);

  // Run the loop until the command timeout task gets scheduled.
  RunUntilIdle();
  ASSERT_EQ(0u, cb_count);
  EXPECT_EQ(0u, closed_cb_count);

  RunFor(kCommandTimeout);
  EXPECT_EQ(0u, cb_count);
  EXPECT_EQ(1u, closed_cb_count);
  EXPECT_TRUE(cmd_chan_weak.is_alive());
}

TEST_F(TransportDeathTest, AttachInspectBeforeInitializeACLDataChannelCrashes) {
  inspect::Inspector inspector;
  EXPECT_DEATH_IF_SUPPORTED(transport()->AttachInspect(inspector.GetRoot()),
                            ".*");
}

TEST_F(TransportTest, HciErrorClosesTransportWithSco) {
  size_t closed_cb_count = 0;
  transport()->SetTransportErrorCallback([&] { closed_cb_count++; });

  EXPECT_TRUE(transport()->InitializeScoDataChannel(
      DataBufferInfo(/*max_data_length=*/1, /*max_num_packets=*/1)));
  RunUntilIdle();

  test_device()->Stop();
  RunUntilIdle();
  EXPECT_EQ(closed_cb_count, 1u);
}

class TransportTestWithoutSco : public TransportTest {
 public:
  void SetUp() override {
    // Disable the SCO feature bit.
    TransportTest::SetUp(testing::MockController::FeaturesBits{0});
  }
};

TEST_F(TransportTestWithoutSco, GetScoChannelFailure) {
  size_t closed_cb_count = 0;
  transport()->SetTransportErrorCallback([&] { closed_cb_count++; });
  EXPECT_FALSE(transport()->InitializeScoDataChannel(
      DataBufferInfo(/*max_data_length=*/1, /*max_num_packets=*/1)));
  RunUntilIdle();
  EXPECT_EQ(closed_cb_count, 0u);
}

TEST_F(TransportTest, InitializeScoFailsBufferNotAvailable) {
  EXPECT_FALSE(transport()->InitializeScoDataChannel(
      DataBufferInfo(/*max_data_length=*/0, /*max_num_packets=*/0)));
}

}  // namespace
}  // namespace bt::hci
