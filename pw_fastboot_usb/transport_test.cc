#include "pw_fastboot_usb/transport.h"

#include <memory>

#include "gmock/gmock.h"
#include "pw_bytes/span.h"
#include "pw_fastboot_usb/packet.h"
#include "pw_fastboot_usb/tests/mock_packet_interface.h"
#include "pw_unit_test/framework.h"

using testing::_;

// Assert that reading `size` bytes from the transport mock
// returns the exact expected result (a read returns exactly
// the size of the result and all bytes are equal).
#define ASSERT_TRANSPORT_READ(mock, read_size, expected_result)               \
  do {                                                                        \
    std::vector<std::byte> received{read_size};                               \
    const auto bytes_read = (mock).Read(received);                            \
    ASSERT_EQ(bytes_read, expected_result.size());                            \
    ASSERT_EQ(                                                                \
        std::memcmp(received.data(), expected_result.data(), bytes_read), 0); \
  } while (false)

namespace pw::fastboot {

// Returns an action that can be used to verify the QueuePacket
// method receives exactly the given byte span (size and bytes equal).
// The span must live longer than the method call.
static auto VerifyPacketMatches(pw::ConstByteSpan to_send) {
  return testing::Truly([to_send](pw::ConstByteSpan data) -> bool {
    return (data.size() == to_send.size_bytes()) &&
           std::memcmp(data.data(), to_send.data(), data.size()) == 0;
  });
}

// Returns an action that always returns the size of the passed in span
// The span must live longer than the method call.
static auto ReturnSentPacketSize(pw::ConstByteSpan to_send) {
  return [to_send] { return to_send.size_bytes(); };
}

TEST(FastbootUsbTransport, TransportWaitsForInitialization) {
  auto packet = std::make_unique<MockUsbPacketInterface>();
  auto* mock = packet.get();

  EXPECT_CALL(*mock, Init(_)).Times(testing::Exactly(1));
  auto transport =
      std::make_unique<pw::fastboot::UsbTransport>(std::move(packet));
}

TEST(FastbootUsbTransport, Write) {
  auto packet = std::make_unique<MockUsbPacketInterface>();
  auto* mock = packet.get();
  auto transport =
      std::make_unique<pw::fastboot::UsbTransport>(std::move(packet));

  std::vector<std::byte> data{pw::fastboot::kFastbootUsbMaxPacketSize,
                              (std::byte)0xFF};
  EXPECT_CALL(*mock, QueuePacket(VerifyPacketMatches(data)))
      .Times(testing::Exactly(1))
      .WillRepeatedly(ReturnSentPacketSize(data));
  ASSERT_EQ(transport->Write(data), pw::fastboot::kFastbootUsbMaxPacketSize);
}

TEST(FastbootUsbTransport, WriteQueuing) {
  // Default queue size is 512b, make sure two packets can fit
  static constexpr size_t kSendSize = 128;

  auto packet = std::make_unique<MockUsbPacketInterface>();
  auto* mock = packet.get();
  auto transport =
      std::make_unique<pw::fastboot::UsbTransport>(std::move(packet));

  std::vector<std::byte> sent1{kSendSize, (std::byte)0x11};
  std::vector<std::byte> sent2{kSendSize, (std::byte)0x22};
  EXPECT_CALL(*mock, QueuePacket(VerifyPacketMatches(sent1)))
      .Times(testing::Exactly(1))
      .WillRepeatedly(ReturnSentPacketSize(sent1));
  ASSERT_EQ(transport->Write(sent1), kSendSize);

  // Until OnPacketSent is called, further packets should not be sent out
  EXPECT_CALL(*mock, QueuePacket(VerifyPacketMatches(sent2))).Times(0);
  ASSERT_EQ(transport->Write(sent2), kSendSize);

  // After calling OnPacketSent, further packets will be sent
  EXPECT_CALL(*mock, QueuePacket(VerifyPacketMatches(sent2)))
      .Times(1)
      .WillRepeatedly(ReturnSentPacketSize(sent2));
  mock->OnPacketSent(sent1);
}

TEST(FastbootUsbTransport, Read) {
  auto packet = std::make_unique<MockUsbPacketInterface>();
  auto* mock = packet.get();
  auto transport =
      std::make_unique<pw::fastboot::UsbTransport>(std::move(packet));

  std::vector<std::byte> sent{pw::fastboot::kFastbootUsbMaxPacketSize,
                              (std::byte)0x11};
  mock->OnPacketReceived(sent);

  ASSERT_TRANSPORT_READ(
      *transport, pw::fastboot::kFastbootUsbMaxPacketSize, sent);
}

TEST(FastbootUsbTransport, ReadQueuing) {
  auto packet = std::make_unique<MockUsbPacketInterface>();
  auto* mock = packet.get();
  auto transport =
      std::make_unique<pw::fastboot::UsbTransport>(std::move(packet));

  std::vector<std::byte> sent1{pw::fastboot::kFastbootUsbMaxPacketSize,
                               (std::byte)0x11};
  std::vector<std::byte> sent2{pw::fastboot::kFastbootUsbMaxPacketSize,
                               (std::byte)0x22};
  std::vector<std::byte> sent3{pw::fastboot::kFastbootUsbMaxPacketSize,
                               (std::byte)0x33};
  mock->OnPacketReceived(sent1);
  mock->OnPacketReceived(sent2);
  mock->OnPacketReceived(sent3);

  ASSERT_TRANSPORT_READ(
      *transport, pw::fastboot::kFastbootUsbMaxPacketSize, sent1);
  ASSERT_TRANSPORT_READ(
      *transport, pw::fastboot::kFastbootUsbMaxPacketSize, sent2);
  ASSERT_TRANSPORT_READ(
      *transport, pw::fastboot::kFastbootUsbMaxPacketSize, sent3);
}

TEST(FastbootUsbTransport, ReadNonMultipleOfPacketSize) {
  static constexpr size_t kLastPacketSize = 42;

  auto packet = std::make_unique<MockUsbPacketInterface>();
  auto* mock = packet.get();
  auto transport =
      std::make_unique<pw::fastboot::UsbTransport>(std::move(packet));

  std::vector<std::byte> sent1{pw::fastboot::kFastbootUsbMaxPacketSize,
                               (std::byte)0x11};
  std::vector<std::byte> sent2{kLastPacketSize, (std::byte)0x22};
  mock->OnPacketReceived(sent1);
  mock->OnPacketReceived(sent2);

  ASSERT_TRANSPORT_READ(
      *transport, pw::fastboot::kFastbootUsbMaxPacketSize, sent1);
  ASSERT_TRANSPORT_READ(*transport, kLastPacketSize, sent2);
}

TEST(FastbootUsbTransport, WriteNonMultipleOfPacketSize) {
  static constexpr size_t kLastPacketSize = 42;

  auto packet = std::make_unique<MockUsbPacketInterface>();
  auto* mock = packet.get();
  auto transport =
      std::make_unique<pw::fastboot::UsbTransport>(std::move(packet));

  std::vector<std::byte> sent1{pw::fastboot::kFastbootUsbMaxPacketSize,
                               (std::byte)0x11};
  std::vector<std::byte> sent2{kLastPacketSize, (std::byte)0x22};

  EXPECT_CALL(*mock, QueuePacket(VerifyPacketMatches(sent1)))
      .Times(testing::Exactly(1))
      .WillRepeatedly(ReturnSentPacketSize(sent1));
  ASSERT_EQ(transport->Write(sent1), sent1.size());
  mock->OnPacketSent(sent1);

  EXPECT_CALL(*mock, QueuePacket(VerifyPacketMatches(sent2)))
      .Times(testing::Exactly(1))
      .WillRepeatedly(ReturnSentPacketSize(sent2));
  ASSERT_EQ(transport->Write(sent2), sent2.size());
  mock->OnPacketSent(sent2);
}

}  // namespace pw::fastboot