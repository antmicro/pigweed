#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "pw_fastboot/constants.h"
#include "pw_fastboot/device_hal.h"
#include "pw_fastboot/tests/mock_device.h"
#include "pw_fastboot/tests/mock_transport.h"
#include "pw_unit_test/framework.h"
#include "stringutils/strings.h"

namespace pw::fastboot {

using ::testing::_;

#define ASSERT_FASTBOOT_STATUS_IMPL(output, expected_status, assertion)        \
  do {                                                                         \
    auto container = output;                                                   \
    ASSERT_GT(container.size(), 0U);                                           \
    const auto status_string = std::string{expected_status};                   \
    assertion(container.back().substr(0, status_string.size()), status_string) \
        << "Last fastboot message:\n\t" << container.back() << "\n";           \
  } while (false)

// Assert that a fastboot command, for which the container `output`
// is presumed to contain the response lines, has completed with
// the given fastboot status. It is safe to call this macro with a
// temporary as `output`; the value will be only evaluated once.
#define ASSERT_FASTBOOT_STATUS(output, expected_status) \
  ASSERT_FASTBOOT_STATUS_IMPL(output, expected_status, ASSERT_EQ)

// Assert that a fastboot command, for which the container `output`
// is presumed to contain the response lines, has completed with
// a fastboot status other than the given one. It is safe to call
// this macro with a temporary as `output`; the value will be only
// evaluated once.
#define ASSERT_FASTBOOT_STATUS_NOT(output, expected_status) \
  ASSERT_FASTBOOT_STATUS_IMPL(output, expected_status, ASSERT_NE)

TEST(FastbootDevice, SendInvalidCommand) {
  auto mock = MockDevice{};
  mock.Start();

  mock.transport()->SendInput("INVALID");
  ASSERT_FASTBOOT_STATUS(mock.transport()->ReadOutputLines(), RESPONSE_FAIL);
}

TEST(FastbootDevice, SendValidCommand) {
  auto mock = MockDevice{};
  mock.Start();

  mock.transport()->SendInput("getvar:version");
  ASSERT_FASTBOOT_STATUS(mock.transport()->ReadOutputLines(), RESPONSE_OKAY);
}

TEST(FastbootDevice, SendDownloadCommand) {
  static constexpr size_t kDownloadSize = 0x1000;
  auto mock = MockDevice{};
  mock.Start();

  // Send download command to enter data download phase
  mock.transport()->SendInput(
      stringutils::StringPrintf("download:%08zx", kDownloadSize));
  ASSERT_FASTBOOT_STATUS(mock.transport()->ReadOutputLines(), RESPONSE_DATA);

  // Prepare some data to compare against later
  std::vector<char> data{};
  data.resize(kDownloadSize);
  for (size_t i = 0; i < 0x1000; ++i) {
    data[i] = static_cast<char>(i % 256);
  }

  // Send the entirety of the data and wait for an OKAY
  mock.transport()->SendInput(data);
  ASSERT_FASTBOOT_STATUS(mock.transport()->ReadOutputLines(), RESPONSE_OKAY);
  // Received data must be equal
  ASSERT_EQ(mock.device()->download_data(), data);
}

TEST(FastbootDevice, GetVariableOne) {
  static std::string kTestVariableName = "TEST-VARIABLE";
  static std::string kTestVariableContents = "CONTENTS OF THE VARIABLE";

  auto mock = MockDevice{};
  mock.variables()->RegisterVariable(
      kTestVariableName, [](auto, auto, std::string* message) -> bool {
        *message = kTestVariableContents;
        return true;
      });
  mock.Start();

  mock.transport()->SendInput("getvar:" + kTestVariableName);
  const auto lines = mock.transport()->ReadOutputLines();
  ASSERT_EQ(lines.size(), 1U);
  auto response = lines.back();

  auto expectedOutput = RESPONSE_OKAY + kTestVariableContents;
  ASSERT_STREQ(response.data(), expectedOutput.data());
}

TEST(FastbootDevice, GetVariableInvalid) {
  auto mock = MockDevice{};
  mock.Start();

  mock.transport()->SendInput("getvar:DOESNOTEXIST");
  ASSERT_FASTBOOT_STATUS(mock.transport()->ReadOutputLines(), RESPONSE_FAIL);
}

TEST(FastbootDevice, GetVariableAll) {
  auto mock = MockDevice{};
  mock.Start();

  mock.transport()->SendInput(std::string{"getvar:"} + FB_VAR_ALL);
  const auto lines = mock.transport()->ReadOutputLines(2);
  ASSERT_GT(lines.size(), 1U);
  // Variable contents are dumped as INFO packets
  for (size_t i = 0; i < lines.size() - 1; ++i) {
    auto line = lines.at(i);
    ASSERT_TRUE(stringutils::StartsWith(line, RESPONSE_INFO));
  }
  // Final packet contains the OKAY
  ASSERT_FASTBOOT_STATUS(lines, RESPONSE_OKAY);
}

TEST(FastbootDevice, Flash) {
  static std::string kTestPartitionName = "system";  // can be anything
  auto mock = MockDevice{};
  mock.Start();

  EXPECT_CALL(*mock.hal(), Flash(mock.device(), kTestPartitionName))
      .Times(testing::Exactly(1))
      .WillRepeatedly(::testing::Return(true));

  mock.transport()->SendInput("flash:" + kTestPartitionName);
  ASSERT_FASTBOOT_STATUS(mock.transport()->ReadOutputLines(), RESPONSE_OKAY);
}

TEST(FastbootDevice, OemCommands) {
  static std::string kTestOemCommandName = "VENDOR_COMMAND";  // can be anything
  auto mock = MockDevice{};
  mock.Start();

  EXPECT_CALL(*mock.hal(),
              OemCommand(mock.device(), "oem " + kTestOemCommandName))
      .Times(testing::Exactly(1))
      .WillRepeatedly(::testing::Return(true));

  mock.transport()->SendInput("oem " + kTestOemCommandName);
  ASSERT_FASTBOOT_STATUS(mock.transport()->ReadOutputLines(), RESPONSE_OKAY);
}

TEST(FastbootDevice, Shutdown) {
  auto mock = MockDevice{};
  mock.Start();

  EXPECT_CALL(*mock.hal(), ShutDown(mock.device()))
      .Times(testing::Exactly(1))
      .WillRepeatedly(::testing::Return(true));

  mock.transport()->SendInput("shutdown");
  ASSERT_FASTBOOT_STATUS_NOT(mock.transport()->ReadOutputLines(),
                             RESPONSE_FAIL);
}

TEST(FastbootDevice, RebootNormal) {
  auto mock = MockDevice{};
  mock.Start();

  EXPECT_CALL(*mock.hal(),
              Reboot(mock.device(), pw::fastboot::RebootType::ToSoftware))
      .Times(testing::Exactly(1))
      .WillRepeatedly(::testing::Return(true));

  mock.transport()->SendInput("reboot");
  ASSERT_FASTBOOT_STATUS_NOT(mock.transport()->ReadOutputLines(),
                             RESPONSE_FAIL);
}

TEST(FastbootDevice, RebootBootloader) {
  auto mock = MockDevice{};
  mock.Start();

  EXPECT_CALL(*mock.hal(),
              Reboot(mock.device(), pw::fastboot::RebootType::ToBootloader))
      .Times(testing::Exactly(1))
      .WillRepeatedly(::testing::Return(true));

  mock.transport()->SendInput("reboot-bootloader");
  ASSERT_FASTBOOT_STATUS_NOT(mock.transport()->ReadOutputLines(),
                             RESPONSE_FAIL);
}

TEST(FastbootDevice, RebootRecovery) {
  auto mock = MockDevice{};
  mock.Start();

  EXPECT_CALL(*mock.hal(),
              Reboot(mock.device(), pw::fastboot::RebootType::ToRecovery))
      .Times(testing::Exactly(1))
      .WillRepeatedly(::testing::Return(true));

  mock.transport()->SendInput("reboot-recovery");
  ASSERT_FASTBOOT_STATUS_NOT(mock.transport()->ReadOutputLines(),
                             RESPONSE_FAIL);
}

TEST(FastbootDevice, RebootFastboot) {
  auto mock = MockDevice{};
  mock.Start();

  EXPECT_CALL(*mock.hal(),
              Reboot(mock.device(), pw::fastboot::RebootType::ToFastboot))
      .Times(testing::Exactly(1))
      .WillRepeatedly(::testing::Return(true));

  mock.transport()->SendInput("reboot-fastboot");
  ASSERT_FASTBOOT_STATUS_NOT(mock.transport()->ReadOutputLines(),
                             RESPONSE_FAIL);
}

}  // namespace pw::fastboot