#include "client.hpp"
#include "crypto.hpp"
#include <boost/bind.hpp>
#include <iostream>
#include <sstream>

namespace chat {

ChatClient::ChatClient(boost::asio::io_context& io_context)
    : io_context_(io_context)
    , connected_(false)
    , authenticated_(false)
    , config_(Config::getInstance()) {

    // Initialize SSL context for client
    ssl_context_ = TLSContext::createClientContext();
}

ChatClient::~ChatClient() {
    disconnect();
}

bool ChatClient::connect(const std::string& host, int port) {
    try {
        socket_ = std::make_unique<ssl_socket>(io_context_, *ssl_context_);

        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(host, std::to_string(port));

        boost::asio::async_connect(socket_->lowest_layer(), endpoints,
            boost::bind(&ChatClient::handleConnect, this,
                       boost::asio::placeholders::error));

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to initiate connection: " << e.what() << std::endl;
        return false;
    }
}

void ChatClient::handleConnect(const boost::system::error_code& error) {
    if (!error) {
        std::cout << "[INFO] Connected to server, starting TLS handshake..." << std::endl;

        socket_->async_handshake(boost::asio::ssl::stream_base::client,
            boost::bind(&ChatClient::handleHandshake, this,
                       boost::asio::placeholders::error));
    } else {
        std::cerr << "[ERROR] Connection failed: " << error.message() << std::endl;
        connected_ = false;
    }
}

void ChatClient::handleHandshake(const boost::system::error_code& error) {
    if (!error) {
        std::cout << "[INFO] TLS handshake completed successfully" << std::endl;
        connected_ = true;
        startRead();
    } else {
        std::cerr << "[ERROR] TLS handshake failed: " << error.message() << std::endl;
        connected_ = false;
    }
}

void ChatClient::disconnect() {
    if (!connected_) return;

    connected_ = false;
    authenticated_ = false;

    if (socket_) {
        boost::system::error_code ec;
        socket_->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_->lowest_layer().close(ec);
    }

    std::cout << "[INFO] Disconnected from server" << std::endl;
}

bool ChatClient::login(const std::string& username, const std::string& password) {
    if (!connected_) {
        std::cerr << "[ERROR] Not connected to server" << std::endl;
        return false;
    }

    username_ = username;
    sendRawMessage("login:" + username + ":" + password + "\n");

    // Note: Authentication result will be handled in processMessage
    return true;
}

bool ChatClient::registerUser(const std::string& username, const std::string& password, const std::string& email) {
    if (!connected_) {
        std::cerr << "[ERROR] Not connected to server" << std::endl;
        return false;
    }

    std::string register_msg = "register:" + username + ":" + password;
    if (!email.empty()) {
        register_msg += ":" + email;
    }
    register_msg += "\n";

    sendRawMessage(register_msg);
    return true;
}

void ChatClient::logout() {
    if (authenticated_) {
        sendRawMessage("logout\n");
        authenticated_ = false;
        username_.clear();
    }
}

void ChatClient::sendMessage(const std::string& message) {
    if (!authenticated_) {
        std::cerr << "[ERROR] Not authenticated" << std::endl;
        return;
    }

    sendRawMessage("broadcast:" + message + "\n");
}

void ChatClient::sendPrivateMessage(const std::string& recipient, const std::string& message) {
    if (!authenticated_) {
        std::cerr << "[ERROR] Not authenticated" << std::endl;
        return;
    }

    sendRawMessage("@" + recipient + ":" + message + "\n");
}

void ChatClient::requestUserList() {
    if (!authenticated_) {
        std::cerr << "[ERROR] Not authenticated" << std::endl;
        return;
    }

    sendRawMessage("users\n");
}

void ChatClient::sendRawMessage(const std::string& message) {
    if (!connected_) return;

    std::lock_guard<std::mutex> lock(write_mutex_);
    bool write_in_progress = !write_queue_.empty();
    write_queue_.push(message);

    if (!write_in_progress && socket_) {
        boost::asio::async_write(*socket_,
            boost::asio::buffer(write_queue_.front()),
            boost::bind(&ChatClient::handleWrite, this,
                       boost::asio::placeholders::error));
    }
}

void ChatClient::startRead() {
    if (!connected_ || !socket_) return;

    boost::asio::async_read_until(*socket_, read_buffer_, "\n",
        boost::bind(&ChatClient::handleRead, this,
                   boost::asio::placeholders::error,
                   boost::asio::placeholders::bytes_transferred));
}

void ChatClient::handleRead(const boost::system::error_code& error, size_t bytes_transferred) {
    if (!error) {
        std::istream is(&read_buffer_);
        std::string message;
        std::getline(is, message);

        // Remove carriage return if present
        if (!message.empty() && message.back() == '\r') {
            message.pop_back();
        }

        processMessage(message);
        startRead();
    } else {
        std::cout << "[INFO] Connection lost: " << error.message() << std::endl;
        connected_ = false;
        authenticated_ = false;
    }
}

void ChatClient::handleWrite(const boost::system::error_code& error) {
    if (!error) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_queue_.pop();

        if (!write_queue_.empty() && socket_) {
            boost::asio::async_write(*socket_,
                boost::asio::buffer(write_queue_.front()),
                boost::bind(&ChatClient::handleWrite, this,
                           boost::asio::placeholders::error));
        }
    } else {
        std::cerr << "[ERROR] Write failed: " << error.message() << std::endl;
        connected_ = false;
        authenticated_ = false;
    }
}

void ChatClient::processMessage(const std::string& message) {
    if (message.rfind("auth:", 0) == 0) {
        // Authentication response
        if (message.rfind("auth:success", 0) == 0) {
            authenticated_ = true;
            std::cout << "[INFO] Authentication successful" << std::endl;
            if (system_message_callback_) {
                system_message_callback_("Authentication successful");
            }
        } else {
            authenticated_ = false;
            std::string error_msg = "Authentication failed";
            size_t pos = message.find(':', 11);
            if (pos != std::string::npos) {
                error_msg = message.substr(pos + 1);
            }
            std::cout << "[ERROR] " << error_msg << std::endl;
            if (system_message_callback_) {
                system_message_callback_(error_msg);
            }
        }
    }
    else if (message.rfind("users:", 0) == 0) {
        // User list update
        std::vector<std::string> users;
        std::string user_list = message.substr(6);

        if (!user_list.empty()) {
            std::stringstream ss(user_list);
            std::string user;
            while (std::getline(ss, user, ',')) {
                if (!user.empty()) {
                    users.push_back(user);
                }
            }
        }

        if (user_list_callback_) {
            user_list_callback_(users);
        }
    }
    else if (message.rfind("private:", 0) == 0) {
        // Private message: private:sender:message
        size_t first_colon = message.find(':', 8);
        if (first_colon != std::string::npos) {
            std::string sender = message.substr(8, first_colon - 8);
            std::string content = message.substr(first_colon + 1);

            if (private_message_callback_) {
                private_message_callback_(sender, content);
            }
        }
    }
    else if (message.rfind("system:", 0) == 0) {
        // System message
        std::string content = message.substr(7);
        if (system_message_callback_) {
            system_message_callback_(content);
        }
    }
    else if (message.rfind("error:", 0) == 0) {
        // Error message
        std::string error_msg = message.substr(6);
        std::cerr << "[ERROR] " << error_msg << std::endl;
        if (system_message_callback_) {
            system_message_callback_("Error: " + error_msg);
        }
    }
    else if (message.rfind("private_sent:", 0) == 0) {
        // Private message sent confirmation
        std::string recipient = message.substr(13);
        if (system_message_callback_) {
            system_message_callback_("Private message sent to " + recipient);
        }
    }
    else if (!message.empty()) {
        // Regular broadcast message
        if (message_callback_) {
            message_callback_(message);
        }
    }
}

} // namespace chat