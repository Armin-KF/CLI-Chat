#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <sqlite3.h>

namespace chat {

struct User {
    int id;
    std::string username;
    std::string password_hash;
    std::string email;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_login;
    bool is_active;
};

struct Message {
    int id;
    int user_id;
    std::string username;
    std::string content;
    std::string recipient; // empty for broadcast messages
    std::chrono::system_clock::time_point timestamp;
    bool is_private;
};

struct Session {
    std::string token;
    int user_id;
    std::string username;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    bool is_active;
};

class Database {
public:
    Database(const std::string& db_path);
    ~Database();

    bool initialize();
    bool createTables();

    // User management
    bool createUser(const std::string& username, const std::string& password_hash, const std::string& email = "");
    std::unique_ptr<User> getUserByUsername(const std::string& username);
    std::unique_ptr<User> getUserById(int user_id);
    bool updateLastLogin(int user_id);
    std::vector<User> getActiveUsers();

    // Message management
    bool saveMessage(const Message& message);
    std::vector<Message> getRecentMessages(int limit = 100);
    std::vector<Message> getPrivateMessages(const std::string& user1, const std::string& user2, int limit = 50);
    std::vector<Message> getMessageHistory(const std::string& username, int limit = 100);

    // Session management
    bool createSession(const Session& session);
    std::unique_ptr<Session> getSession(const std::string& token);
    bool updateSession(const std::string& token);
    bool deleteSession(const std::string& token);
    bool cleanupExpiredSessions();

    // Statistics
    int getUserCount();
    int getMessageCount();
    std::vector<std::pair<std::string, int>> getTopActiveUsers(int limit = 10);

private:
    sqlite3* db_;
    std::string db_path_;

    bool executeQuery(const std::string& query);
    std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str);
};

} // namespace chat