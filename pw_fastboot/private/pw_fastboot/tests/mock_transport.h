#pragma once
#include <queue>
#include <type_traits>
#include <vector>

#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"
#include "pw_fastboot/transport.h"
#include "pw_sync/mutex.h"
#include "pw_sync/thread_notification.h"
#include "pw_unit_test/framework.h"

template <class From, class To, typename = std::void_t<>>
struct explicitly_convertible_to : std::false_type {};

template <class From, class To>
struct explicitly_convertible_to<
    From,
    To,
    std::void_t<decltype(static_cast<To>(std::declval<From>()))>>
    : std::true_type {};

template <class From, class To>
inline constexpr bool explicitly_convertible_to_v =
    explicitly_convertible_to<From, To>::value;

namespace pw::fastboot {

class MockTransport : public pw::fastboot::Transport {
 public:
  MockTransport() = default;

  // Read operations on the mock will timeout after this duration
  static constexpr pw::chrono::SystemClock::duration kMockReadTimeout =
      pw::chrono::SystemClock::for_at_least(std::chrono::seconds(10));

  // Send bytes from the given vector over the pw::fastboot::Transport.
  void SendInput(std::vector<char> const& input) { SendContainer(input); }
  // Send bytes from the given vector over the pw::fastboot::Transport.
  void SendInput(std::vector<std::byte> const& input) { SendContainer(input); }
  // Send a string over the pw::fastboot::Transport.
  void SendInput(std::string const& input) { SendContainer(input); }

  // Read all packets sent out from the pw::fastboot::Transport by
  // interpreting them as consecutive strings. This waits until
  // **at least** N lines have been received from the transport, it
  // is allowed for this function to return more than that.
  // This will block until data is available, or the timeout occurs.
  // In the case of failure, an empty vector is returned.
  std::vector<std::string> ReadOutputLines(
      size_t N = 1,
      pw::chrono::SystemClock::duration timeout = kMockReadTimeout) {
    std::vector<std::string> strs{};

    while (strs.size() < N) {
      if (!WaitForOutputWithTimeout(timeout)) {
        return {};
      }
      std::lock_guard lg{state_lock_};

      strs.reserve(strs.size() + output_.size());
      while (!output_.empty()) {
        const auto packet = output_.front();

        std::string str{};
        str.reserve(packet.size());
        for (auto& byte : packet) {
          str += static_cast<char>(byte);
        }

        strs.push_back(str);
        output_.pop();
      }
    }
    return strs;
  }

  // DO NOT USE EXTERNALLY: pw::fastboot::Transport implementation
  // Called by the pw::fastboot::Device that receives this MockTransport.
  std::ptrdiff_t Read(pw::ByteSpan span) override {
    {
      std::lock_guard lg{state_lock_};
      if (transport_closed_) {
        return -1;
      }
    }
    input_available_.acquire();
    std::lock_guard lg{state_lock_};
    if (transport_closed_) {
      return -1;
    }
    const auto transfer_size = std::min(input_.size(), span.size());
    std::memcpy(span.data(), input_.data(), transfer_size);
    input_.clear();
    return transfer_size;
  }

  // DO NOT USE EXTERNALLY: pw::fastboot::Transport implementation
  // Called by the pw::fastboot::Device that receives this MockTransport.
  std::ptrdiff_t Write(pw::ConstByteSpan span) override {
    {
      std::lock_guard lg{state_lock_};

      std::vector<std::byte> vec{};
      for (auto& byte : span) {
        vec.push_back(byte);
      }

      output_.emplace(std::move(vec));
    }
    output_available_.release();
    return span.size();
  }

  // DO NOT USE EXTERNALLY: pw::fastboot::Transport implementation
  // Called by the pw::fastboot::Device that receives this MockTransport.
  int Close() override {
    {
      std::lock_guard lg{state_lock_};
      input_.clear();
      transport_closed_ = true;
    }
    input_available_.release();
    return 0;
  }

  // DO NOT USE EXTERNALLY: pw::fastboot::Transport implementation
  // Called by the pw::fastboot::Device that receives this MockTransport.
  int Reset() override { return 0; }

 private:
  // Send values of a container T over the pw::fastboot::Transport.
  // Values of the container T must be explicitly convertible to std::byte.
  template <typename T>
  void SendContainer(T const& container) {
    static_assert(
        explicitly_convertible_to_v<
            typename std::iterator_traits<typename T::iterator>::value_type,
            std::byte>,
        "Container elements must be explicitly convertible to std::byte");

    {
      std::lock_guard lg{state_lock_};
      for (auto& value : container) {
        input_.push_back(static_cast<std::byte>(value));
      }
    }
    input_available_.release();
  }

  // Wait for output to appear on the pw::fastboot::Transport with a given
  // timeout. If a timeout occurs, the transport is closed and a failure
  // is logged (the assumption is that if a timeout ever happens, the tests
  // have failed and there's nothing left to do other than cleanup).
  bool WaitForOutputWithTimeout(pw::chrono::SystemClock::duration timeout) {
    if (transport_closed_) {
      return false;
    }

    pw::chrono::SystemTimer timer{[this](pw::chrono::SystemClock::time_point) {
      // Close the transport and unblock the caller to propagate the failure
      Close();
      output_available_.release();

      ADD_FAILURE() << "ERROR: Reading fastboot output timed out!";
    }};
    timer.InvokeAfter(timeout);
    output_available_.acquire();
    timer.Cancel();

    // If the transport was closed, let the reader consume the data
    // that is remaining in the queue.
    std::lock_guard lg{state_lock_};
    return transport_closed_ ? (output_.size() > 0) : true;
  }

  bool transport_closed_;
  pw::sync::ThreadNotification input_available_;
  pw::sync::ThreadNotification output_available_;
  pw::sync::Mutex state_lock_;
  std::vector<std::byte> input_;
  std::queue<std::vector<std::byte>> output_;
};

}  // namespace pw::fastboot