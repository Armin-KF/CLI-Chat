#include "crypto.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <random>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace chat {

std::string CryptoUtils::hashPassword(const std::string& password) {
    std::string salt = generateSalt();

    // Simple PBKDF2-like implementation
    unsigned char hash[SHA256_DIGEST_LENGTH];
    std::string salted_password = password + salt;

    SHA256(reinterpret_cast<const unsigned char*>(salted_password.c_str()),
           salted_password.length(), hash);

    // Convert to hex and prepend salt
    std::stringstream ss;
    ss << salt << "$";
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

bool CryptoUtils::verifyPassword(const std::string& password, const std::string& hash) {
    size_t pos = hash.find('$');
    if (pos == std::string::npos) return false;

    std::string salt = hash.substr(0, pos);
    std::string stored_hash = hash.substr(pos + 1);

    // Hash the provided password with the stored salt
    unsigned char computed_hash[SHA256_DIGEST_LENGTH];
    std::string salted_password = password + salt;

    SHA256(reinterpret_cast<const unsigned char*>(salted_password.c_str()),
           salted_password.length(), computed_hash);

    // Convert to hex
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(computed_hash[i]);
    }

    return ss.str() == stored_hash;
}

std::string CryptoUtils::generateSessionToken() {
    unsigned char buffer[TOKEN_LENGTH];
    if (RAND_bytes(buffer, TOKEN_LENGTH) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }

    std::stringstream ss;
    for (int i = 0; i < TOKEN_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
    }

    return ss.str();
}

std::string CryptoUtils::generateSalt() {
    unsigned char buffer[SALT_LENGTH];
    if (RAND_bytes(buffer, SALT_LENGTH) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }

    std::stringstream ss;
    for (int i = 0; i < SALT_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
    }

    return ss.str();
}

std::string CryptoUtils::encryptMessage(const std::string& message, const std::string& key) {
    // Simple XOR encryption for demonstration (use AES in production)
    std::string encrypted = message;
    for (size_t i = 0; i < encrypted.length(); ++i) {
        encrypted[i] ^= key[i % key.length()];
    }
    return encrypted;
}

std::string CryptoUtils::decryptMessage(const std::string& encrypted, const std::string& key) {
    // XOR decryption (same as encryption for XOR)
    return encryptMessage(encrypted, key);
}

std::shared_ptr<boost::asio::ssl::context> TLSContext::createServerContext(
    const std::string& cert_file,
    const std::string& key_file
) {
    auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);

    ctx->set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::no_sslv3 |
        boost::asio::ssl::context::single_dh_use
    );

    try {
        ctx->use_certificate_chain_file(cert_file);
        ctx->use_private_key_file(key_file, boost::asio::ssl::context::pem);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load SSL certificates: " + std::string(e.what()));
    }

    return ctx;
}

std::shared_ptr<boost::asio::ssl::context> TLSContext::createClientContext() {
    auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);

    ctx->set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::no_sslv3
    );

    ctx->set_verify_mode(boost::asio::ssl::verify_none); // For self-signed certs

    return ctx;
}

bool TLSContext::generateSelfSignedCert(
    const std::string& cert_file,
    const std::string& key_file,
    const std::string& common_name
) {
    // Generate RSA key
    EVP_PKEY* pkey = EVP_PKEY_new();
    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();

    if (!pkey || !rsa || !bn) {
        return false;
    }

    BN_set_word(bn, RSA_F4);
    if (RSA_generate_key_ex(rsa, 2048, bn, nullptr) != 1) {
        BN_free(bn);
        RSA_free(rsa);
        EVP_PKEY_free(pkey);
        return false;
    }

    EVP_PKEY_assign_RSA(pkey, rsa);

    // Generate certificate
    X509* x509 = X509_new();
    if (!x509) {
        BN_free(bn);
        EVP_PKEY_free(pkey);
        return false;
    }

    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L); // 1 year

    X509_set_pubkey(x509, pkey);

    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC,
                              reinterpret_cast<const unsigned char*>("US"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                              reinterpret_cast<const unsigned char*>("CLI-Chat"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                              reinterpret_cast<const unsigned char*>(common_name.c_str()), -1, -1, 0);

    X509_set_issuer_name(x509, name);
    X509_sign(x509, pkey, EVP_sha256());

    // Write certificate file
    FILE* cert_fp = fopen(cert_file.c_str(), "w");
    if (!cert_fp) {
        BN_free(bn);
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return false;
    }

    PEM_write_X509(cert_fp, x509);
    fclose(cert_fp);

    // Write private key file
    FILE* key_fp = fopen(key_file.c_str(), "w");
    if (!key_fp) {
        BN_free(bn);
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return false;
    }

    PEM_write_PrivateKey(key_fp, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(key_fp);

    BN_free(bn);
    X509_free(x509);
    EVP_PKEY_free(pkey);

    return true;
}

} // namespace chat