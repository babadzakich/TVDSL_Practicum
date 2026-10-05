add_library(funnylex STATIC
        src/Regex.cpp
        src/Nfa.cpp
        src/Dfa.cpp
        src/TokenSpec.cpp
        src/Lexer.cpp
        src/Export.cpp
        src/TestCases.cpp
)

target_include_directories(funnylex PUBLIC Include)
