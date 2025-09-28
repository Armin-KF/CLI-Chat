#include "terminal_ui.hpp"
#include <iostream>
#include <iomanip>
#include <limits>

namespace chat {

const std::string TerminalUI::RESET = "\033[0m";
const std::string TerminalUI::RED = "\033[31m";
const std::string TerminalUI::GREEN = "\033[32m";
const std::string TerminalUI::YELLOW = "\033[33m";
const std::string TerminalUI::BLUE = "\033[34m";
const std::string TerminalUI::PURPLE = "\033[35m";
const std::string TerminalUI::CYAN = "\033[36m";

TerminalUI::TerminalUI(ChatClient& client)
    : client_(client), running_(true) {

    // Set up callbacks
    client_.setMessageCallback([this](const std::string& msg) { onMessage(msg); });
    client_.setPrivateMessageCallback([this](const std::string& sender, const std::string& msg) {
        onPrivateMessage(sender, msg);
    });
    client_.setUserListCallback([this](const std::vector<std::string>& users) { onUserList(users); });
    client_.setSystemMessageCallback([this](const std::string& msg) { onSystemMessage(msg); });
}

TerminalUI::~TerminalUI() {
    running_ = false;
}

void TerminalUI::run() {
    displayLogo();

    // Connect to server
    std::string host = "127.0.0.1";
    int port = 4000;

    printColored("Connecting to server...", YELLOW);
    if (!client_.connect(host, port)) {
        printColored("Failed to connect to server", RED);
        return;
    }

    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!client_.isConnected()) {
        printColored("Connection failed", RED);
        return;
    }

    printColored("Connected successfully!", GREEN);

    // Main menu loop
    while (running_) {
        displayMenu();

        std::string choice = getInput("Enter your choice (1-3): ");

        if (choice == "1") {
            if (handleLogin()) {
                chatMode();
            }
        } else if (choice == "2") {
            handleRegister();
        } else if (choice == "3") {
            running_ = false;
        } else {
            printColored("Invalid choice. Please try again.", RED);
        }
    }

    client_.disconnect();
}

void TerminalUI::displayLogo() {
    clearScreen();

    std::vector<std::string> logo = {
        "  ____ _     ___     ____ _           _   ",
        " / ___| |   |_ _|   / ___| |__   __ _| |_ ",
        "| |   | |    | |   | |   | '_ \\ / _` | __|",
        "| |___| |___ | |   | |___| | | | (_| | |_ ",
        " \\____|_____|___|   \\____|_| |_|\\__,_|\\__|"
    };

    std::cout << "\n";
    for (const auto& line : logo) {
        std::cout << GREEN << std::setw(50) << line << RESET << std::endl;
    }

    printColored("========================================", CYAN);
    printColored("   Enterprise-Grade Secure Chat", CYAN);
    printColored("========================================", CYAN);
    std::cout << "\n";
}

void TerminalUI::displayMenu() {
    std::cout << "\n";
    printColored("=== Main Menu ===", BLUE);
    std::cout << "1. Login\n";
    std::cout << "2. Register\n";
    std::cout << "3. Exit\n";
    std::cout << "\n";
}

bool TerminalUI::handleLogin() {
    std::cout << "\n";
    printColored("=== Login ===", BLUE);

    std::string username = getInput("Username: ");
    std::string password = getInput("Password: ", true);

    if (client_.login(username, password)) {
        // Wait for authentication response
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (client_.isAuthenticated()) {
            printColored("Login successful! Welcome, " + username, GREEN);
            return true;
        }
    }

    printColored("Login failed. Please check your credentials.", RED);
    return false;
}

bool TerminalUI::handleRegister() {
    std::cout << "\n";
    printColored("=== Register ===", BLUE);

    std::string username = getInput("Username (alphanumeric, underscore, dash only): ");
    std::string password = getInput("Password (minimum 6 characters): ", true);
    std::string email = getInput("Email (optional): ");

    if (client_.registerUser(username, password, email)) {
        printColored("Registration successful! You can now login.", GREEN);
        return true;
    }

    printColored("Registration failed. Username may already exist.", RED);
    return false;
}

void TerminalUI::chatMode() {
    std::cout << "\n";
    printColored("=== Chat Mode ===", BLUE);
    printColored("Type 'help' for commands, 'quit' to exit chat mode", YELLOW);

    // Request initial user list
    client_.requestUserList();

    std::string input;
    while (running_ && client_.isAuthenticated()) {
        std::cout << CYAN << "Chat> " << RESET;
        std::getline(std::cin, input);

        if (input.empty()) continue;

        if (input == "quit" || input == "exit") {
            break;
        } else if (input == "help") {
            printColored("Commands:", GREEN);
            std::cout << "  help          - Show this help\n";
            std::cout << "  users         - Show online users\n";
            std::cout << "  @username msg - Send private message\n";
            std::cout << "  quit          - Exit chat mode\n";
            std::cout << "  anything else - Send public message\n";
        } else if (input == "users") {
            client_.requestUserList();
        } else if (input[0] == '@') {
            // Private message
            size_t space = input.find(' ');
            if (space != std::string::npos) {
                std::string recipient = input.substr(1, space - 1);
                std::string message = input.substr(space + 1);
                client_.sendPrivateMessage(recipient, message);
            } else {
                printColored("Usage: @username message", RED);
            }
        } else {
            // Public message
            client_.sendMessage(input);
        }
    }

    printColored("Exiting chat mode...", YELLOW);
}

void TerminalUI::showOnlineUsers(const std::vector<std::string>& users) {
    printColored("Online Users:", GREEN);
    if (users.empty()) {
        printColored("  No other users online", YELLOW);
    } else {
        for (const auto& user : users) {
            std::cout << "  " << CYAN << "● " << user << RESET << std::endl;
        }
    }
}

void TerminalUI::onMessage(const std::string& message) {
    std::cout << "\r" << YELLOW << "[PUBLIC] " << message << RESET << std::endl;
    std::cout << CYAN << "Chat> " << RESET << std::flush;
}

void TerminalUI::onPrivateMessage(const std::string& sender, const std::string& message) {
    std::cout << "\r" << PURPLE << "[PRIVATE] " << sender << ": " << message << RESET << std::endl;
    std::cout << CYAN << "Chat> " << RESET << std::flush;
}

void TerminalUI::onUserList(const std::vector<std::string>& users) {
    online_users_ = users;
    showOnlineUsers(users);
    std::cout << CYAN << "Chat> " << RESET << std::flush;
}

void TerminalUI::onSystemMessage(const std::string& message) {
    std::cout << "\r" << GREEN << "[SYSTEM] " << message << RESET << std::endl;
    std::cout << CYAN << "Chat> " << RESET << std::flush;
}

std::string TerminalUI::getInput(const std::string& prompt, bool hidden) {
    std::cout << prompt;
    std::string input;

    if (hidden) {
        // Simple hidden input (not secure, but works for demo)
        std::getline(std::cin, input);
    } else {
        std::getline(std::cin, input);
    }

    return input;
}

void TerminalUI::clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void TerminalUI::printColored(const std::string& text, const std::string& color) {
    std::cout << color << text << RESET << std::endl;
}

} // namespace chat