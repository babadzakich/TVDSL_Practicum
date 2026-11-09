find_package(GTest REQUIRED)

add_executable(funnyir_tests
        LoweringTest.cpp
        IRPrinterTest.cpp
        OptimizeTest.cpp
        LivenessTest.cpp
        IRInterpreter.cpp
)
target_link_libraries(funnyir_tests PRIVATE funnyir funnysema GTest::gtest GTest::gtest_main)
add_test(NAME funnyir_unit COMMAND funnyir_tests)
target_compile_definitions(funnyir_tests PRIVATE FUNNY_HW4_TESTS_DIR="${FUNNY_HW4_SOURCE_DIR}/tests")
