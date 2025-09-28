#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include "database.hpp"

namespace chat {

class ChatServer;

class Session : public std::enable_shared_from_this<Session> {
public:
    using ssl_socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    Session(boost::asio::io_context& io_context,
            std::shared_ptr<boost::asio::ssl::context> ssl_context,
            ChatServer& server);

    ssl_socket& socket() { return socket_; }
    void start();
    void stop();
    void sendMessage(const std::string& message);

    const std::string& getUsername() const { return username_; }
    bool isAuthenticated() const { return authenticated_; }
    int getUserId() const { return user_id_; }

private:
    void handleHandshake(const boost::system::error_code& error);
    void startRead();
    void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
    void handleWrite(const boost::system::error_code& error);
    void processMessage(const std::string& message);

    // Authentication
    bool handleLogin(const std::string& username, const std::string& password);
    bool handleRegister(const std::string& username, const std::string& password, const std::string& email);
    void sendAuthResponse(bool success, const std::string& message = "");

    // Message handling
    void handlePrivateMessage(const std::string& recipient, const std::string& content);
    void handleBroadcastMessage(const std::string& content);

    ssl_socket socket_;
    ChatServer& server_;
    boost::asio::streambuf read_buffer_;
    std::queue<std::string> write_queue_;
    std::mutex write_mutex_;

    std::string username_;
    int user_id_;
    bool authenticated_;
    std::string session_token_;

    static const size_t MAX_MESSAGE_LENGTH = 4096;
};

} // namespace chat