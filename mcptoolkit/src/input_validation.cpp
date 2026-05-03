#include "input_validation.h"
#include <regex>
#include <algorithm>
#include <cctype>

namespace mcptoolkit {

bool InputValidationHandler::validate_arguments(
    const std::string& tool_name,
    const std::map<std::string, std::string>& arguments,
    std::string& error_msg) {

  for (const auto& [key, value] : arguments) {
    // Layer 1: Check for shell metacharacters
    if (contains_shell_metacharacters(value)) {
      error_msg = "Argument '" + key + "' contains shell metacharacters";
      return false;
    }

    // Layer 2: Check for URL-encoded metacharacters
    if (contains_encoded_metacharacters(value)) {
      error_msg = "Argument '" + key + "' contains encoded metacharacters";
      return false;
    }

    // Layer 3: Check for path traversal patterns
    if (contains_path_traversal(value)) {
      error_msg = "Argument '" + key + "' contains path traversal patterns";
      return false;
    }

    // Layer 4: Validate against allowlist pattern
    if (!matches_allowed_pattern(value)) {
      error_msg = "Argument '" + key + "' does not match allowed pattern";
      return false;
    }
  }

  return true;
}

bool InputValidationHandler::contains_shell_metacharacters(
    const std::string& value) {
  // Shell metacharacters that enable command injection:
  // ; (sequential) | (pipe) & (background) $ (variable)
  // ( ) (subshell) ` (command sub) < > (redirect) \n \r (newline)
  const char* metacharacters = ";|&$()`\n\r<>";

  return value.find_first_of(metacharacters) != std::string::npos;
}

bool InputValidationHandler::contains_encoded_metacharacters(
    const std::string& value) {
  // URL-encoded versions of shell metacharacters.
  // Common patterns used to bypass naive filters:
  const std::vector<std::string> encoded_patterns = {
    "%3b", "%3B",  // semicolon (;)
    "%7c", "%7C",  // pipe (|)
    "%26",         // ampersand (&)
    "%24",         // dollar sign ($)
    "%28",         // open parenthesis (()
    "%29",         // close parenthesis ())
    "%60",         // backtick (`)
    "%3c", "%3C",  // less-than (<)
    "%3e", "%3E"   // greater-than (>)
  };

  for (const auto& pattern : encoded_patterns) {
    if (value.find(pattern) != std::string::npos) {
      return true;
    }
  }

  return false;
}

bool InputValidationHandler::contains_path_traversal(
    const std::string& value) {
  // Detect path traversal patterns
  // ".." — directory traversal
  if (value.find("..") != std::string::npos) {
    return true;
  }

  // "~/" or "~\" — home directory expansion
  if (value.find("~/") != std::string::npos ||
      value.find("~\\") != std::string::npos) {
    return true;
  }

  return false;
}

bool InputValidationHandler::matches_allowed_pattern(
    const std::string& value) {
  // Default allowlist pattern: alphanumeric, underscore, hyphen, dot, slash
  // This is permissive by default; customize per tool/argument as needed
  // Pattern: ^[a-zA-Z0-9._/-]*$
  // Empty strings are allowed (optional arguments)

  if (value.empty()) {
    return true;
  }

  static const std::regex allowed_pattern("^[a-zA-Z0-9._/-]*$");
  return std::regex_match(value, allowed_pattern);
}

std::string InputValidationHandler::get_allowed_pattern_for_argument(
    const std::string& tool_name,
    const std::string& arg_name) {
  // Tool-specific and argument-specific patterns can be added here
  // For now, return default permissive pattern
  // Future: load from tool definition schema
  return "^[a-zA-Z0-9._/-]*$";
}

} // namespace mcptoolkit
