find_package(GTest REQUIRED)

add_executable(funnysema_tests
        SymbolTest.cpp
        TypeTest.cpp
        InferTest.cpp
        InitTest.cpp
        PrintTest.cpp
        SnapshotTest.cpp
        RobustTest.cpp
)

target_link_libraries(funnysema_tests PRIVATE funnysema GTest::gtest GTest::gtest_main)

# Каталог эталонов вшивается при сборке: тесты работают из любого текущего каталога.
target_compile_definitions(funnysema_tests PRIVATE FUNNY_TESTS_DIR="${CMAKE_SOURCE_DIR}/tests")

add_test(NAME funnysema_unit COMMAND funnysema_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(funnysema_unit PROPERTIES TIMEOUT 120)
add_test(NAME funnysema_cases COMMAND funnycheck_cli test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME funnysema_missing_file COMMAND funnycheck_cli no_such_file.funny WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(funnysema_missing_file PROPERTIES PASS_REGULAR_EXPRESSION "не удалось открыть")
