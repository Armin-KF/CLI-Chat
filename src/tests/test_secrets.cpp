#include <gtest/gtest.h>
#include "secrets.hpp"
#include <filesystem>
#include <fstream>

using namespace chat;

class SecretsTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_secrets_file_ = "test_secrets.conf";
        // Clean up any existing test file
        std::filesystem::remove(test_secrets_file_);
    }

    void TearDown() override {
        // Clean up test file
        std::filesystem::remove(test_secrets_file_);

        // Clear secrets manager
        SecretsManager::getInstance().clearSecrets();
    }

    void createTestSecretsFile(const std::string& content) {
        std::ofstream file(test_secrets_file_);
        file << content;
        file.close();
    }

    std::string test_secrets_file_;
};

TEST_F(SecretsTest, InitializationWithoutFile) {
    SecretsManager& sm = SecretsManager::getInstance();

    // Should initialize successfully even without secrets file
    EXPECT_TRUE(sm.initialize("nonexistent.conf"));

    // Should return empty passwords when no file exists
    EXPECT_EQ(sm.getRedisPassword(), "");
    EXPECT_FALSE(sm.hasRedisPassword());
}

TEST_F(SecretsTest, LoadValidSecrets) {
    createTestSecretsFile(R"(
# Test secrets
redis_password=TestRedisPass123!
database_password=TestDBPass456!
session_secret=TestSessionSecret789!AbCdEf
)");

    SecretsManager& sm = SecretsManager::getInstance();
    EXPECT_TRUE(sm.initialize(test_secrets_file_));

    EXPECT_EQ(sm.getRedisPassword(), "TestRedisPass123!");
    EXPECT_TRUE(sm.hasRedisPassword());
    EXPECT_TRUE(sm.hasDatabasePassword());
    EXPECT_TRUE(sm.hasSessionSecret());
}

TEST_F(SecretsTest, RejectWeakPasswords) {
    // Test various weak passwords
    std::vector<std::string> weak_passwords = {
        "password",      // Common word
        "123456",        // Too short and common
        "admin",         // Too short and common
        "Password123",   // Contains "password"
        "short",         // Too short
        "UPPERCASE",     // No lowercase or numbers
        "lowercase",     // No uppercase or numbers
        "12345678"       // No letters
    };

    for (const auto& weak_pass : weak_passwords) {
        createTestSecretsFile("redis_password=" + weak_pass);

        SecretsManager& sm = SecretsManager::getInstance();
        // Should fail to initialize with weak passwords
        EXPECT_FALSE(sm.initialize(test_secrets_file_))
            << "Weak password should be rejected: " << weak_pass;

        sm.clearSecrets();
        std::filesystem::remove(test_secrets_file_);
    }
}

TEST_F(SecretsTest, AcceptStrongPasswords) {
    std::vector<std::string> strong_passwords = {
        "StrongPass123!",
        "My$ecureP@ss456",
        "ComplexPassword789#",
        "Rand0mStr1ng!@#$%"
    };

    for (const auto& strong_pass : strong_passwords) {
        createTestSecretsFile("redis_password=" + strong_pass);

        SecretsManager& sm = SecretsManager::getInstance();
        EXPECT_TRUE(sm.initialize(test_secrets_file_))
            << "Strong password should be accepted: " << strong_pass;

        EXPECT_EQ(sm.getRedisPassword(), strong_pass);

        sm.clearSecrets();
        std::filesystem::remove(test_secrets_file_);
    }
}

TEST_F(SecretsTest, SecureMemoryClearing) {
    createTestSecretsFile("redis_password=TestPassword123!");

    SecretsManager& sm = SecretsManager::getInstance();
    EXPECT_TRUE(sm.initialize(test_secrets_file_));

    EXPECT_TRUE(sm.hasRedisPassword());

    // Clear secrets
    sm.clearSecrets();

    // Should no longer have passwords
    EXPECT_FALSE(sm.hasRedisPassword());
    EXPECT_EQ(sm.getRedisPassword(), "");
}

TEST_F(SecretsTest, IgnoreComentsAndEmptyLines) {
    createTestSecretsFile(R"(
# This is a comment
redis_password=ValidPass123!

# Another comment
database_password=AnotherValidPass456!

# Empty lines should be ignored
)");

    SecretsManager& sm = SecretsManager::getInstance();
    EXPECT_TRUE(sm.initialize(test_secrets_file_));

    EXPECT_EQ(sm.getRedisPassword(), "ValidPass123!");
    EXPECT_EQ(sm.getDatabasePassword(), "AnotherValidPass456!");
}