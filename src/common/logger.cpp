#include "logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace chat {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_level_ = level;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_file_ = filename;

    if (!filename.empty()) {
        file_stream_ = std::make_unique<std::ofstream>(filename, std::ios::app);
        if (!file_stream_->is_open()) {
            std::cerr << "Failed to open log file: " << filename << std::endl;
            file_stream_.reset();
        }
    } else {
        file_stream_.reset();
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::fatal(const std::string& message) {
    log(LogLevel::FATAL, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < log_level_) {
        return;
    }

    std::lock_guard<std::mutex> lock(log_mutex_);

    std::string formatted_message = formatMessage(level, message);

    // Output to console
    if (level >= LogLevel::ERROR) {
        std::cerr << formatted_message << std::endl;
    } else {
        std::cout << formatted_message << std::endl;
    }

    // Output to file if configured
    if (file_stream_ && file_stream_->is_open()) {
        *file_stream_ << formatted_message << std::endl;
        file_stream_->flush();
    }
}

std::string Logger::formatMessage(LogLevel level, const std::string& message) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] "
        << "[" << levelToString(level) << "] "
        << message;
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace chat