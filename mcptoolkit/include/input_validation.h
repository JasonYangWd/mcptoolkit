#pragma once

#include <string>
#include <map>
#include <vector>

namespace mcptoolkit {

class InputValidationHandler {
public:
  // Main validation method - validates all arguments for command injection
  static bool validate_arguments(
      const std::string& tool_name,
      const std::map<std::string, std::string>& arguments,
      std::string& error_msg);

  // Defense Layer 1: Detect shell metacharacters (;|&$()` etc)
  static bool contains_shell_metacharacters(const std::string& value);

  // Defense Layer 2: Detect URL-encoded metacharacters (%3b, %7c, etc)
  static bool contains_encoded_metacharacters(const std::string& value);

  // Defense Layer 3: Detect path traversal patterns (.., ~/)
  static bool contains_path_traversal(const std::string& value);

  // Defense Layer 4: Validate against regex allowlist pattern
  static bool matches_allowed_pattern(const std::string& value);

private:
  // Helper to get the allowed pattern for a specific argument
  static std::string get_allowed_pattern_for_argument(
      const std::string& tool_name,
      const std::string& arg_name);
};

} // namespace mcptoolkit
