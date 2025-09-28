#pragma once

#include <string>
#include <map>
#include <memory>

namespace chat {

class Config {
public:
    static Config& getInstance();

    void loadFromFile(const std::string& filename);
    void loadFromEnv();

    // Server configuration
    std::string getServerHost() const { return server_host_; }
    int getServerPort() const { return server_port_; }
    bool getTLSEnabled() const { return tls_enabled_; }
    std::string getCertFile() const { return cert_file_; }
    std::string getKeyFile() const { return key_file_; }

    // Database configuration
    std::string getDatabasePath() const { return database_path_; }

    // Redis configuration
    std::string getRedisHost() const { return redis_host_; }
    int getRedisPort() const { return redis_port_; }
    std::string getRedisPassword() const { return redis_password_; }

    // Security configuration
    int getMaxUsers() const { return max_users_; }
    int getMaxMessageLength() const { return max_message_length_; }
    int getSessionTimeout() const { return session_timeout_; }

    // Web configuration
    int getWebPort() const { return web_port_; }
    std::string getWebRoot() const { return web_root_; }

private:
    Config() = default;
    void setDefaults();

    std::string server_host_ = "127.0.0.1";
    int server_port_ = 4000;
    bool tls_enabled_ = true;
    std::string cert_file_ = "certs/server.crt";
    std::string key_file_ = "certs/server.key";

    std::string database_path_ = "chat.db";

    std::string redis_host_ = "127.0.0.1";
    int redis_port_ = 6379;
    std::string redis_password_ = "";

    int max_users_ = 1000;
    int max_message_length_ = 4096;
    int session_timeout_ = 3600;

    int web_port_ = 8080;
    std::string web_root_ = "frontend/dist";
};

} // namespace chat