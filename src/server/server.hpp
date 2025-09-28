#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <unordered_map>
#include <set>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include "database.hpp"
#include "config.hpp"

namespace chat {

class Session;

class ChatServer {
public:
    ChatServer(boost::asio::io_context& io_context);
    ~ChatServer();

    bool initialize();
    void start();
    void stop();

    // Session management
    void addSession(std::shared_ptr<Session> session);
    void removeSession(std::shared_ptr<Session> session);
    void broadcastMessage(const std::string& message, std::shared_ptr<Session> exclude = nullptr);
    void sendPrivateMessage(const std::string& recipient, const std::string& message, std::shared_ptr<Session> sender);

    // User management
    std::vector<std::string> getOnlineUsers();
    bool isUserOnline(const std::string& username);

    // Database access
    Database& getDatabase() { return *database_; }

private:
    void startAccept();
    void handleAccept(std::shared_ptr<Session> session, const boost::system::error_code& error);
    void setupSignalHandlers();
    void handleStop(const boost::system::error_code& error, int signal_number);

    boost::asio::io_context& io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::shared_ptr<boost::asio::ssl::context> ssl_context_;
    std::unique_ptr<Database> database_;

    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_by_username_;
    std::set<std::shared_ptr<Session>> all_sessions_;
    std::mutex sessions_mutex_;

    std::atomic<bool> running_;
    boost::asio::signal_set signals_;

    Config& config_;
};

} // namespace chat