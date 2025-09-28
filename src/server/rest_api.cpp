#include "rest_api.hpp"
#include "server.hpp"
#include "crypto.hpp"
#include <boost/json.hpp>
#include <iostream>
#include <chrono>

namespace chat {

namespace json = boost::json;

RestApiServer::RestApiServer(net::io_context& ioc, ChatServer& chat_server)
    : ioc_(ioc), acceptor_(ioc), chat_server_(chat_server), running_(false) {
}

RestApiServer::~RestApiServer() {
    stop();
}

bool RestApiServer::start(const std::string& address, unsigned short port) {
    try {
        auto const addr = net::ip::make_address(address);
        tcp::endpoint endpoint{addr, port};

        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);

        running_ = true;
        do_accept();

        std::cout << "[INFO] REST API server listening on " << address << ":" << port << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to start REST API server: " << e.what() << std::endl;
        return false;
    }
}

void RestApiServer::stop() {
    if (!running_) return;

    running_ = false;
    boost::system::error_code ec;
    acceptor_.close(ec);
}

void RestApiServer::do_accept() {
    if (!running_) return;

    acceptor_.async_accept(
        net::make_strand(ioc_),
        [this](beast::error_code ec, tcp::socket socket) {
            on_accept(ec, std::move(socket));
        });
}

void RestApiServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (!ec) {
        std::make_shared<HttpSession>(std::move(socket), *this)->run();
    }

    if (running_) {
        do_accept();
    }
}

// HttpSession implementation
RestApiServer::HttpSession::HttpSession(tcp::socket&& socket, RestApiServer& server)
    : socket_(std::move(socket)), server_(server) {
}

void RestApiServer::HttpSession::run() {
    do_read();
}

void RestApiServer::HttpSession::do_read() {
    req_ = {};

    http::async_read(socket_, buffer_, req_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            self->on_read(ec, bytes_transferred);
        });
}

void RestApiServer::HttpSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec == http::error::end_of_stream) {
        return;
    }

    if (ec) {
        return;
    }

    handle_request();
}

void RestApiServer::HttpSession::handle_request() {
    auto const target = req_.target();

    // CORS headers for all responses
    auto add_cors = [](auto& res) {
        res.set(http::field::access_control_allow_origin, "*");
        res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
        res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
    };

    // Handle OPTIONS requests (CORS preflight)
    if (req_.method() == http::verb::options) {
        auto res = std::make_shared<http::response<http::string_body>>(
            http::status::ok, req_.version());
        res->set(http::field::server, "CLI-Chat REST API");
        res->keep_alive(req_.keep_alive());
        add_cors(*res);
        res->body() = "";
        res->prepare_payload();
        res_ = res;
        do_write();
        return;
    }

    std::shared_ptr<http::response<http::string_body>> res;

    if (target == "/api/login" && req_.method() == http::verb::post) {
        res = std::make_shared<http::response<http::string_body>>(
            server_.handle_login(req_));
    }
    else if (target == "/api/register" && req_.method() == http::verb::post) {
        res = std::make_shared<http::response<http::string_body>>(
            server_.handle_register(req_));
    }
    else if (target == "/api/messages" && req_.method() == http::verb::get) {
        res = std::make_shared<http::response<http::string_body>>(
            server_.handle_messages(req_));
    }
    else if (target == "/api/users" && req_.method() == http::verb::get) {
        res = std::make_shared<http::response<http::string_body>>(
            server_.handle_users(req_));
    }
    else if (target == "/api/stats" && req_.method() == http::verb::get) {
        res = std::make_shared<http::response<http::string_body>>(
            server_.handle_stats(req_));
    }
    else {
        res = std::make_shared<http::response<http::string_body>>(
            server_.not_found());
    }

    add_cors(*res);
    res->version(req_.version());
    res->keep_alive(req_.keep_alive());
    res_ = res;
    do_write();
}

void RestApiServer::HttpSession::do_write() {
    auto res = std::static_pointer_cast<http::response<http::string_body>>(res_);

    http::async_write(socket_, *res,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            self->on_write(ec, bytes_transferred, res->need_eof());
        });
}

void RestApiServer::HttpSession::on_write(beast::error_code ec, std::size_t bytes_transferred, bool close) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        return;
    }

    if (close) {
        return;
    }

    do_read();
}

// API handlers
http::response<http::string_body> RestApiServer::handle_login(const http::request<http::string_body>& req) {
    try {
        auto body = json::parse(req.body());
        auto username = body.at("username").as_string();
        auto password = body.at("password").as_string();

        auto user = chat_server_.getDatabase().getUserByUsername(username.c_str());
        if (!user || !CryptoUtils::verifyPassword(password.c_str(), user->password_hash)) {
            return unauthorized();
        }

        // Create session
        auto token = CryptoUtils::generateSessionToken();
        auto now = std::chrono::system_clock::now();
        auto expires = now + std::chrono::hours(24);

        Session session;
        session.token = token;
        session.user_id = user->id;
        session.username = user->username;
        session.created_at = now;
        session.expires_at = expires;
        session.is_active = true;

        if (!chat_server_.getDatabase().createSession(session)) {
            return server_error("Failed to create session");
        }

        json::object response = {
            {"success", true},
            {"token", token},
            {"username", user->username},
            {"user_id", user->id}
        };

        return create_response(http::status::ok, json::serialize(response));
    } catch (const std::exception& e) {
        return bad_request("Invalid JSON or missing fields");
    }
}

http::response<http::string_body> RestApiServer::handle_register(const http::request<http::string_body>& req) {
    try {
        auto body = json::parse(req.body());
        auto username = body.at("username").as_string();
        auto password = body.at("password").as_string();
        std::string email = "";

        if (body.if_contains("email")) {
            email = body.at("email").as_string().c_str();
        }

        // Check if user exists
        auto existing_user = chat_server_.getDatabase().getUserByUsername(username.c_str());
        if (existing_user) {
            json::object response = {
                {"success", false},
                {"error", "Username already taken"}
            };
            return create_response(http::status::conflict, json::serialize(response));
        }

        // Create user
        auto password_hash = CryptoUtils::hashPassword(password.c_str());
        if (!chat_server_.getDatabase().createUser(username.c_str(), password_hash, email)) {
            return server_error("Failed to create user");
        }

        json::object response = {
            {"success", true},
            {"message", "User created successfully"}
        };

        return create_response(http::status::created, json::serialize(response));
    } catch (const std::exception& e) {
        return bad_request("Invalid JSON or missing fields");
    }
}

http::response<http::string_body> RestApiServer::handle_messages(const http::request<http::string_body>& req) {
    std::string token = extract_token(req);
    std::string username;

    if (!validate_session(token, username)) {
        return unauthorized();
    }

    try {
        auto messages = chat_server_.getDatabase().getRecentMessages(50);
        json::array message_array;

        for (const auto& msg : messages) {
            json::object msg_obj = {
                {"id", msg.id},
                {"username", msg.username},
                {"content", msg.content},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    msg.timestamp.time_since_epoch()).count()},
                {"is_private", msg.is_private}
            };

            if (!msg.recipient.empty()) {
                msg_obj["recipient"] = msg.recipient;
            }

            message_array.push_back(msg_obj);
        }

        json::object response = {
            {"success", true},
            {"messages", message_array}
        };

        return create_response(http::status::ok, json::serialize(response));
    } catch (const std::exception& e) {
        return server_error("Failed to retrieve messages");
    }
}

http::response<http::string_body> RestApiServer::handle_users(const http::request<http::string_body>& req) {
    std::string token = extract_token(req);
    std::string username;

    if (!validate_session(token, username)) {
        return unauthorized();
    }

    auto online_users = chat_server_.getOnlineUsers();
    json::array user_array;

    for (const auto& user : online_users) {
        user_array.push_back(user);
    }

    json::object response = {
        {"success", true},
        {"users", user_array}
    };

    return create_response(http::status::ok, json::serialize(response));
}

http::response<http::string_body> RestApiServer::handle_stats(const http::request<http::string_body>& req) {
    std::string token = extract_token(req);
    std::string username;

    if (!validate_session(token, username)) {
        return unauthorized();
    }

    try {
        int user_count = chat_server_.getDatabase().getUserCount();
        int message_count = chat_server_.getDatabase().getMessageCount();
        int online_count = chat_server_.getOnlineUsers().size();

        json::object response = {
            {"success", true},
            {"stats", {
                {"total_users", user_count},
                {"total_messages", message_count},
                {"online_users", online_count}
            }}
        };

        return create_response(http::status::ok, json::serialize(response));
    } catch (const std::exception& e) {
        return server_error("Failed to retrieve statistics");
    }
}

// Utility functions
http::response<http::string_body> RestApiServer::bad_request(const std::string& why) {
    json::object response = {
        {"success", false},
        {"error", why}
    };
    return create_response(http::status::bad_request, json::serialize(response));
}

http::response<http::string_body> RestApiServer::not_found() {
    json::object response = {
        {"success", false},
        {"error", "Not found"}
    };
    return create_response(http::status::not_found, json::serialize(response));
}

http::response<http::string_body> RestApiServer::server_error(const std::string& what) {
    json::object response = {
        {"success", false},
        {"error", what}
    };
    return create_response(http::status::internal_server_error, json::serialize(response));
}

http::response<http::string_body> RestApiServer::unauthorized() {
    json::object response = {
        {"success", false},
        {"error", "Unauthorized"}
    };
    return create_response(http::status::unauthorized, json::serialize(response));
}

http::response<http::string_body> RestApiServer::create_response(http::status status, const std::string& body) {
    http::response<http::string_body> res{status, 11};
    res.set(http::field::server, "CLI-Chat REST API");
    res.set(http::field::content_type, "application/json");
    res.body() = body;
    res.prepare_payload();
    return res;
}

std::string RestApiServer::extract_token(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it != req.end()) {
        std::string auth = it->value();
        if (auth.starts_with("Bearer ")) {
            return auth.substr(7);
        }
    }
    return "";
}

bool RestApiServer::validate_session(const std::string& token, std::string& username) {
    if (token.empty()) return false;

    auto session = chat_server_.getDatabase().getSession(token);
    if (!session || !session->is_active) {
        return false;
    }

    // Check if session is expired
    auto now = std::chrono::system_clock::now();
    if (now > session->expires_at) {
        chat_server_.getDatabase().deleteSession(token);
        return false;
    }

    username = session->username;
    return true;
}

} // namespace chat