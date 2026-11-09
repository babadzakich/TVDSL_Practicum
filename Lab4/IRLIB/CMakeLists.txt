add_library(funnyir STATIC
        src/IR.cpp
        src/IRPrinter.cpp
        src/Lowering.cpp
        src/Optimize.cpp
        src/RegisterAllocation.cpp
)
target_include_directories(funnyir PUBLIC Include)
target_link_libraries(funnyir PUBLIC funnysema)
add_subdirectory(test)
