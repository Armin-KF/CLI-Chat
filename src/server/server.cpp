#include "server.hpp"
#include "session.hpp"
#include "crypto.hpp"
#include <boost/bind.hpp>
#include <iostream>
#include <filesystem>

namespace chat {

ChatServer::ChatServer(boost::asio::io_context& io_context)
    : io_context_(io_context)
    , running_(false)
    , signals_(io_context, SIGINT, SIGTERM)
    , config_(Config::getInstance()) {
}

ChatServer::~ChatServer() {
    stop();
}

bool ChatServer::initialize() {
    try {
        // Initialize database
        database_ = std::make_unique<Database>(config_.getDatabasePath());
        if (!database_->initialize()) {
            std::cerr << "[ERROR] Failed to initialize database" << std::endl;
            return false;
        }

        // Create certificates directory if it doesn't exist
        std::filesystem::create_directories("certs");

        // Initialize SSL context
        if (config_.getTLSEnabled()) {
            // Check if certificates exist, generate if not
            if (!std::filesystem::exists(config_.getCertFile()) ||
                !std::filesystem::exists(config_.getKeyFile())) {
                std::cout << "[INFO] Generating self-signed certificates..." << std::endl;
                if (!TLSContext::generateSelfSignedCert(config_.getCertFile(),
                                                       config_.getKeyFile())) {
                    std::cerr << "[ERROR] Failed to generate SSL certificates" << std::endl;
                    return false;
                }
            }

            ssl_context_ = TLSContext::createServerContext(config_.getCertFile(),
                                                          config_.getKeyFile());
            if (!ssl_context_) {
                std::cerr << "[ERROR] Failed to create SSL context" << std::endl;
                return false;
            }
        }

        // Initialize acceptor
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::make_address(config_.getServerHost()),
            config_.getServerPort()
        );

        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_, endpoint);
        acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

        setupSignalHandlers();

        std::cout << "[INFO] Server initialized on " << config_.getServerHost()
                  << ":" << config_.getServerPort()
                  << " (TLS: " << (config_.getTLSEnabled() ? "enabled" : "disabled") << ")" << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Server initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void ChatServer::start() {
    running_ = true;
    std::cout << "[INFO] Starting CLI-Chat server..." << std::endl;
    startAccept();
}

void ChatServer::stop() {
    if (!running_) return;

    running_ = false;
    std::cout << "[INFO] Stopping server..." << std::endl;

    // Close acceptor
    if (acceptor_) {
        boost::system::error_code ec;
        acceptor_->close(ec);
    }

    // Close all sessions
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& session : all_sessions_) {
        session->stop();
    }
    all_sessions_.clear();
    sessions_by_username_.clear();

    std::cout << "[INFO] Server stopped" << std::endl;
}

void ChatServer::startAccept() {
    if (!running_) return;

    auto new_session = std::make_shared<Session>(io_context_, ssl_context_, *this);

    acceptor_->async_accept(new_session->socket().lowest_layer(),
        boost::bind(&ChatServer::handleAccept, this, new_session,
                   boost::asio::placeholders::error));
}

void ChatServer::handleAccept(std::shared_ptr<Session> session,
                             const boost::system::error_code& error) {
    if (!error && running_) {
        std::cout << "[INFO] New client connection" << std::endl;
        session->start();
        startAccept();
    } else if (running_) {
        std::cerr << "[ERROR] Accept failed: " << error.message() << std::endl;
        startAccept();
    }
}

void ChatServer::addSession(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    all_sessions_.insert(session);
    if (!session->getUsername().empty()) {
        sessions_by_username_[session->getUsername()] = session;

        // Notify other users
        std::string notification = "system:User " + session->getUsername() + " joined the chat\n";
        for (auto& [username, sess] : sessions_by_username_) {
            if (username != session->getUsername()) {
                sess->sendMessage(notification);
            }
        }

        // Send updated user list
        auto users = getOnlineUsers();
        std::string user_list = "users:";
        for (const auto& user : users) {
            user_list += user + ",";
        }
        if (user_list.back() == ',') {
            user_list.pop_back();
        }
        user_list += "\n";

        for (auto& [username, sess] : sessions_by_username_) {
            sess->sendMessage(user_list);
        }
    }

    std::cout << "[INFO] Session added for user: " << session->getUsername() << std::endl;
}

void ChatServer::removeSession(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    std::string username = session->getUsername();

    all_sessions_.erase(session);
    if (!username.empty()) {
        sessions_by_username_.erase(username);

        // Notify other users
        std::string notification = "system:User " + username + " left the chat\n";
        for (auto& [user, sess] : sessions_by_username_) {
            sess->sendMessage(notification);
        }

        // Send updated user list
        auto users = getOnlineUsers();
        std::string user_list = "users:";
        for (const auto& user : users) {
            user_list += user + ",";
        }
        if (user_list.back() == ',') {
            user_list.pop_back();
        }
        user_list += "\n";

        for (auto& [user, sess] : sessions_by_username_) {
            sess->sendMessage(user_list);
        }
    }

    session->stop();
    std::cout << "[INFO] Session removed for user: " << username << std::endl;
}

void ChatServer::broadcastMessage(const std::string& message, std::shared_ptr<Session> exclude) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    for (auto& [username, session] : sessions_by_username_) {
        if (session != exclude) {
            session->sendMessage(message);
        }
    }
}

void ChatServer::sendPrivateMessage(const std::string& recipient,
                                   const std::string& message,
                                   std::shared_ptr<Session> sender) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = sessions_by_username_.find(recipient);
    if (it != sessions_by_username_.end()) {
        std::string private_msg = "private:" + sender->getUsername() + ":" + message + "\n";
        it->second->sendMessage(private_msg);

        // Send confirmation to sender
        sender->sendMessage("private_sent:" + recipient + "\n");
    } else {
        // User not online
        sender->sendMessage("error:User " + recipient + " is not online\n");
    }
}

std::vector<std::string> ChatServer::getOnlineUsers() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    std::vector<std::string> users;
    for (const auto& [username, session] : sessions_by_username_) {
        users.push_back(username);
    }

    return users;
}

bool ChatServer::isUserOnline(const std::string& username) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_by_username_.find(username) != sessions_by_username_.end();
}

void ChatServer::setupSignalHandlers() {
    signals_.async_wait(
        boost::bind(&ChatServer::handleStop, this,
                   boost::asio::placeholders::error,
                   boost::asio::placeholders::signal_number));
}

void ChatServer::handleStop(const boost::system::error_code& error, int signal_number) {
    if (!error) {
        std::cout << "[INFO] Received signal " << signal_number << ", shutting down..." << std::endl;
        stop();
    }
}

} // namespace chat