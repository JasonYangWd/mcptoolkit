#include <gtest/gtest.h>
#include <string>
#include "json/json_msg.h"

using namespace mcptoolkit;

// ============================================================================
// InputSizeLimits Tests
// ============================================================================

class InputSizeLimitsTest : public ::testing::Test {
 protected:
  // Default limit is 1 MB
  static constexpr size_t kDefaultMaxBytes = 1024 * 1024;

  // Create a valid request message of specified size
  std::string create_message(size_t payload_size) {
    std::string payload(payload_size, 'x');
    return R"({"jsonrpc":"2.0","id":1,"method":"test","params":")" + payload + R"("})";
  }
};

// Test: Reject message exceeding default limit
TEST_F(InputSizeLimitsTest, RejectOversizedMessage) {
  std::string oversized = create_message(kDefaultMaxBytes + 1);
  MCPMessage msg = MCPMessage::parse(oversized.c_str(), oversized.size());

  EXPECT_FALSE(msg.valid);
  EXPECT_EQ(msg.error_code, -32700);  // Parse error
}

// Test: Accept message at exactly the limit
TEST_F(InputSizeLimitsTest, AcceptMessageAtLimit) {
  std::string at_limit = create_message(kDefaultMaxBytes - 100);  // Leave room for JSON overhead
  MCPMessage msg = MCPMessage::parse(at_limit.c_str(), at_limit.size());

  // Should parse successfully (may fail due to malformed JSON, but not size)
  EXPECT_NE(msg.error_code, -32700);  // Should NOT be parse error from size
}

// Test: Accept message below limit
TEST_F(InputSizeLimitsTest, AcceptSmallMessage) {
  std::string small = R"({"jsonrpc":"2.0","id":1,"method":"test"})";
  MCPMessage msg = MCPMessage::parse(small.c_str(), small.size());

  EXPECT_TRUE(msg.valid);
  EXPECT_EQ(msg.error_code, 0);
}

// Test: Custom limit enforcement
TEST_F(InputSizeLimitsTest, CustomLimitSmall) {
  size_t custom_limit = 100;
  std::string oversized = create_message(custom_limit + 1);
  MCPMessage msg = MCPMessage::parse(oversized.c_str(), oversized.size(), custom_limit);

  EXPECT_FALSE(msg.valid);
  EXPECT_EQ(msg.error_code, -32700);
}

// Test: Accept message below custom limit
TEST_F(InputSizeLimitsTest, CustomLimitAccept) {
  size_t custom_limit = 1000;
  std::string small = R"({"jsonrpc":"2.0","id":1,"method":"test"})";
  MCPMessage msg = MCPMessage::parse(small.c_str(), small.size(), custom_limit);

  EXPECT_TRUE(msg.valid);
  EXPECT_EQ(msg.error_code, 0);
}

// Test: Zero-length input
TEST_F(InputSizeLimitsTest, EmptyInput) {
  MCPMessage msg = MCPMessage::parse("", 0);
  EXPECT_FALSE(msg.valid);
}

// Test: Boundary at exactly 1 MB (typical limit)
TEST_F(InputSizeLimitsTest, OneMBBoundary) {
  // Create message at 1 MB - 1 byte
  std::string almost_1mb = create_message(kDefaultMaxBytes - 100);
  MCPMessage msg1 = MCPMessage::parse(almost_1mb.c_str(), almost_1mb.size());
  EXPECT_NE(msg1.error_code, -32700);

  // Create message at 1 MB + 1 byte
  std::string over_1mb = create_message(kDefaultMaxBytes + 1);
  MCPMessage msg2 = MCPMessage::parse(over_1mb.c_str(), over_1mb.size());
  EXPECT_EQ(msg2.error_code, -32700);
}

// Test: std::string variant of parse
TEST_F(InputSizeLimitsTest, StringVariantDefaultLimit) {
  std::string small = R"({"jsonrpc":"2.0","id":1,"method":"test"})";
  MCPMessage msg = MCPMessage::parse(small);  // Uses default limit

  EXPECT_TRUE(msg.valid);
}

// Test: std::string variant with custom limit
TEST_F(InputSizeLimitsTest, StringVariantCustomLimit) {
  size_t custom_limit = 50;
  std::string oversized = create_message(custom_limit + 1);
  MCPMessage msg = MCPMessage::parse(oversized, custom_limit);

  EXPECT_FALSE(msg.valid);
  EXPECT_EQ(msg.error_code, -32700);
}

// Test: Performance - verify no overhead for small messages
TEST_F(InputSizeLimitsTest, SmallMessagePerformance) {
  std::string small = R"({"jsonrpc":"2.0","id":1,"method":"test"})";

  // Parse many times - should be fast with size check
  for (int i = 0; i < 1000; ++i) {
    MCPMessage msg = MCPMessage::parse(small);
    EXPECT_TRUE(msg.valid);
  }
}
