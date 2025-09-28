#include <iostream>
#include <thread>
#include "server.hpp"
#include "config.hpp"

int main(int argc, char* argv[]) {
    try {
        // Load configuration
        auto& config = chat::Config::getInstance();

        // Load from config file if provided
        if (argc > 1) {
            config.loadFromFile(argv[1]);
        } else {
            config.loadFromFile("config/server.conf");
        }

        // Load from environment variables (overrides file config)
        config.loadFromEnv();

        std::cout << "=== CLI-Chat Server ===" << std::endl;
        std::cout << "Enhanced with TLS, Authentication, and Database persistence" << std::endl;
        std::cout << "=======================================================" << std::endl;

        // Create IO context
        boost::asio::io_context io_context;

        // Create and initialize server
        chat::ChatServer server(io_context);
        if (!server.initialize()) {
            std::cerr << "[ERROR] Failed to initialize server" << std::endl;
            return 1;
        }

        // Start server
        server.start();

        // Run server in multiple threads for better performance
        std::vector<std::thread> threads;
        unsigned int thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 2;

        std::cout << "[INFO] Starting " << thread_count << " worker threads" << std::endl;

        for (unsigned int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }

        std::cout << "[INFO] Server shutdown complete" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}