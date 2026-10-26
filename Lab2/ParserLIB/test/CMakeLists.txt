find_package(GTest REQUIRED)

add_executable(funnyparse_tests
        LexTest.cpp
        ExprTest.cpp
        CondTest.cpp
        StmtTest.cpp
        JsonTest.cpp
        SnapshotTest.cpp
        FuzzTest.cpp
        SExpr.cpp
)

target_link_libraries(funnyparse_tests PRIVATE funnyparse GTest::gtest GTest::gtest_main)

# Каталог эталонов вшивается при сборке: тесты работают из любого текущего каталога.
target_compile_definitions(funnyparse_tests PRIVATE FUNNY_TESTS_DIR="${CMAKE_SOURCE_DIR}/tests")

add_test(NAME funnyparse_unit COMMAND funnyparse_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(funnyparse_unit PROPERTIES TIMEOUT 120)
add_test(NAME funnyparse_cases COMMAND funnyparse_cli test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME funnyparse_missing_file COMMAND funnyparse_cli no_such_file.funny WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(funnyparse_missing_file PROPERTIES PASS_REGULAR_EXPRESSION "не удалось открыть")
