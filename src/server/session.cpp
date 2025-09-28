#include "session.hpp"
#include "server.hpp"
#include "crypto.hpp"
#include <boost/bind.hpp>
#include <iostream>
#include <sstream>
#include <chrono>

namespace chat {

Session::Session(boost::asio::io_context& io_context,
                 std::shared_ptr<boost::asio::ssl::context> ssl_context,
                 ChatServer& server)
    : socket_(io_context, *ssl_context)
    , server_(server)
    , user_id_(0)
    , authenticated_(false) {
}

void Session::start() {
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
        boost::bind(&Session::handleHandshake, shared_from_this(),
                   boost::asio::placeholders::error));
}

void Session::handleHandshake(const boost::system::error_code& error) {
    if (!error) {
        std::cout << "[INFO] SSL handshake completed for new client" << std::endl;
        startRead();
    } else {
        std::cerr << "[ERROR] SSL handshake failed: " << error.message() << std::endl;
    }
}

void Session::startRead() {
    boost::asio::async_read_until(socket_, read_buffer_, "\n",
        boost::bind(&Session::handleRead, shared_from_this(),
                   boost::asio::placeholders::error,
                   boost::asio::placeholders::bytes_transferred));
}

void Session::handleRead(const boost::system::error_code& error, size_t bytes_transferred) {
    if (!error) {
        std::istream is(&read_buffer_);
        std::string message;
        std::getline(is, message);

        if (message.length() > MAX_MESSAGE_LENGTH) {
            sendMessage("error:Message too long\n");
            startRead();
            return;
        }

        // Remove carriage return if present
        if (!message.empty() && message.back() == '\r') {
            message.pop_back();
        }

        processMessage(message);
        startRead();
    } else {
        std::cout << "[INFO] Client disconnected: " << username_ << std::endl;
        server_.removeSession(shared_from_this());
    }
}

void Session::processMessage(const std::string& message) {
    std::cout << "[DEBUG] Received message: " << message << std::endl;

    if (!authenticated_) {
        // Handle authentication commands
        if (message.rfind("login:", 0) == 0) {
            size_t first_delim = message.find(':', 6);
            if (first_delim != std::string::npos) {
                std::string username = message.substr(6, first_delim - 6);
                std::string password = message.substr(first_delim + 1);
                handleLogin(username, password);
            } else {
                sendAuthResponse(false, "Invalid login format");
            }
        }
        else if (message.rfind("register:", 0) == 0) {
            size_t first_delim = message.find(':', 9);
            size_t second_delim = message.find(':', first_delim + 1);

            if (first_delim != std::string::npos && second_delim != std::string::npos) {
                std::string username = message.substr(9, first_delim - 9);
                std::string password = message.substr(first_delim + 1, second_delim - first_delim - 1);
                std::string email = message.substr(second_delim + 1);
                handleRegister(username, password, email);
            } else if (first_delim != std::string::npos) {
                std::string username = message.substr(9, first_delim - 9);
                std::string password = message.substr(first_delim + 1);
                handleRegister(username, password, "");
            } else {
                sendAuthResponse(false, "Invalid registration format");
            }
        }
        else {
            sendMessage("error:Authentication required\n");
        }
    } else {
        // Handle authenticated user commands
        if (message.rfind("@", 0) == 0) {
            // Private message: @username:message
            size_t pos = message.find(':');
            if (pos != std::string::npos && pos > 1) {
                std::string recipient = message.substr(1, pos - 1);
                std::string content = message.substr(pos + 1);
                handlePrivateMessage(recipient, content);
            }
        }
        else if (message.rfind("broadcast:", 0) == 0) {
            std::string content = message.substr(10);
            handleBroadcastMessage(content);
        }
        else if (message == "users") {
            auto users = server_.getOnlineUsers();
            std::string user_list = "users:";
            for (const auto& user : users) {
                if (user != username_) {
                    user_list += user + ",";
                }
            }
            if (user_list.back() == ',') {
                user_list.pop_back();
            }
            user_list += "\n";
            sendMessage(user_list);
        }
        else if (message == "logout") {
            authenticated_ = false;
            server_.removeSession(shared_from_this());
        }
        else {
            // Default to broadcast for backwards compatibility
            handleBroadcastMessage(message);
        }
    }
}

bool Session::handleLogin(const std::string& username, const std::string& password) {
    auto user = server_.getDatabase().getUserByUsername(username);
    if (!user) {
        sendAuthResponse(false, "User not found");
        return false;
    }

    if (!CryptoUtils::verifyPassword(password, user->password_hash)) {
        sendAuthResponse(false, "Invalid password");
        return false;
    }

    if (!user->is_active) {
        sendAuthResponse(false, "Account disabled");
        return false;
    }

    // Check if user is already online
    if (server_.isUserOnline(username)) {
        sendAuthResponse(false, "User already online");
        return false;
    }

    // Create session
    session_token_ = CryptoUtils::generateSessionToken();
    auto now = std::chrono::system_clock::now();
    auto expires = now + std::chrono::hours(24);

    Session db_session;
    db_session.token = session_token_;
    db_session.user_id = user->id;
    db_session.username = username;
    db_session.created_at = now;
    db_session.expires_at = expires;
    db_session.is_active = true;

    if (!server_.getDatabase().createSession(db_session)) {
        sendAuthResponse(false, "Failed to create session");
        return false;
    }

    // Update last login
    server_.getDatabase().updateLastLogin(user->id);

    // Set session data
    username_ = username;
    user_id_ = user->id;
    authenticated_ = true;

    server_.addSession(shared_from_this());
    sendAuthResponse(true, "Login successful");

    std::cout << "[INFO] User authenticated: " << username_ << std::endl;
    return true;
}

bool Session::handleRegister(const std::string& username, const std::string& password, const std::string& email) {
    // Validate username
    if (username.empty() || username.length() > 32) {
        sendAuthResponse(false, "Invalid username length");
        return false;
    }

    // Check if user already exists
    auto existing_user = server_.getDatabase().getUserByUsername(username);
    if (existing_user) {
        sendAuthResponse(false, "Username already taken");
        return false;
    }

    // Hash password
    std::string password_hash = CryptoUtils::hashPassword(password);

    // Create user
    if (!server_.getDatabase().createUser(username, password_hash, email)) {
        sendAuthResponse(false, "Failed to create user");
        return false;
    }

    sendAuthResponse(true, "Registration successful. Please login.");
    std::cout << "[INFO] New user registered: " << username << std::endl;
    return true;
}

void Session::sendAuthResponse(bool success, const std::string& message) {
    std::string response = success ? "auth:success" : "auth:failed";
    if (!message.empty()) {
        response += ":" + message;
    }
    response += "\n";
    sendMessage(response);
}

void Session::handlePrivateMessage(const std::string& recipient, const std::string& content) {
    // Save to database
    Message msg;
    msg.user_id = user_id_;
    msg.username = username_;
    msg.content = content;
    msg.recipient = recipient;
    msg.timestamp = std::chrono::system_clock::now();
    msg.is_private = true;

    server_.getDatabase().saveMessage(msg);

    // Send to recipient if online
    server_.sendPrivateMessage(recipient, content, shared_from_this());

    std::cout << "[INFO] Private message from " << username_ << " to " << recipient << std::endl;
}

void Session::handleBroadcastMessage(const std::string& content) {
    // Save to database
    Message msg;
    msg.user_id = user_id_;
    msg.username = username_;
    msg.content = content;
    msg.timestamp = std::chrono::system_clock::now();
    msg.is_private = false;

    server_.getDatabase().saveMessage(msg);

    // Broadcast to all users
    std::string broadcast_msg = username_ + ": " + content + "\n";
    server_.broadcastMessage(broadcast_msg, shared_from_this());

    std::cout << "[INFO] Broadcast message from " << username_ << std::endl;
}

void Session::sendMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    bool write_in_progress = !write_queue_.empty();
    write_queue_.push(message);

    if (!write_in_progress) {
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_queue_.front()),
            boost::bind(&Session::handleWrite, shared_from_this(),
                       boost::asio::placeholders::error));
    }
}

void Session::handleWrite(const boost::system::error_code& error) {
    if (!error) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_queue_.pop();

        if (!write_queue_.empty()) {
            boost::asio::async_write(socket_,
                boost::asio::buffer(write_queue_.front()),
                boost::bind(&Session::handleWrite, shared_from_this(),
                           boost::asio::placeholders::error));
        }
    } else {
        std::cerr << "[ERROR] Write failed: " << error.message() << std::endl;
        server_.removeSession(shared_from_this());
    }
}

void Session::stop() {
    boost::system::error_code ec;
    socket_.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.lowest_layer().close(ec);

    if (!session_token_.empty()) {
        server_.getDatabase().deleteSession(session_token_);
    }
}

} // namespace chat