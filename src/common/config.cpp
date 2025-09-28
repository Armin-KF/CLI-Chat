#include "config.hpp"
#include "secrets.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace chat {

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

void Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file: " << filename << std::endl;
        setDefaults();
        return;
    }

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

        if (key == "server_host") server_host_ = value;
        else if (key == "server_port") server_port_ = std::stoi(value);
        else if (key == "tls_enabled") tls_enabled_ = (value == "true");
        else if (key == "cert_file") cert_file_ = value;
        else if (key == "key_file") key_file_ = value;
        else if (key == "database_path") database_path_ = value;
        else if (key == "redis_host") redis_host_ = value;
        else if (key == "redis_port") redis_port_ = std::stoi(value);
        // NOTE: redis_password is now handled by SecretsManager
        else if (key == "max_users") max_users_ = std::stoi(value);
        else if (key == "max_message_length") max_message_length_ = std::stoi(value);
        else if (key == "session_timeout") session_timeout_ = std::stoi(value);
        else if (key == "web_port") web_port_ = std::stoi(value);
        else if (key == "web_root") web_root_ = value;
    }
}

void Config::loadFromEnv() {
    const char* env_val;

    if ((env_val = std::getenv("CHAT_SERVER_HOST"))) server_host_ = env_val;
    if ((env_val = std::getenv("CHAT_SERVER_PORT"))) server_port_ = std::stoi(env_val);
    if ((env_val = std::getenv("CHAT_TLS_ENABLED"))) tls_enabled_ = (std::string(env_val) == "true");
    if ((env_val = std::getenv("CHAT_CERT_FILE"))) cert_file_ = env_val;
    if ((env_val = std::getenv("CHAT_KEY_FILE"))) key_file_ = env_val;
    if ((env_val = std::getenv("CHAT_DATABASE_PATH"))) database_path_ = env_val;
    if ((env_val = std::getenv("CHAT_REDIS_HOST"))) redis_host_ = env_val;
    if ((env_val = std::getenv("CHAT_REDIS_PORT"))) redis_port_ = std::stoi(env_val);
    // SECURITY: Redis password no longer loaded from environment variables
    // Use secrets file instead: /etc/cli-chat/secrets or ./secrets/secrets.conf
    if ((env_val = std::getenv("CHAT_MAX_USERS"))) max_users_ = std::stoi(env_val);
    if ((env_val = std::getenv("CHAT_MAX_MESSAGE_LENGTH"))) max_message_length_ = std::stoi(env_val);
    if ((env_val = std::getenv("CHAT_SESSION_TIMEOUT"))) session_timeout_ = std::stoi(env_val);
    if ((env_val = std::getenv("CHAT_WEB_PORT"))) web_port_ = std::stoi(env_val);
    if ((env_val = std::getenv("CHAT_WEB_ROOT"))) web_root_ = env_val;
}

std::string Config::getRedisPassword() const {
    return SecretsManager::getInstance().getRedisPassword();
}

void Config::setDefaults() {
    // Defaults are already set in the header file

    // Initialize secrets manager
    SecretsManager::getInstance().initialize("secrets/secrets.conf");
}

} // namespace chat