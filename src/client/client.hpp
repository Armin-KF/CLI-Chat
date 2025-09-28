#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include "config.hpp"

namespace chat {

class ChatClient {
public:
    using ssl_socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    ChatClient(boost::asio::io_context& io_context);
    ~ChatClient();

    bool connect(const std::string& host, int port);
    void disconnect();

    bool login(const std::string& username, const std::string& password);
    bool registerUser(const std::string& username, const std::string& password, const std::string& email = "");
    void logout();

    void sendMessage(const std::string& message);
    void sendPrivateMessage(const std::string& recipient, const std::string& message);
    void requestUserList();

    bool isConnected() const { return connected_; }
    bool isAuthenticated() const { return authenticated_; }
    const std::string& getUsername() const { return username_; }

    // Message handling callbacks
    void setMessageCallback(std::function<void(const std::string&)> callback) {
        message_callback_ = callback;
    }

    void setPrivateMessageCallback(std::function<void(const std::string&, const std::string&)> callback) {
        private_message_callback_ = callback;
    }

    void setUserListCallback(std::function<void(const std::vector<std::string>&)> callback) {
        user_list_callback_ = callback;
    }

    void setSystemMessageCallback(std::function<void(const std::string&)> callback) {
        system_message_callback_ = callback;
    }

private:
    void handleConnect(const boost::system::error_code& error);
    void handleHandshake(const boost::system::error_code& error);
    void startRead();
    void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
    void handleWrite(const boost::system::error_code& error);

    void processMessage(const std::string& message);
    void sendRawMessage(const std::string& message);

    boost::asio::io_context& io_context_;
    std::unique_ptr<ssl_socket> socket_;
    std::shared_ptr<boost::asio::ssl::context> ssl_context_;

    boost::asio::streambuf read_buffer_;
    std::queue<std::string> write_queue_;
    std::mutex write_mutex_;

    std::atomic<bool> connected_;
    std::atomic<bool> authenticated_;
    std::string username_;

    // Callbacks
    std::function<void(const std::string&)> message_callback_;
    std::function<void(const std::string&, const std::string&)> private_message_callback_;
    std::function<void(const std::vector<std::string>&)> user_list_callback_;
    std::function<void(const std::string&)> system_message_callback_;

    Config& config_;
};

} // namespace chat