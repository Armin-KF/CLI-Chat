#include "message.hpp"
#include <sstream>
#include <algorithm>

namespace chat {

ChatMessage MessageParser::parse(const std::string& raw_message) {
    ChatMessage message;

    if (raw_message.rfind("login:", 0) == 0) {
        message.type = MessageType::AUTH;
        message.content = raw_message;
    }
    else if (raw_message.rfind("register:", 0) == 0) {
        message.type = MessageType::AUTH;
        message.content = raw_message;
    }
    else if (raw_message.rfind("auth:", 0) == 0) {
        message.type = MessageType::AUTH;
        message.content = raw_message;
    }
    else if (raw_message.rfind("users:", 0) == 0) {
        message.type = MessageType::USER_LIST;
        message.content = raw_message;
    }
    else if (raw_message.rfind("error:", 0) == 0) {
        message.type = MessageType::ERROR;
        message.content = raw_message.substr(6);
    }
    else if (raw_message.rfind("system:", 0) == 0) {
        message.type = MessageType::SYSTEM;
        message.content = raw_message.substr(7);
    }
    else if (raw_message.rfind("private:", 0) == 0) {
        message.type = MessageType::PRIVATE;
        // Parse: private:sender:content
        size_t first_colon = raw_message.find(':', 8);
        if (first_colon != std::string::npos) {
            message.sender = raw_message.substr(8, first_colon - 8);
            message.content = raw_message.substr(first_colon + 1);
        }
    }
    else if (raw_message.rfind("@", 0) == 0) {
        message.type = MessageType::PRIVATE;
        // Parse: @recipient:content
        size_t colon = raw_message.find(':');
        if (colon != std::string::npos) {
            message.recipient = raw_message.substr(1, colon - 1);
            message.content = raw_message.substr(colon + 1);
        }
    }
    else {
        message.type = MessageType::BROADCAST;
        // Parse: sender: content
        size_t colon = raw_message.find(':');
        if (colon != std::string::npos) {
            message.sender = raw_message.substr(0, colon);
            message.content = raw_message.substr(colon + 2); // Skip ": "
        } else {
            message.content = raw_message;
        }
    }

    return message;
}

std::string MessageParser::serialize(const ChatMessage& message) {
    std::ostringstream oss;

    switch (message.type) {
        case MessageType::BROADCAST:
            if (!message.sender.empty()) {
                oss << message.sender << ": " << message.content;
            } else {
                oss << message.content;
            }
            break;

        case MessageType::PRIVATE:
            if (!message.sender.empty()) {
                oss << "private:" << message.sender << ":" << message.content;
            } else {
                oss << "@" << message.recipient << ":" << message.content;
            }
            break;

        case MessageType::SYSTEM:
            oss << "system:" << message.content;
            break;

        case MessageType::ERROR:
            oss << "error:" << message.content;
            break;

        case MessageType::AUTH:
            oss << message.content;
            break;

        case MessageType::USER_LIST:
            oss << message.content;
            break;
    }

    return oss.str();
}

std::string MessageParser::buildLoginMessage(const std::string& username, const std::string& password) {
    return "login:" + username + ":" + password;
}

std::string MessageParser::buildRegisterMessage(const std::string& username, const std::string& password, const std::string& email) {
    std::string message = "register:" + username + ":" + password;
    if (!email.empty()) {
        message += ":" + email;
    }
    return message;
}

std::string MessageParser::buildBroadcastMessage(const std::string& content) {
    return "broadcast:" + content;
}

std::string MessageParser::buildPrivateMessage(const std::string& recipient, const std::string& content) {
    return "@" + recipient + ":" + content;
}

std::string MessageParser::buildUserListRequest() {
    return "users";
}

std::string MessageParser::buildLogoutMessage() {
    return "logout";
}

bool MessageParser::isAuthResponse(const std::string& message) {
    return message.rfind("auth:", 0) == 0;
}

bool MessageParser::isUserListResponse(const std::string& message) {
    return message.rfind("users:", 0) == 0;
}

bool MessageParser::isErrorResponse(const std::string& message) {
    return message.rfind("error:", 0) == 0;
}

std::vector<std::string> MessageParser::parseUserList(const std::string& message) {
    std::vector<std::string> users;

    if (!isUserListResponse(message)) {
        return users;
    }

    std::string user_list = message.substr(6); // Remove "users:"
    if (user_list.empty()) {
        return users;
    }

    std::istringstream ss(user_list);
    std::string user;

    while (std::getline(ss, user, ',')) {
        if (!user.empty()) {
            users.push_back(user);
        }
    }

    return users;
}

std::string MessageParser::escapeString(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        if (c == ':' || c == '\\') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

std::string MessageParser::unescapeString(const std::string& str) {
    std::string unescaped;
    bool escaped = false;

    for (char c : str) {
        if (escaped) {
            unescaped += c;
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else {
            unescaped += c;
        }
    }

    return unescaped;
}

} // namespace chat