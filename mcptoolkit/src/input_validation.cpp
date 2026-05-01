#include "../pch.h"
#include "input_validation.h"
#include <algorithm>

namespace mcptoolkit {

InputValidationHandler::InputValidationHandler() {}

void InputValidationHandler::register_tool_rules(const ToolValidationRules& rules) {
    rules_[rules.tool_name] = rules;
}

bool InputValidationHandler::validate_arguments(const std::string& tool_name,
                                                const std::map<std::string, std::string>& arguments,
                                                std::string& error_msg) {
    // Check if tool has validation rules
    auto it = rules_.find(tool_name);
    if (it == rules_.end()) {
        // No rules registered for this tool — allow by default
        return true;
    }

    const ToolValidationRules& tool_rules = it->second;

    // Validate required arguments
    for (const auto& req : tool_rules.required_args) {
        auto arg_it = arguments.find(req.arg_name);
        if (arg_it == arguments.end()) {
            error_msg = "Missing required argument: " + req.arg_name;
            return false;
        }

        if (!validate_argument(req, arg_it->second, error_msg)) {
            return false;
        }
    }

    // Validate optional arguments that are present
    for (const auto& opt : tool_rules.optional_args) {
        auto arg_it = arguments.find(opt.arg_name);
        if (arg_it != arguments.end()) {
            if (!validate_argument(opt, arg_it->second, error_msg)) {
                return false;
            }
        }
    }

    // Check for unknown arguments if strict mode
    if (tool_rules.strict_mode) {
        std::vector<std::string> known_args;
        for (const auto& req : tool_rules.required_args) {
            known_args.push_back(req.arg_name);
        }
        for (const auto& opt : tool_rules.optional_args) {
            known_args.push_back(opt.arg_name);
        }

        for (const auto& [key, value] : arguments) {
            if (std::find(known_args.begin(), known_args.end(), key) == known_args.end()) {
                error_msg = "Unknown argument: " + key;
                return false;
            }
        }
    }

    return true;
}

bool InputValidationHandler::validate_argument(const ArgumentValidationRule& rule,
                                              const std::string& value,
                                              std::string& error_msg) {
    // Check for shell metacharacters unless explicitly allowed
    if (!rule.allow_shell_metacharacters) {
        if (contains_shell_metacharacters(value)) {
            error_msg = "Argument '" + rule.arg_name + "' contains invalid shell metacharacters";
            return false;
        }
    }

    // Check for path traversal unless explicitly allowed
    if (!rule.allow_path_traversal) {
        if (contains_path_traversal(value)) {
            error_msg = "Argument '" + rule.arg_name + "' contains invalid path traversal sequences";
            return false;
        }
    }

    // Check for URL-encoded dangerous characters
    if (contains_encoded_metacharacters(value)) {
        error_msg = "Argument '" + rule.arg_name + "' contains URL-encoded dangerous characters";
        return false;
    }

    // Validate against regex pattern if provided
    if (!rule.pattern.empty()) {
        try {
            std::regex pattern(rule.pattern);
            if (!std::regex_match(value, pattern)) {
                error_msg = "Argument '" + rule.arg_name + "' does not match required pattern";
                return false;
            }
        } catch (const std::regex_error& e) {
            error_msg = "Regex pattern error: " + std::string(e.what());
            return false;
        }
    }

    return true;
}

bool InputValidationHandler::contains_shell_metacharacters(const std::string& value) {
    // Characters that have special meaning in shell: ; | & $ ( ) ` < > \n \r
    static const char* metacharacters = ";|&$()` \n\r";
    return value.find_first_of(metacharacters) != std::string::npos;
}

bool InputValidationHandler::contains_path_traversal(const std::string& value) {
    // Check for .. sequences that could escape directory
    if (value.find("..") != std::string::npos) {
        return true;
    }
    // Also check for ~/ which expands to home directory
    if (value.find("~/") != std::string::npos) {
        return true;
    }
    return false;
}

bool InputValidationHandler::contains_encoded_metacharacters(const std::string& value) {
    // Check for URL-encoded special characters
    // %3b = ;, %7c = |, %26 = &, %24 = $, %28 = (, %29 = ), %60 = `, %3c = <, %3e = >
    static const char* encoded_patterns[] = {
        "%3b", "%3B",  // semicolon
        "%7c", "%7C",  // pipe
        "%26",         // ampersand
        "%24",         // dollar sign
        "%28",         // open paren
        "%29",         // close paren
        "%60",         // backtick
        "%3c", "%3C",  // less than
        "%3e", "%3E",  // greater than
        "%0a", "%0A",  // newline
        "%0d", "%0D",  // carriage return
    };

    for (const auto& pattern : encoded_patterns) {
        if (value.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string InputValidationHandler::escape_shell_argument(const std::string& arg) {
    // Wrap in single quotes and escape single quotes
    std::string escaped = "'";
    for (char c : arg) {
        if (c == '\'') {
            // Break out of single quotes, escape the quote, re-enter
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

} // namespace mcptoolkit
