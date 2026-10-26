add_library(funnysema STATIC
        src/Symbols.cpp
        src/Checker.cpp
        src/CheckExpr.cpp
        src/Sema.cpp
        src/SemaSnapshot.cpp
)

target_include_directories(funnysema PUBLIC Include)
target_link_libraries(funnysema PUBLIC funnyparse)
