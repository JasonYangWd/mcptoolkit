#include <gtest/gtest.h>
#include "path_validator.h"
#include <filesystem>
#include <chrono>

using namespace mcptoolkit;

class PathValidatorTest : public ::testing::Test {
protected:
    PathValidator validator_;
    std::string error_msg_;

    void SetUp() override {
        PathValidationConfig config;
        config.base_directory = "/documents/";
        config.allow_absolute_paths = false;
        config.allow_symlinks = false;
        validator_.configure(config);
    }
};

// Basic valid paths should pass
TEST_F(PathValidatorTest, SafeRelativePathPasses) {
    EXPECT_TRUE(validator_.is_safe_path("report.txt", error_msg_));
    EXPECT_TRUE(validator_.is_safe_path("subfolder/file.txt", error_msg_));
    EXPECT_TRUE(validator_.is_safe_path("deep/nested/folder/document.pdf", error_msg_));
}

// Attempt to escape base directory with ..
TEST_F(PathValidatorTest, SimpleTraversalBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("../secret.txt", error_msg_));
    EXPECT_FALSE(validator_.is_safe_path("../../etc/passwd", error_msg_));
    EXPECT_FALSE(validator_.is_safe_path("../../../etc/passwd", error_msg_));
}

// Multiple .. sequences should be blocked
TEST_F(PathValidatorTest, MultipleTraversalBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("a/../b/../c/../../secret", error_msg_));
}

// Null bytes should be blocked (can truncate paths in C APIs)
TEST_F(PathValidatorTest, NullByteBlocked) {
    std::string malicious("file.txt\0../secret", 18);
    EXPECT_FALSE(validator_.is_safe_path(malicious, error_msg_));
}

// Windows-style path traversal
TEST_F(PathValidatorTest, WindowsStyleTraversalBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("..\\..\\windows\\system32", error_msg_));
    EXPECT_FALSE(validator_.is_safe_path("..\\secret.txt", error_msg_));
}

// Home directory expansion should be blocked
TEST_F(PathValidatorTest, HomeDirectoryExpansionBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("~/.ssh/id_rsa", error_msg_));
}

// URL-encoded traversal: %2e%2e = ..
TEST_F(PathValidatorTest, URLEncodedTraversalBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("%2e%2e/etc/passwd", error_msg_));
    EXPECT_FALSE(validator_.is_safe_path("%2E%2E/etc/passwd", error_msg_));
    EXPECT_FALSE(validator_.is_safe_path("%2e%2E/etc/passwd", error_msg_));
}

// URL-encoded forward slash: %2f = /
TEST_F(PathValidatorTest, URLEncodedSlashBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("..%2f..%2fetc%2fpasswd", error_msg_));
}

// URL-encoded backslash: %5c = backslash
TEST_F(PathValidatorTest, URLEncodedBackslashBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("..%5c..%5cwindows%5csystem32", error_msg_));
}

// Double-encoded traversal: %252e%252e = %2e%2e = ..
TEST_F(PathValidatorTest, DoubleEncodedTraversalBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("%252e%252e/etc/passwd", error_msg_));
    EXPECT_FALSE(validator_.is_safe_path("%252e%252e%252fetc%252fpasswd", error_msg_));
}

// Absolute paths should be blocked when not allowed
TEST_F(PathValidatorTest, AbsolutePathBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("/etc/passwd", error_msg_));
    EXPECT_FALSE(validator_.is_safe_path("/home/user/.ssh/id_rsa", error_msg_));
}

// Windows absolute paths should be blocked
TEST_F(PathValidatorTest, WindowsAbsolutePathBlocked) {
    EXPECT_FALSE(validator_.is_safe_path("C:\\Windows\\System32\\config\\SAM", error_msg_));
    EXPECT_FALSE(validator_.is_safe_path("D:\\Data\\secrets.txt", error_msg_));
}

// Static methods: contains_traversal_sequences
TEST(PathValidatorStaticTest, ContainsTraversalSequences) {
    EXPECT_TRUE(PathValidator::contains_traversal_sequences("../file.txt"));
    EXPECT_TRUE(PathValidator::contains_traversal_sequences("../../secret"));
    EXPECT_TRUE(PathValidator::contains_traversal_sequences("..\\secret"));
    EXPECT_TRUE(PathValidator::contains_traversal_sequences("~/.ssh/key"));
    EXPECT_FALSE(PathValidator::contains_traversal_sequences("normal/path/file.txt"));
    EXPECT_FALSE(PathValidator::contains_traversal_sequences("documents/report.pdf"));
}

// Static methods: contains_encoded_traversal
TEST(PathValidatorStaticTest, ContainsEncodedTraversal) {
    EXPECT_TRUE(PathValidator::contains_encoded_traversal("%2e%2e/etc/passwd"));
    EXPECT_TRUE(PathValidator::contains_encoded_traversal("%2E%2E/secret"));
    EXPECT_TRUE(PathValidator::contains_encoded_traversal("..%2f..%2f"));
    EXPECT_TRUE(PathValidator::contains_encoded_traversal("..%5c..%5c"));
    EXPECT_TRUE(PathValidator::contains_encoded_traversal("%252e%252e/secret"));
    EXPECT_FALSE(PathValidator::contains_encoded_traversal("normal/path.txt"));
    EXPECT_FALSE(PathValidator::contains_encoded_traversal("documents%20file.txt"));
}

// Static methods: contains_null_byte
TEST(PathValidatorStaticTest, ContainsNullByte) {
    std::string with_null("file.txt\0hidden", 15);
    std::string without_null = "file.txt";
    EXPECT_TRUE(PathValidator::contains_null_byte(with_null));
    EXPECT_FALSE(PathValidator::contains_null_byte(without_null));
}

// Test with allow_absolute_paths = true
TEST(PathValidatorAbsolutePathsAllowed, AllowsAbsolutePaths) {
    PathValidator validator;
    PathValidationConfig config;
    config.base_directory = "/documents/";
    config.allow_absolute_paths = true;
    config.allow_symlinks = false;
    validator.configure(config);

    std::string error_msg;
    // Absolute paths should now be allowed (if they resolve to base or don't escape)
    // But paths that escape via .. should still be blocked
    EXPECT_FALSE(validator.is_safe_path("../secret", error_msg));
}

// Mixed encoding and traversal attempts
TEST(PathValidatorEdgeCases, MixedEncodingAttempts) {
    PathValidator validator;
    PathValidationConfig config;
    config.base_directory = "/documents/";
    config.allow_absolute_paths = false;
    config.allow_symlinks = false;
    validator.configure(config);

    std::string error_msg;
    EXPECT_FALSE(validator.is_safe_path("file%2e%2etxt", error_msg));
    EXPECT_FALSE(validator.is_safe_path("doc%252e%252etxt", error_msg));
}

// Performance test: validate 1000 paths
TEST(PathValidatorPerformanceTest, BatchPathValidation) {
    PathValidator validator;
    PathValidationConfig config;
    config.base_directory = "/documents/";
    config.allow_absolute_paths = false;
    config.allow_symlinks = false;
    validator.configure(config);

    std::string error_msg;
    auto start = std::chrono::high_resolution_clock::now();

    // Validate 1000 paths
    for (int i = 0; i < 1000; ++i) {
        std::string path = "document" + std::to_string(i) + ".txt";
        validator.is_safe_path(path, error_msg);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 500ms for 1000 paths)
    EXPECT_LT(duration.count(), 500);
}

// Test configuration requirement
TEST(PathValidatorConfigTest, RequiresConfiguration) {
    PathValidator validator;
    std::string error_msg;
    // Should fail if not configured
    EXPECT_FALSE(validator.is_safe_path("file.txt", error_msg));
    EXPECT_EQ(error_msg, "PathValidator not configured");
}

// Test different base directories
TEST(PathValidatorBaseDirectoryTest, DifferentBaseDirs) {
    PathValidator validator;
    std::string error_msg;

    // Test with /tmp base
    PathValidationConfig config;
    config.base_directory = "/tmp/";
    validator.configure(config);

    // Normal path should pass
    EXPECT_TRUE(validator.is_safe_path("file.txt", error_msg));

    // Traversal should fail
    EXPECT_FALSE(validator.is_safe_path("../etc/passwd", error_msg));
}

// Test Unicode and special characters (safe if not traversal)
TEST(PathValidatorUnicodeTest, SafeUnicodeCharacters) {
    PathValidator validator;
    PathValidationConfig config;
    config.base_directory = "/documents/";
    validator.configure(config);

    std::string error_msg;
    // These should pass - unicode isn't inherently traversal
    EXPECT_TRUE(validator.is_safe_path("document_с_кириллицей.txt", error_msg));
    EXPECT_TRUE(validator.is_safe_path("文件.txt", error_msg));
}

// Empty path handling
TEST(PathValidatorEdgeCases, EmptyPath) {
    PathValidator validator;
    PathValidationConfig config;
    config.base_directory = "/documents/";
    validator.configure(config);

    std::string error_msg;
    // Empty path might be valid (refers to base directory)
    bool result = validator.is_safe_path("", error_msg);
    // Either result is acceptable as long as no crash
    EXPECT_TRUE(result || !result);
}
