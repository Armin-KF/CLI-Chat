#include <gtest/gtest.h>
#include "database.hpp"
#include "crypto.hpp"
#include <filesystem>

using namespace chat;

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = "test_chat.db";
        // Remove test database if it exists
        std::filesystem::remove(db_path_);

        db_ = std::make_unique<Database>(db_path_);
        ASSERT_TRUE(db_->initialize());
    }

    void TearDown() override {
        db_.reset();
        std::filesystem::remove(db_path_);
    }

    std::unique_ptr<Database> db_;
    std::string db_path_;
};

TEST_F(DatabaseTest, UserCreationAndRetrieval) {
    std::string username = "testuser";
    std::string password = "testpassword";
    std::string password_hash = CryptoUtils::hashPassword(password);
    std::string email = "test@example.com";

    // Create user
    EXPECT_TRUE(db_->createUser(username, password_hash, email));

    // Retrieve user
    auto user = db_->getUserByUsername(username);
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->username, username);
    EXPECT_EQ(user->password_hash, password_hash);
    EXPECT_EQ(user->email, email);
    EXPECT_TRUE(user->is_active);

    // Verify password
    EXPECT_TRUE(CryptoUtils::verifyPassword(password, user->password_hash));

    // Try to create duplicate user
    EXPECT_FALSE(db_->createUser(username, password_hash, email));
}

TEST_F(DatabaseTest, MessageStorage) {
    // First create a user
    std::string username = "testuser";
    std::string password_hash = CryptoUtils::hashPassword("password");
    ASSERT_TRUE(db_->createUser(username, password_hash));

    auto user = db_->getUserByUsername(username);
    ASSERT_NE(user, nullptr);

    // Create message
    Message msg;
    msg.user_id = user->id;
    msg.username = username;
    msg.content = "Hello, World!";
    msg.timestamp = std::chrono::system_clock::now();
    msg.is_private = false;

    // Save message
    EXPECT_TRUE(db_->saveMessage(msg));

    // Retrieve messages
    auto messages = db_->getRecentMessages(10);
    EXPECT_FALSE(messages.empty());
    EXPECT_EQ(messages[0].content, "Hello, World!");
    EXPECT_EQ(messages[0].username, username);
}

TEST_F(DatabaseTest, SessionManagement) {
    // Create user first
    std::string username = "testuser";
    std::string password_hash = CryptoUtils::hashPassword("password");
    ASSERT_TRUE(db_->createUser(username, password_hash));

    auto user = db_->getUserByUsername(username);
    ASSERT_NE(user, nullptr);

    // Create session
    Session session;
    session.token = CryptoUtils::generateSessionToken();
    session.user_id = user->id;
    session.username = username;
    session.created_at = std::chrono::system_clock::now();
    session.expires_at = session.created_at + std::chrono::hours(1);
    session.is_active = true;

    // Save session
    EXPECT_TRUE(db_->createSession(session));

    // Retrieve session
    auto retrieved_session = db_->getSession(session.token);
    ASSERT_NE(retrieved_session, nullptr);
    EXPECT_EQ(retrieved_session->token, session.token);
    EXPECT_EQ(retrieved_session->username, username);
    EXPECT_TRUE(retrieved_session->is_active);

    // Delete session
    EXPECT_TRUE(db_->deleteSession(session.token));

    // Session should no longer exist
    auto deleted_session = db_->getSession(session.token);
    EXPECT_EQ(deleted_session, nullptr);
}

TEST_F(DatabaseTest, Statistics) {
    // Initial counts
    EXPECT_EQ(db_->getUserCount(), 0);
    EXPECT_EQ(db_->getMessageCount(), 0);

    // Create user
    std::string username = "testuser";
    std::string password_hash = CryptoUtils::hashPassword("password");
    ASSERT_TRUE(db_->createUser(username, password_hash));

    EXPECT_EQ(db_->getUserCount(), 1);

    // Create message
    auto user = db_->getUserByUsername(username);
    Message msg;
    msg.user_id = user->id;
    msg.username = username;
    msg.content = "Test message";
    msg.timestamp = std::chrono::system_clock::now();
    msg.is_private = false;

    ASSERT_TRUE(db_->saveMessage(msg));
    EXPECT_EQ(db_->getMessageCount(), 1);
}