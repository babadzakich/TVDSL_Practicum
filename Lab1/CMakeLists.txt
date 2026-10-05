cmake_minimum_required(VERSION 3.25)
project(FunnyLexer CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

find_program(CLANG_FORMAT "clang-format")

if (CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
            "*.cpp" "*.hpp" "*.c" "*.h"
    )

    add_custom_target(
            format
            COMMAND ${CLANG_FORMAT}
            -i
            -style=file
            ${ALL_SOURCE_FILES}
            COMMENT "Formatting all source files with clang-format..."
    )
else()
    message(WARNING "clang-format not found. The 'format' target will not be available.")
endif()

find_program(CLANG_TIDY "clang-tidy")

if (CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY}
            -config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            -extra-arg=-Wno-unknown-warning-option
    )
else()
    message(WARNING "clang-tidy not found. Static analysis will not be performed.")
endif()

enable_testing()

add_subdirectory(RegexLIB)
add_subdirectory(RegexMain)
add_subdirectory(RegexLIB/test)
