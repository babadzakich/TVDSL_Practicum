add_executable(funnylex_cli funnylex.cpp)
set_target_properties(funnylex_cli PROPERTIES OUTPUT_NAME funnylex)

target_link_libraries(funnylex_cli PRIVATE funnylex)
