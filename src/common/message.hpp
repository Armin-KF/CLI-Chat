#pragma once

#include <string>
#include <chrono>
#include <vector>

namespace chat {

enum class MessageType {
    BROADCAST,
    PRIVATE,
    SYSTEM,
    AUTH,
    USER_LIST,
    ERROR
};

struct ChatMessage {
    MessageType type;
    std::string sender;
    std::string recipient; // Empty for broadcast messages
    std::string content;
    std::chrono::system_clock::time_point timestamp;

    ChatMessage() : type(MessageType::BROADCAST), timestamp(std::chrono::system_clock::now()) {}
};

class MessageParser {
public:
    static ChatMessage parse(const std::string& raw_message);
    static std::string serialize(const ChatMessage& message);

    // Protocol message builders
    static std::string buildLoginMessage(const std::string& username, const std::string& password);
    static std::string buildRegisterMessage(const std::string& username, const std::string& password, const std::string& email = "");
    static std::string buildBroadcastMessage(const std::string& content);
    static std::string buildPrivateMessage(const std::string& recipient, const std::string& content);
    static std::string buildUserListRequest();
    static std::string buildLogoutMessage();

    // Response parsers
    static bool isAuthResponse(const std::string& message);
    static bool isUserListResponse(const std::string& message);
    static bool isErrorResponse(const std::string& message);

    static std::vector<std::string> parseUserList(const std::string& message);

private:
    static std::string escapeString(const std::string& str);
    static std::string unescapeString(const std::string& str);
};

} // namespace chat