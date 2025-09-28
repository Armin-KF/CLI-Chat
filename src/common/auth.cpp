#include "auth.hpp"
#include "database.hpp"
#include "crypto.hpp"
#include "config.hpp"
#include <regex>

namespace chat {

const std::chrono::hours AuthManager::SESSION_DURATION = std::chrono::hours(24);

AuthResult AuthManager::authenticate(const std::string& username, const std::string& password) {
    AuthResult result;
    result.success = false;

    if (!validateUsername(username) || !validatePassword(password)) {
        result.message = "Invalid username or password format";
        return result;
    }

    auto& config = Config::getInstance();
    Database db(config.getDatabasePath());

    if (!db.initialize()) {
        result.message = "Database connection failed";
        return result;
    }

    auto user = db.getUserByUsername(username);
    if (!user) {
        result.message = "User not found";
        return result;
    }

    if (!user->is_active) {
        result.message = "Account disabled";
        return result;
    }

    if (!CryptoUtils::verifyPassword(password, user->password_hash)) {
        result.message = "Invalid password";
        return result;
    }

    // Create session
    auto token = CryptoUtils::generateSessionToken();
    auto now = std::chrono::system_clock::now();
    auto expires = now + SESSION_DURATION;

    Session session;
    session.token = token;
    session.user_id = user->id;
    session.username = username;
    session.created_at = now;
    session.expires_at = expires;
    session.is_active = true;

    if (!db.createSession(session)) {
        result.message = "Failed to create session";
        return result;
    }

    // Update last login
    db.updateLastLogin(user->id);

    result.success = true;
    result.token = token;
    result.user_id = user->id;
    result.username = username;
    result.message = "Authentication successful";

    return result;
}

AuthResult AuthManager::registerUser(const std::string& username, const std::string& password, const std::string& email) {
    AuthResult result;
    result.success = false;

    if (!validateUsername(username)) {
        result.message = "Invalid username format";
        return result;
    }

    if (!validatePassword(password)) {
        result.message = "Password too weak";
        return result;
    }

    if (!email.empty() && !validateEmail(email)) {
        result.message = "Invalid email format";
        return result;
    }

    auto& config = Config::getInstance();
    Database db(config.getDatabasePath());

    if (!db.initialize()) {
        result.message = "Database connection failed";
        return result;
    }

    // Check if user already exists
    auto existing_user = db.getUserByUsername(username);
    if (existing_user) {
        result.message = "Username already taken";
        return result;
    }

    // Hash password
    auto password_hash = CryptoUtils::hashPassword(password);

    // Create user
    if (!db.createUser(username, password_hash, email)) {
        result.message = "Failed to create user";
        return result;
    }

    result.success = true;
    result.message = "Registration successful";

    return result;
}

bool AuthManager::validateSession(const std::string& token, std::string& username, int& user_id) {
    if (token.empty()) return false;

    auto& config = Config::getInstance();
    Database db(config.getDatabasePath());

    if (!db.initialize()) return false;

    auto session = db.getSession(token);
    if (!session || !session->is_active) return false;

    // Check if session is expired
    auto now = std::chrono::system_clock::now();
    if (now > session->expires_at) {
        db.deleteSession(token);
        return false;
    }

    username = session->username;
    user_id = session->user_id;
    return true;
}

bool AuthManager::invalidateSession(const std::string& token) {
    if (token.empty()) return false;

    auto& config = Config::getInstance();
    Database db(config.getDatabasePath());

    if (!db.initialize()) return false;

    return db.deleteSession(token);
}

bool AuthManager::validateUsername(const std::string& username) {
    if (username.empty() || username.length() > MAX_USERNAME_LENGTH) {
        return false;
    }

    // Allow alphanumeric, underscore, and dash
    std::regex pattern("^[a-zA-Z0-9_-]+$");
    return std::regex_match(username, pattern);
}

bool AuthManager::validatePassword(const std::string& password) {
    return password.length() >= MIN_PASSWORD_LENGTH;
}

bool AuthManager::validateEmail(const std::string& email) {
    if (email.empty()) return true; // Email is optional

    std::regex pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, pattern);
}

} // namespace chat