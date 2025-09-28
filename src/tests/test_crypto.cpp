#include <gtest/gtest.h>
#include "crypto.hpp"

using namespace chat;

class CryptoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CryptoTest, PasswordHashing) {
    std::string password = "test_password_123";
    std::string hash = CryptoUtils::hashPassword(password);

    // Hash should not be empty
    EXPECT_FALSE(hash.empty());

    // Hash should contain salt separator
    EXPECT_NE(hash.find('$'), std::string::npos);

    // Verify password should work
    EXPECT_TRUE(CryptoUtils::verifyPassword(password, hash));

    // Wrong password should fail
    EXPECT_FALSE(CryptoUtils::verifyPassword("wrong_password", hash));
}

TEST_F(CryptoTest, TokenGeneration) {
    std::string token1 = CryptoUtils::generateSessionToken();
    std::string token2 = CryptoUtils::generateSessionToken();

    // Tokens should not be empty
    EXPECT_FALSE(token1.empty());
    EXPECT_FALSE(token2.empty());

    // Tokens should be different
    EXPECT_NE(token1, token2);

    // Token should be hex string of expected length
    EXPECT_EQ(token1.length(), 64); // 32 bytes * 2 hex chars
}

TEST_F(CryptoTest, SaltGeneration) {
    std::string salt1 = CryptoUtils::generateSalt();
    std::string salt2 = CryptoUtils::generateSalt();

    // Salts should not be empty
    EXPECT_FALSE(salt1.empty());
    EXPECT_FALSE(salt2.empty());

    // Salts should be different
    EXPECT_NE(salt1, salt2);

    // Salt should be hex string of expected length
    EXPECT_EQ(salt1.length(), 32); // 16 bytes * 2 hex chars
}

TEST_F(CryptoTest, MessageEncryption) {
    std::string message = "Hello, World!";
    std::string key = "encryption_key";

    std::string encrypted = CryptoUtils::encryptMessage(message, key);
    std::string decrypted = CryptoUtils::decryptMessage(encrypted, key);

    // Encrypted should be different from original
    EXPECT_NE(message, encrypted);

    // Decrypted should match original
    EXPECT_EQ(message, decrypted);
}

TEST_F(CryptoTest, TLSContextCreation) {
    // Test client context creation
    auto client_ctx = TLSContext::createClientContext();
    EXPECT_NE(client_ctx, nullptr);
}