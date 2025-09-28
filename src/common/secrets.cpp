#include "secrets.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <regex>

namespace chat {

SecretsManager& SecretsManager::getInstance() {
    static SecretsManager instance;
    return instance;
}

SecretsManager::~SecretsManager() {
    clearSecrets();
}

bool SecretsManager::initialize(const std::string& secrets_file_path) {
    if (initialized_) {
        std::cerr << "[WARN] SecretsManager already initialized" << std::endl;
        return true;
    }

    secrets_file_path_ = secrets_file_path;

    // Try to load secrets from file
    if (!loadSecretsFromFile(secrets_file_path)) {
        // If no secrets file exists, generate secure defaults
        std::cout << "[INFO] No secrets file found, using secure defaults" << std::endl;

        // Set empty passwords - require explicit configuration
        redis_password_ = std::make_unique<std::string>("");
        database_password_ = std::make_unique<std::string>("");
        session_secret_ = std::make_unique<std::string>("");
    }

    initialized_ = true;
    return true;
}

bool SecretsManager::loadSecretsFromFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return false;
    }

    // Set restrictive file permissions warning
    std::cout << "[INFO] Loading secrets from: " << file_path << std::endl;
    std::cout << "[SECURITY] Ensure secrets file has 0600 permissions (owner read/write only)" << std::endl;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Validate and store secrets
        if (key == "redis_password") {
            if (validateSecretStrength(value)) {
                redis_password_ = std::make_unique<std::string>(value);
            } else {
                std::cerr << "[ERROR] Redis password does not meet security requirements" << std::endl;
                return false;
            }
        }
        else if (key == "database_password") {
            if (validateSecretStrength(value)) {
                database_password_ = std::make_unique<std::string>(value);
            } else {
                std::cerr << "[ERROR] Database password does not meet security requirements" << std::endl;
                return false;
            }
        }
        else if (key == "session_secret") {
            if (validateSecretStrength(value)) {
                session_secret_ = std::make_unique<std::string>(value);
            } else {
                std::cerr << "[ERROR] Session secret does not meet security requirements" << std::endl;
                return false;
            }
        }

        // Immediately wipe the value from memory
        secureMemoryWipe(value);
    }

    file.close();
    return true;
}

bool SecretsManager::validateSecretStrength(const std::string& secret) const {
    if (secret.empty()) {
        return true; // Allow empty for optional secrets
    }

    // Minimum length requirement
    if (secret.length() < 12) {
        std::cerr << "[ERROR] Secret must be at least 12 characters long" << std::endl;
        return false;
    }

    // Maximum length to prevent DoS
    if (secret.length() > 256) {
        std::cerr << "[ERROR] Secret too long (max 256 characters)" << std::endl;
        return false;
    }

    // Check for common weak passwords
    std::vector<std::string> weak_passwords = {
        "password", "123456", "admin", "root", "test", "guest",
        "default", "redis", "database", "secret"
    };

    std::string lower_secret = secret;
    std::transform(lower_secret.begin(), lower_secret.end(), lower_secret.begin(), ::tolower);

    for (const auto& weak : weak_passwords) {
        if (lower_secret.find(weak) != std::string::npos) {
            std::cerr << "[ERROR] Secret contains weak/common password patterns" << std::endl;
            return false;
        }
    }

    // Check for minimum complexity
    bool has_upper = false, has_lower = false, has_digit = false, has_special = false;
    for (char c : secret) {
        if (c >= 'A' && c <= 'Z') has_upper = true;
        else if (c >= 'a' && c <= 'z') has_lower = true;
        else if (c >= '0' && c <= '9') has_digit = true;
        else has_special = true;
    }

    if (!(has_upper && has_lower && (has_digit || has_special))) {
        std::cerr << "[ERROR] Secret must contain uppercase, lowercase, and numbers/special characters" << std::endl;
        return false;
    }

    return true;
}

std::string SecretsManager::getRedisPassword() const {
    if (!initialized_ || !redis_password_) {
        return "";
    }
    return *redis_password_;
}

std::string SecretsManager::getDatabasePassword() const {
    if (!initialized_ || !database_password_) {
        return "";
    }
    return *database_password_;
}

std::string SecretsManager::getSessionSecret() const {
    if (!initialized_ || !session_secret_) {
        return "";
    }
    return *session_secret_;
}

bool SecretsManager::hasRedisPassword() const {
    return initialized_ && redis_password_ && !redis_password_->empty();
}

bool SecretsManager::hasDatabasePassword() const {
    return initialized_ && database_password_ && !database_password_->empty();
}

bool SecretsManager::hasSessionSecret() const {
    return initialized_ && session_secret_ && !session_secret_->empty();
}

void SecretsManager::clearSecrets() {
    if (redis_password_) {
        secureMemoryWipe(*redis_password_);
        redis_password_.reset();
    }
    if (database_password_) {
        secureMemoryWipe(*database_password_);
        database_password_.reset();
    }
    if (session_secret_) {
        secureMemoryWipe(*session_secret_);
        session_secret_.reset();
    }
    initialized_ = false;
}

void SecretsManager::secureMemoryWipe(std::string& str) {
    // Securely wipe memory to prevent secrets from remaining in memory
    if (!str.empty()) {
        volatile char* ptr = const_cast<volatile char*>(str.data());
        for (size_t i = 0; i < str.size(); ++i) {
            ptr[i] = '\0';
        }
        str.clear();
        str.shrink_to_fit();
    }
}

} // namespace chat