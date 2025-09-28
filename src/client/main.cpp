#include <iostream>
#include <thread>
#include "client.hpp"
#include "terminal_ui.hpp"
#include "config.hpp"

int main(int argc, char* argv[]) {
    try {
        // Load configuration
        auto& config = chat::Config::getInstance();

        // Load from config file if provided
        if (argc > 1) {
            config.loadFromFile(argv[1]);
        }

        // Load from environment variables
        config.loadFromEnv();

        std::cout << "=== CLI-Chat Client ===" << std::endl;
        std::cout << "Secure TLS-enabled chat client" << std::endl;
        std::cout << "===============================" << std::endl;

        // Create IO context
        boost::asio::io_context io_context;

        // Create client
        chat::ChatClient client(io_context);

        // Create terminal UI
        chat::TerminalUI ui(client);

        // Run IO context in separate thread
        std::thread io_thread([&io_context]() {
            io_context.run();
        });

        // Run terminal UI in main thread
        ui.run();

        // Cleanup
        io_context.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }

        std::cout << "Client shutdown complete" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Client error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}