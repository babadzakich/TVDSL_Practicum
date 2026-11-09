cmake_minimum_required(VERSION 3.25)
project(FunnyIR CXX)
set(FUNNY_HW4_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
enable_testing()

add_subdirectory(../Lab3/ParserLIB ParserLIB)
add_subdirectory(../Lab3/SemaLIB SemaLIB)
add_subdirectory(IRLIB)
add_subdirectory(Lab4Main)
