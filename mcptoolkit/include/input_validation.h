#pragma once

#include <string>
#include <map>
#include <vector>
#include <regex>

namespace mcptoolkit {

// Validation rule for a tool argument
struct ArgumentValidationRule {
    std::string arg_name;
    std::string pattern;           // Regex pattern for allowed values
    bool allow_shell_metacharacters = false;
    bool allow_path_traversal = false;
};

// Validation rules for a specific tool
struct ToolValidationRules {
    std::string tool_name;
    std::vector<ArgumentValidationRule> required_args;
    std::vector<ArgumentValidationRule> optional_args;
    bool strict_mode = true;  // Reject unknown arguments
};

class InputValidationHandler {
public:
    InputValidationHandler();

    // Register validation rules for a tool
    void register_tool_rules(const ToolValidationRules& rules);

    // Validate tool arguments against registered rules
    // Returns true if valid, false if invalid
    // error_msg contains details if validation fails
    bool validate_arguments(const std::string& tool_name,
                           const std::map<std::string, std::string>& arguments,
                           std::string& error_msg);

    // Check if a string contains shell metacharacters
    static bool contains_shell_metacharacters(const std::string& value);

    // Check if a string contains path traversal sequences
    static bool contains_path_traversal(const std::string& value);

    // Check if a string contains URL-encoded dangerous characters
    static bool contains_encoded_metacharacters(const std::string& value);

    // Escape shell argument (wrap in quotes, escape quotes)
    static std::string escape_shell_argument(const std::string& arg);

private:
    std::map<std::string, ToolValidationRules> rules_;

    // Helper to check argument against a rule
    bool validate_argument(const ArgumentValidationRule& rule,
                          const std::string& value,
                          std::string& error_msg);
};

} // namespace mcptoolkit
