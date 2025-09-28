cmake_minimum_required(VERSION 3.15)
project(CLIChat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)

# Find Redis++ (hiredis)
pkg_check_modules(HIREDIS REQUIRED hiredis)

# Include directories
include_directories(src/common)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

# Common library
add_library(common STATIC
    src/common/config.cpp
    src/common/crypto.cpp
    src/common/database.cpp
    src/common/message.cpp
    src/common/auth.cpp
    src/common/logger.cpp
    src/common/redis_client.cpp
)

target_link_libraries(common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    SQLite::SQLite3
    ${HIREDIS_LIBRARIES}
)

# Server executable
add_executable(chat_server
    src/server/main.cpp
    src/server/server.cpp
    src/server/session.cpp
    src/server/rest_api.cpp
)

target_link_libraries(chat_server
    common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    SQLite::SQLite3
    ${HIREDIS_LIBRARIES}
)

# Client executable
add_executable(chat_client
    src/client/main.cpp
    src/client/client.cpp
    src/client/terminal_ui.cpp
)

target_link_libraries(chat_client
    common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

# Tests
enable_testing()
add_subdirectory(src/tests)

# Install targets
install(TARGETS chat_server chat_client
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/cli-chat
)