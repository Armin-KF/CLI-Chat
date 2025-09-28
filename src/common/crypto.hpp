#pragma once

#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace chat {

class CryptoUtils {
public:
    // Password hashing using bcrypt-style
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& password, const std::string& hash);

    // Session token generation
    static std::string generateSessionToken();
    static std::string generateSalt();

    // Message encryption/decryption
    static std::string encryptMessage(const std::string& message, const std::string& key);
    static std::string decryptMessage(const std::string& encrypted, const std::string& key);

private:
    static const int SALT_LENGTH = 16;
    static const int TOKEN_LENGTH = 32;
};

class TLSContext {
public:
    static std::shared_ptr<boost::asio::ssl::context> createServerContext(
        const std::string& cert_file,
        const std::string& key_file
    );

    static std::shared_ptr<boost::asio::ssl::context> createClientContext();

    // Certificate generation for development
    static bool generateSelfSignedCert(
        const std::string& cert_file,
        const std::string& key_file,
        const std::string& common_name = "localhost"
    );
};

} // namespace chat