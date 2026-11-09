add_library(funnyparse STATIC
        src/Diagnostic.cpp
        src/Token.cpp
        src/Ast.cpp
        src/AstTree.cpp
        src/AstJson.cpp
        src/ParserImpl.cpp
        src/ParseExpr.cpp
        src/ParseCond.cpp
        src/ParseStmt.cpp
        src/Parser.cpp
        src/Snapshot.cpp
)

target_include_directories(funnyparse PUBLIC Include)
