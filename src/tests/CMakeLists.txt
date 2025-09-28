find_package(GTest REQUIRED)

# Test executable
add_executable(tests
    test_crypto.cpp
    test_database.cpp
    test_redis.cpp
    test_config.cpp
)

target_link_libraries(tests
    common
    GTest::gtest_main
    GTest::gmock_main
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    SQLite::SQLite3
    ${HIREDIS_LIBRARIES}
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(tests)