#pragma once

#include <string>
#include <memory>

namespace chat {

class SecretsManager {
public:
    static SecretsManager& getInstance();

    // Initialize secrets manager with secure file path
    bool initialize(const std::string& secrets_file_path);

    // Get secrets (never logs or exposes in memory dumps)
    std::string getRedisPassword() const;
    std::string getDatabasePassword() const;
    std::string getSessionSecret() const;

    // Check if a secret exists without exposing its value
    bool hasRedisPassword() const;
    bool hasDatabasePassword() const;
    bool hasSessionSecret() const;

    // Clear secrets from memory (called on shutdown)
    void clearSecrets();

private:
    SecretsManager() = default;
    ~SecretsManager();

    // Secure memory management
    void secureMemoryWipe(std::string& str);
    bool loadSecretsFromFile(const std::string& file_path);
    bool validateSecretStrength(const std::string& secret) const;

    // Use secure allocators for sensitive data
    std::unique_ptr<std::string> redis_password_;
    std::unique_ptr<std::string> database_password_;
    std::unique_ptr<std::string> session_secret_;

    bool initialized_ = false;
    std::string secrets_file_path_;
};

} // namespace chat