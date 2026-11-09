add_library(funnyir_cli_lib STATIC Cli.cpp)
target_include_directories(funnyir_cli_lib PUBLIC .)
target_link_libraries(funnyir_cli_lib PUBLIC funnysema funnyir)

add_executable(funnyir_cli funnyir.cpp)
set_target_properties(funnyir_cli PROPERTIES OUTPUT_NAME funnyir)
target_link_libraries(funnyir_cli PRIVATE funnyir_cli_lib)

add_subdirectory(test)
