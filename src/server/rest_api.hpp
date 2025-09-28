#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include "database.hpp"

namespace chat {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class ChatServer;

class RestApiServer {
public:
    RestApiServer(net::io_context& ioc, ChatServer& chat_server);
    ~RestApiServer();

    bool start(const std::string& address, unsigned short port);
    void stop();

private:
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    public:
        HttpSession(tcp::socket&& socket, RestApiServer& server);
        void run();

    private:
        void do_read();
        void on_read(beast::error_code ec, std::size_t bytes_transferred);
        void handle_request();
        void do_write();
        void on_write(beast::error_code ec, std::size_t bytes_transferred, bool close);

        tcp::socket socket_;
        RestApiServer& server_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> req_;
        std::shared_ptr<void> res_;
    };

    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    // API handlers
    http::response<http::string_body> handle_login(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_register(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_messages(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_users(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_stats(const http::request<http::string_body>& req);

    // Utility functions
    http::response<http::string_body> bad_request(const std::string& why);
    http::response<http::string_body> not_found();
    http::response<http::string_body> server_error(const std::string& what);
    http::response<http::string_body> unauthorized();
    http::response<http::string_body> create_response(http::status status, const std::string& body);

    std::string extract_token(const http::request<http::string_body>& req);
    bool validate_session(const std::string& token, std::string& username);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    ChatServer& chat_server_;
    std::atomic<bool> running_;
};

} // namespace chat