#include "request_queue.h"

namespace mcptoolkit {

RequestQueue::RequestQueue() : max_size_(1000) {}

bool RequestQueue::enqueue(const ToolRequest& req) {
  std::unique_lock<std::mutex> lock(lock_);

  // Check if queue is full
  if (queue_.size() >= max_size_) {
    return false;
  }

  queue_.push(req);
  cv_.notify_one();
  return true;
}

bool RequestQueue::dequeue(ToolRequest& req) {
  std::unique_lock<std::mutex> lock(lock_);

  // Wait for queue to have items (with timeout to prevent indefinite blocking)
  if (!cv_.wait_for(lock, std::chrono::seconds(30),
                    [this] { return !queue_.empty(); })) {
    return false;
  }

  if (queue_.empty()) {
    return false;
  }

  req = queue_.front();
  queue_.pop();
  return true;
}

size_t RequestQueue::size() const {
  std::lock_guard<std::mutex> lock(lock_);
  return queue_.size();
}

bool RequestQueue::is_full() const {
  std::lock_guard<std::mutex> lock(lock_);
  return queue_.size() >= max_size_;
}

void RequestQueue::set_max_size(size_t max) {
  std::lock_guard<std::mutex> lock(lock_);
  max_size_ = max;
}

size_t RequestQueue::get_max_size() const {
  std::lock_guard<std::mutex> lock(lock_);
  return max_size_;
}

void RequestQueue::clear() {
  std::lock_guard<std::mutex> lock(lock_);
  while (!queue_.empty()) {
    queue_.pop();
  }
}

}  // namespace mcptoolkit
