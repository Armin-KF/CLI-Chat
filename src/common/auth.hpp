#pragma once

#include <string>
#include <chrono>

namespace chat {

struct AuthResult {
    bool success;
    std::string message;
    std::string token;
    int user_id;
    std::string username;
};

class AuthManager {
public:
    static AuthResult authenticate(const std::string& username, const std::string& password);
    static AuthResult registerUser(const std::string& username, const std::string& password, const std::string& email = "");
    static bool validateSession(const std::string& token, std::string& username, int& user_id);
    static bool invalidateSession(const std::string& token);

    // Input validation
    static bool validateUsername(const std::string& username);
    static bool validatePassword(const std::string& password);
    static bool validateEmail(const std::string& email);

private:
    static const size_t MIN_PASSWORD_LENGTH = 6;
    static const size_t MAX_USERNAME_LENGTH = 32;
    static const std::chrono::hours SESSION_DURATION;
};

} // namespace chat