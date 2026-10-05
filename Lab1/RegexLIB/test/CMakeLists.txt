find_package(GTest REQUIRED)

add_executable(funnylex_tests
        RegexParserTest.cpp
        AutomataTest.cpp
        LexerTest.cpp
        GeneratedTableTest.cpp
)

target_link_libraries(funnylex_tests PRIVATE funnylex GTest::gtest GTest::gtest_main)

target_include_directories(funnylex_tests PRIVATE ${CMAKE_SOURCE_DIR}/artifacts)


add_test(NAME funnylex_unit COMMAND funnylex_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME funnylex_cases COMMAND funnylex_cli test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
