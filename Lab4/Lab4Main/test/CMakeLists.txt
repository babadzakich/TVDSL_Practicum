find_package(GTest REQUIRED)

add_executable(funnyir_cli_tests CliTest.cpp)
target_link_libraries(funnyir_cli_tests PRIVATE funnyir_cli_lib GTest::gtest GTest::gtest_main)
add_test(NAME funnyir_cli COMMAND funnyir_cli_tests)
