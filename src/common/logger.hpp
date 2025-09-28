#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace chat {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4
};

class Logger {
public:
    static Logger& getInstance();

    void setLogLevel(LogLevel level);
    void setLogFile(const std::string& filename);

    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);

    // Formatted logging
    template<typename... Args>
    void debugf(const std::string& format, Args... args);

    template<typename... Args>
    void infof(const std::string& format, Args... args);

    template<typename... Args>
    void warningf(const std::string& format, Args... args);

    template<typename... Args>
    void errorf(const std::string& format, Args... args);

private:
    Logger() = default;

    void log(LogLevel level, const std::string& message);
    std::string formatMessage(LogLevel level, const std::string& message);
    std::string levelToString(LogLevel level);
    std::string getCurrentTimestamp();

    LogLevel log_level_ = LogLevel::INFO;
    std::string log_file_;
    std::unique_ptr<std::ofstream> file_stream_;
    std::mutex log_mutex_;
};

} // namespace chat