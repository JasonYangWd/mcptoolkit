#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace mcptoolkit {

struct ToolRequest {
  std::string tool_name;
  std::string args_json;
  std::string client_id;

  ToolRequest() = default;
  ToolRequest(const std::string& name, const std::string& args,
              const std::string& client)
      : tool_name(name), args_json(args), client_id(client) {}
};

class RequestQueue {
 public:
  RequestQueue();
  ~RequestQueue() = default;

  // Enqueue a request (returns false if queue is full)
  bool enqueue(const ToolRequest& req);

  // Dequeue a request (blocks until available or timeout)
  bool dequeue(ToolRequest& req);

  // Get current queue size
  size_t size() const;

  // Check if queue is full
  bool is_full() const;

  // Set maximum queue size
  void set_max_size(size_t max);

  // Get maximum queue size
  size_t get_max_size() const;

  // Clear the queue
  void clear();

 private:
  mutable std::mutex lock_;
  std::condition_variable cv_;
  std::queue<ToolRequest> queue_;
  size_t max_size_;
};

}  // namespace mcptoolkit
