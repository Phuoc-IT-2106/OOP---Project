#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace oop_agent::agent {

// A blocking FIFO mailbox. close() wakes waiting workers so thread creation or
// shutdown failures cannot leave a coordinator blocked forever.
template <typename T>
class MessageQueue {
  public:
    MessageQueue() = default;
    MessageQueue(const MessageQueue &) = delete;
    MessageQueue &operator=(const MessageQueue &) = delete;

    bool push(T message) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) {
                return false;
            }
            messages_.push(std::move(message));
        }
        available_.notify_one();
        return true;
    }

    std::optional<T> waitPop() {
        std::unique_lock<std::mutex> lock(mutex_);
        available_.wait(lock, [this] { return closed_ || !messages_.empty(); });
        if (messages_.empty()) {
            return std::nullopt;
        }

        T message = std::move(messages_.front());
        messages_.pop();
        return message;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        available_.notify_all();
    }

    bool isClosed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_.size();
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable available_;
    std::queue<T> messages_;
    bool closed_{false};
};

} // namespace oop_agent::agent
