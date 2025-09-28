#pragma once

#include "client.hpp"
#include <string>
#include <vector>
#include <atomic>

namespace chat {

class TerminalUI {
public:
    TerminalUI(ChatClient& client);
    ~TerminalUI();

    void run();

private:
    void displayLogo();
    void displayMenu();
    bool handleLogin();
    bool handleRegister();
    void chatMode();
    void showOnlineUsers(const std::vector<std::string>& users);

    // Message callbacks
    void onMessage(const std::string& message);
    void onPrivateMessage(const std::string& sender, const std::string& message);
    void onUserList(const std::vector<std::string>& users);
    void onSystemMessage(const std::string& message);

    // Utility functions
    std::string getInput(const std::string& prompt, bool hidden = false);
    void clearScreen();
    void printColored(const std::string& text, const std::string& color);

    ChatClient& client_;
    std::atomic<bool> running_;
    std::vector<std::string> online_users_;

    // Colors
    static const std::string RESET;
    static const std::string RED;
    static const std::string GREEN;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string PURPLE;
    static const std::string CYAN;
};

} // namespace chat