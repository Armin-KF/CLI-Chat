#include "database.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace chat {

Database::Database(const std::string& db_path) : db_(nullptr), db_path_(db_path) {}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool Database::initialize() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // Enable foreign keys
    executeQuery("PRAGMA foreign_keys = ON;");

    return createTables();
}

bool Database::createTables() {
    std::vector<std::string> create_queries = {
        R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            email TEXT,
            created_at TEXT NOT NULL,
            last_login TEXT,
            is_active BOOLEAN DEFAULT 1
        );
        )",

        R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            username TEXT NOT NULL,
            content TEXT NOT NULL,
            recipient TEXT,
            timestamp TEXT NOT NULL,
            is_private BOOLEAN DEFAULT 0,
            FOREIGN KEY (user_id) REFERENCES users (id)
        );
        )",

        R"(
        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL,
            username TEXT NOT NULL,
            created_at TEXT NOT NULL,
            expires_at TEXT NOT NULL,
            is_active BOOLEAN DEFAULT 1,
            FOREIGN KEY (user_id) REFERENCES users (id)
        );
        )",

        R"(
        CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp);
        )",

        R"(
        CREATE INDEX IF NOT EXISTS idx_messages_user_id ON messages(user_id);
        )",

        R"(
        CREATE INDEX IF NOT EXISTS idx_sessions_expires_at ON sessions(expires_at);
        )"
    };

    for (const auto& query : create_queries) {
        if (!executeQuery(query)) {
            return false;
        }
    }

    return true;
}

bool Database::executeQuery(const std::string& query) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

bool Database::createUser(const std::string& username, const std::string& password_hash, const std::string& email) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (username, password_hash, email, created_at) VALUES (?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    auto now = std::chrono::system_clock::now();
    std::string now_str = timePointToString(now);

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, email.empty() ? nullptr : email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, now_str.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::unique_ptr<User> Database::getUserByUsername(const std::string& username) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, username, password_hash, email, created_at, last_login, is_active FROM users WHERE username = ?;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return nullptr;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto user = std::make_unique<User>();
        user->id = sqlite3_column_int(stmt, 0);
        user->username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user->password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user->email = email ? email : "";

        const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        user->created_at = stringToTimePoint(created_at);

        const char* last_login = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (last_login) {
            user->last_login = stringToTimePoint(last_login);
        }

        user->is_active = sqlite3_column_int(stmt, 6) != 0;

        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return nullptr;
}

std::unique_ptr<User> Database::getUserById(int user_id) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, username, password_hash, email, created_at, last_login, is_active FROM users WHERE id = ?;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return nullptr;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto user = std::make_unique<User>();
        user->id = sqlite3_column_int(stmt, 0);
        user->username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user->password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user->email = email ? email : "";

        const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        user->created_at = stringToTimePoint(created_at);

        const char* last_login = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (last_login) {
            user->last_login = stringToTimePoint(last_login);
        }

        user->is_active = sqlite3_column_int(stmt, 6) != 0;

        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return nullptr;
}

bool Database::updateLastLogin(int user_id) {
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET last_login = ? WHERE id = ?;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    std::string now_str = timePointToString(now);

    sqlite3_bind_text(stmt, 1, now_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::saveMessage(const Message& message) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO messages (user_id, username, content, recipient, timestamp, is_private) VALUES (?, ?, ?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    std::string timestamp_str = timePointToString(message.timestamp);

    sqlite3_bind_int(stmt, 1, message.user_id);
    sqlite3_bind_text(stmt, 2, message.username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message.content.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, message.recipient.empty() ? nullptr : message.recipient.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, timestamp_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, message.is_private ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::vector<Message> Database::getRecentMessages(int limit) {
    std::vector<Message> messages;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, user_id, username, content, recipient, timestamp, is_private FROM messages WHERE is_private = 0 ORDER BY timestamp DESC LIMIT ?;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return messages;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(stmt, 0);
        msg.user_id = sqlite3_column_int(stmt, 1);
        msg.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        const char* recipient = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.recipient = recipient ? recipient : "";

        const char* timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        msg.timestamp = stringToTimePoint(timestamp);

        msg.is_private = sqlite3_column_int(stmt, 6) != 0;

        messages.push_back(msg);
    }

    sqlite3_finalize(stmt);

    // Reverse to get chronological order
    std::reverse(messages.begin(), messages.end());
    return messages;
}

bool Database::createSession(const Session& session) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO sessions (token, user_id, username, created_at, expires_at, is_active) VALUES (?, ?, ?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    std::string created_str = timePointToString(session.created_at);
    std::string expires_str = timePointToString(session.expires_at);

    sqlite3_bind_text(stmt, 1, session.token.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, session.user_id);
    sqlite3_bind_text(stmt, 3, session.username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, created_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, expires_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, session.is_active ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::unique_ptr<Session> Database::getSession(const std::string& token) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT token, user_id, username, created_at, expires_at, is_active FROM sessions WHERE token = ? AND is_active = 1;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return nullptr;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto session = std::make_unique<Session>();
        session->token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        session->user_id = sqlite3_column_int(stmt, 1);
        session->username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        session->created_at = stringToTimePoint(created_at);

        const char* expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        session->expires_at = stringToTimePoint(expires_at);

        session->is_active = sqlite3_column_int(stmt, 5) != 0;

        sqlite3_finalize(stmt);
        return session;
    }

    sqlite3_finalize(stmt);
    return nullptr;
}

bool Database::deleteSession(const std::string& token) {
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE sessions SET is_active = 0 WHERE token = ?;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::string Database::timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto tm = *std::gmtime(&time_t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::chrono::system_clock::time_point Database::stringToTimePoint(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

int Database::getUserCount() {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM users WHERE is_active = 1;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

int Database::getMessageCount() {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM messages;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

} // namespace chat