# Funny PEG Parser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A hand-written PEG parser for the full Funny language, running over the token stream of the HW1 lexer. It produces an AST (text tree and JSON) and Russian diagnostics with positions, and it recovers from errors.

**Architecture:** A token layer wraps `funny_lexer::next_token` and adds positions, skips whitespace and comments, and reports lexical errors. A `ParserImpl` class holds the token cursor, the farthest-failure tracker, labels, the expression memo and the depth guard. Rule functions are split by grammar area: expressions, conditions/predicates, statements/declarations. Printers (tree, JSON), a snapshot runner and the `funnyparse` CLI sit on top.

**Tech Stack:** C++23, CMake ≥ 3.25 + Ninja, GoogleTest (`find_package(GTest REQUIRED)`; the system package works, as does conan), clang-tidy and clang-format from the existing skeleton.

**Spec:** `Lab2/docs/superpowers/specs/2026-10-19-funny-peg-parser-design.md` (language: `Lab2/docs/funny.ru.md`)

## Global Constraints

- All paths are relative to `Lab2/`. Build and test with `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`.
- Do not modify the root `.gitignore`, `Lab1/`, or the copy of `funny_lexer_table.h`. No third-party parser libraries.
- Work on a branch, not `main`. Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Code style follows Lab1: types in PascalCase, functions/variables in snake_case, constants `kName`, private members `name_`, header guards `FUNNYPARSE_<FILE>_H`, sparse comments in Russian. Parser namespace: `funny`.
- Diagnostic format: `<label>:<line>:<col>: ошибка: <message>`. Line and column are 1-based. The column counts UTF-8 code points; a tab counts as 1.
- Syntax message template: `ожидается <list>, найдено '<lexeme>'`. At EOF: `ожидается <list>, найден конец файла`. A list reads `A`, `A или B`, or `A, B или C`, in order of first recording, without duplicates.
- Expected-item names: keywords and punctuation as the lexeme in single quotes (`';'`, `'returns'`, `'=>'`), `IDENT` → `идентификатор`, `INT` → `число`, EOF → `конец файла`.
- Labels: `Decl` → `объявление функции или формулы`, `Statement` → `оператор`, operand (`Unary`) → `выражение`, `Condition` → `условие`, `Predicate` → `предикат`, `Call` in a tuple assignment → `вызов функции`.
- Silent (not recorded as expected): continuations of binary operator loops (`+ - * /`, `and`, `or`, `->`), the `}` check in the block loop, all lookaheads (`peek`), including the choice between Call/ArrayAccess/Var by the token after `IDENT` and the choice between Tuple/Array/Simple assignment. Recorded: mandatory tokens, keywords of optional clauses (`requires`, `ensures`, `uses`, `invariant`, `else`), the `[` in `Type`, and `,` in lists.
- Fixed lexical messages:
  - `недопустимый символ '<c>'` for one code point, `недопустимые символы '<text>'` for several adjacent ones. Bytes < 0x20 and 0x7F are escaped as `\xHH`.
  - `некорректное число '<text>': ведущие нули не допускаются`
  - `недопустимые символы в комментарии (разрешён только ASCII)`
- Other fixed messages:
  - `ожидается '}' (для '{' в L:C), найден конец файла`
  - `слишком глубокая вложенность (больше 1000 уровней)`
- AST is printed only when there are no diagnostics. CLI exit codes: 0 = ok, 1 = lexical/syntax errors, 2 = usage or I/O error.

## Review Focus

1. Deeply nested statements (thousands of `{` or `if (...)`) → one depth diagnostic, not a stack overflow. Test in Task 4.
2. Binary garbage and NUL bytes in the input → one coalesced lexical diagnostic with `\xHH` escapes, no crash. Test in Task 1.
3. A huge integer literal (`99999999999999999999`) → parsed, with `IntLit` text preserved verbatim and no overflow. Test in Task 2.
4. A keyword used as a name (`f(int: int) ...`, `length = 1;`) → a clear "expected identifier/statement" error. Test in Task 7 (corpus file 25).
5. The CLI given a missing file → exit code 2 and the message `не удалось открыть '<path>'`. ctest in Task 6.

---

### Task 1: Scaffolding and token layer

**Files:**
- Modify: `CMakeLists.txt` (the line `add_subdirectory(Parser/test)` → `add_subdirectory(ParserLIB/test)`)
- Create: `ParserLIB/CMakeLists.txt`, `ParserLIB/test/CMakeLists.txt`, `ParserMain/CMakeLists.txt`
- Create: `ParserLIB/Include/funny_lexer_table.h` (byte-for-byte copy of `../Lab1/artifacts/funny_lexer_table.h`)
- Create: `ParserLIB/Include/Diagnostic.h`, `ParserLIB/src/Diagnostic.cpp`
- Create: `ParserLIB/Include/Token.h`, `ParserLIB/src/Token.cpp`
- Create: `ParserMain/funnyparse.cpp` (the `tokens` command only for now)
- Test: `ParserLIB/test/LexTest.cpp`

**Interfaces:**
- Produces:

```cpp
// Diagnostic.h
namespace funny {
struct Pos { int line = 1; int col = 1; std::size_t offset = 0; };
struct Diagnostic { Pos pos; std::string message; };
std::string format_diagnostic(std::string_view label, const Diagnostic& d);   // "label:L:C: ошибка: msg"
void sort_diagnostics(std::vector<Diagnostic>& ds);                         // stable, by pos.offset
}
// Token.h
namespace funny {
using funny_lexer::TokenKind;
inline constexpr TokenKind kEof = static_cast<TokenKind>(funny_lexer::kNumTokens);
struct Token { TokenKind kind; std::string_view text; Pos pos; };   // text points into the source
struct LexResult { std::vector<Token> tokens; std::vector<Diagnostic> diagnostics; };  // tokens end with kEof
LexResult lex(std::string_view source);
std::string describe_kind(TokenKind kind);    // "идентификатор", "число", "конец файла", "';'", "'while'"
std::string quote_lexeme(std::string_view s); // "'x'" with \xHH escapes for control bytes
}
```

- CMake targets: library `funnyparse` (STATIC, `PUBLIC Include`), executable `funnyparse_cli` with `OUTPUT_NAME funnyparse`, test binary `funnyparse_tests` (links `funnyparse GTest::gtest GTest::gtest_main`), ctest `funnyparse_unit` with `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}`.

- [ ] **Step 1: Create the CMake files, the header copy, and an empty `LexTest.cpp`; check that the project configures**

Run: `cp ../Lab1/artifacts/funny_lexer_table.h ParserLIB/Include/ && cmake -S . -B build -G Ninja`
Expected: configuration succeeds (until the sources exist, temporary stubs are fine).

- [ ] **Step 2: Write the failing tests in `ParserLIB/test/LexTest.cpp`**

```cpp
// kinds(r) is a test helper: the token names via funny_lexer::token_name / "EOF", space-separated.
TEST(Lex, SkipsWhitespaceAndComments) {
  auto r = lex("x // c\n  y");
  EXPECT_EQ(kinds(r), "IDENT IDENT EOF");
  EXPECT_EQ(r.tokens[1].pos.line, 2); EXPECT_EQ(r.tokens[1].pos.col, 3);
  EXPECT_EQ(r.tokens[2].pos.col, 4);  EXPECT_TRUE(r.diagnostics.empty());
}
TEST(Lex, EmptyInputIsJustEof) { auto r = lex(""); EXPECT_EQ(kinds(r), "EOF"); EXPECT_EQ(r.tokens[0].pos.col, 1); }
TEST(Lex, IntArrayIsThreeTokens) { EXPECT_EQ(kinds(lex("int[]")), "KW_INT LBRACKET RBRACKET EOF"); }
TEST(Lex, CrLfLines) { auto r = lex("a\r\nb"); EXPECT_EQ(r.tokens[1].pos.line, 2); EXPECT_EQ(r.tokens[1].pos.col, 1); }
TEST(Lex, SingleBadCharStaysInStream) {
  auto r = lex("x = @;");
  EXPECT_EQ(kinds(r), "IDENT ASSIGN ERROR SEMICOLON EOF");
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]), "t:1:5: ошибка: недопустимый символ '@'");
}
TEST(Lex, AdjacentBadCharsCoalesceAndColumnsCountCodepoints) {
  auto r = lex("жж x");
  EXPECT_EQ(kinds(r), "ERROR IDENT EOF");
  EXPECT_EQ(r.tokens[1].pos.col, 4);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]), "t:1:1: ошибка: недопустимые символы 'жж'");
}
TEST(Lex, ControlBytesAreEscaped) {   // Review Focus 2
  auto r = lex(std::string_view("\0\x01", 2));
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "недопустимые символы '\\x00\\x01'");
}
TEST(Lex, BadIntBecomesIntWithOneDiagnostic) {
  auto r = lex("x = 007;");
  EXPECT_EQ(kinds(r), "IDENT ASSIGN INT SEMICOLON EOF");
  EXPECT_EQ(r.tokens[2].text, "007");
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]),
            "t:1:5: ошибка: некорректное число '007': ведущие нули не допускаются");
}
TEST(Lex, NonAsciiCommentReportedOnceRestOfLineSkipped) {
  auto r = lex("x = 1; // привет world\ny");
  EXPECT_EQ(kinds(r), "IDENT ASSIGN INT SEMICOLON IDENT EOF");
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]),
            "t:1:11: ошибка: недопустимые символы в комментарии (разрешён только ASCII)");
}
TEST(Lex, DescribeKind) {
  EXPECT_EQ(describe_kind(TokenKind::IDENT), "идентификатор");
  EXPECT_EQ(describe_kind(TokenKind::INT), "число");
  EXPECT_EQ(describe_kind(kEof), "конец файла");
  EXPECT_EQ(describe_kind(TokenKind::SEMICOLON), "';'");
  EXPECT_EQ(describe_kind(TokenKind::KW_RETURNS), "'returns'");
  EXPECT_EQ(describe_kind(TokenKind::FAT_ARROW), "'=>'");
}
```

- [ ] **Step 3: Run the tests and confirm they fail**

Run: `cmake --build build 2>&1 | tail -5`
Expected: compilation error (`lex`/`format_diagnostic` are not defined).

- [ ] **Step 4: Implement `Diagnostic.cpp` and `Token.cpp`**

Implementation notes for `lex`:
- Loop over `next_token(src, pos)`.
- The column grows only on bytes where `(b & 0xC0) != 0x80`. `\n` advances the line and resets the column to 1.
- A COMMENT that ends neither at `\n` nor at end of input stopped on a non-ASCII byte. Report the comment diagnostic at that byte, then skip to `\n`.
- `BAD_INT` gets a diagnostic, then becomes a token of kind `INT`.
- Adjacent ERROR tokens (the next one starts exactly where the previous one ended) merge into one token and one diagnostic.
- `describe_kind`: keywords take their text from the name with `KW_` removed, lower-cased. Punctuation uses an explicit lexeme table.

- [ ] **Step 5: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build -R funnyparse_unit --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 6: Add the `tokens` command to `ParserMain/funnyparse.cpp` and check it by hand**

Format: one line per token, `L:C KIND 'lexeme'`. The last line is `L:C EOF`. Lexical diagnostics go to stderr with the file name as label (`<stdin>` for stdin).
Run: `printf 'x = 1;' | ./build/ParserMain/funnyparse tokens`
Expected: `1:1 IDENT 'x'`, `1:3 ASSIGN '='`, `1:5 INT '1'`, `1:6 SEMICOLON ';'`, `1:7 EOF`.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt ParserLIB ParserMain
git commit -m "lab2: token layer over HW1 lexer with positions and lexical diagnostics"
```

---

### Task 2: AST, tree printer, PEG core, expressions

**Files:**
- Create: `ParserLIB/Include/Ast.h`, `ParserLIB/src/Ast.cpp` (`clone`)
- Create: `ParserLIB/Include/AstPrinter.h`, `ParserLIB/src/AstTree.cpp`
- Create: `ParserLIB/Include/Parser.h`, `ParserLIB/src/ParserImpl.h`, `ParserLIB/src/ParserImpl.cpp` (cursor, tracker, labels, memo, depth), `ParserLIB/src/ParseExpr.cpp`, `ParserLIB/src/Parser.cpp` (public entry points)
- Test: `ParserLIB/test/SExpr.h`, `ParserLIB/test/SExpr.cpp` (test-only S-expression printer), `ParserLIB/test/ExprTest.cpp`

**Interfaces:**
- Consumes: `lex`, `Token`, `kEof`, `describe_kind`, `quote_lexeme`, `Diagnostic`, `sort_diagnostics` (Task 1).
- Produces: the whole `Ast.h` (later tasks only fill in the parsing and printing of the nodes already declared here):

```cpp
namespace funny {
enum class TypeKind { Int, IntArray };
struct Name { std::string text; Pos pos; };
struct VarDef { std::string name; TypeKind type; Pos pos; };
struct LocalDef { std::string name; std::optional<TypeKind> type; Pos pos; };

struct Expr; using ExprPtr = std::unique_ptr<Expr>;
struct IntLit { std::string text; };
struct VarRef { std::string name; };
struct ArrayAccess { std::string array; ExprPtr index; };
struct Call { std::string callee; std::vector<ExprPtr> args; };
struct Neg { ExprPtr operand; };
enum class BinOp { Add, Sub, Mul, Div };
struct Binary { BinOp op; ExprPtr lhs, rhs; };
struct Expr { Pos pos; std::variant<IntLit, VarRef, ArrayAccess, Call, Neg, Binary> node; };

struct Cond; using CondPtr = std::unique_ptr<Cond>;   // both conditions and predicates
enum class CmpOp { Eq, Ne, Ge, Le, Gt, Lt };
enum class LogicOp { And, Or, Implies };
struct BoolLit { bool value; };
struct Compare { CmpOp op; ExprPtr lhs, rhs; };
struct Not { CondPtr operand; };
struct Logical { LogicOp op; CondPtr lhs, rhs; };
struct Quantifier { bool is_forall; VarDef var; CondPtr body; };
struct FormulaRef { std::string name; std::vector<ExprPtr> args; };
struct Cond { Pos pos; std::variant<BoolLit, Compare, Not, Logical, Quantifier, FormulaRef> node; };

struct Stmt; using StmtPtr = std::unique_ptr<Stmt>;
enum class Sugar { None, ArrayUpdate };
struct Block { std::vector<StmtPtr> body; };
struct If { CondPtr cond; StmtPtr then_branch; StmtPtr else_branch; };       // else_branch may be null
struct While { CondPtr cond; CondPtr invariant; StmtPtr body; };            // invariant may be null
struct Assert { CondPtr pred; };
struct Assume { CondPtr pred; };
struct Assign { Name target; ExprPtr value; Sugar sugar = Sugar::None; };
struct TupleAssign { std::vector<Name> targets; ExprPtr call; };            // call holds a Call
struct Stmt { Pos pos; std::variant<Block, If, While, Assert, Assume, Assign, TupleAssign> node; };

struct Function { Name name; std::vector<VarDef> params; CondPtr requires_clause;
                  std::vector<VarDef> returns; CondPtr ensures_clause; std::vector<LocalDef> uses; StmtPtr body; };
struct Formula { Name name; std::vector<VarDef> params; CondPtr body; };
using Decl = std::variant<Function, Formula>;
struct Module { std::vector<Decl> decls; };

ExprPtr clone(const Expr& e);
std::string_view to_string(BinOp op);   // "+" "-" "*" "/"
std::string_view to_string(CmpOp op);   // "==" "!=" ">=" "<=" ">" "<"
std::string to_string(TypeKind t);      // "int" "int[]"
}
```

```cpp
// Parser.h
namespace funny {
inline constexpr int kMaxDepth = 1000;
template <class T> struct FragmentResult { T node; std::vector<Diagnostic> diagnostics; };
FragmentResult<ExprPtr> parse_expression(std::string_view source);   // the whole input must be consumed
}
// AstPrinter.h
namespace funny { std::string print_tree(const Expr& e); }   // Task 3 adds Cond, Task 4 adds Stmt/Module
// test/SExpr.h
std::string sexpr(const funny::Expr& e);
```

- `ParserImpl` (internal, `src/ParserImpl.h`). **Contract: a rule function returns `nullptr`/`std::nullopt` on failure and has already restored the cursor to its start.**

```cpp
class ParserImpl {
 public:
  explicit ParserImpl(std::vector<Token> tokens);
  const Token& peek(std::size_t ahead = 0) const;   // past the end → the kEof token
  bool at(TokenKind k, std::size_t ahead = 0) const;
  bool accept(TokenKind k);          // silent
  bool expect(TokenKind k);          // records describe_kind(k) on failure
  std::size_t mark() const; void reset(std::size_t m);
  void fail(std::string what);       // records at the current position
  template <class F> auto labeled(std::string_view label, F&& f) -> decltype(f());
  std::optional<Diagnostic> farthest_diagnostic() const; // nullopt if the farthest token is ERROR (suppression)
  void clear_failures();             // resets the tracker AND the memo
  struct DepthGuard { explicit DepthGuard(ParserImpl&); ~DepthGuard(); ParserImpl& p; };  // throws DepthExceeded{Pos}
  ExprPtr expr();                    // Task 3 adds condition()/predicate(), Task 4 statement()/decl()/module()
};
```

`labeled` algorithm (not determined by the signature):

```
start = mark(); saved = tracker
r = f()
if !r && tracker.far == start:
    tracker.expected = (saved.far == start ? saved.expected : {}) + {label}
return r
```

Tracker: `far` is a token index. `fail` at index `i`: if `i > far`, set `far = i` and `expected = {what}`; if `i == far`, append `what` unless already present.

Expression memo: `std::unordered_map<std::size_t, {std::size_t end; ExprPtr result}>` keyed by start index, covering `expr()` only. On a hit, set the cursor to `end` and return `clone(*result)` (or `nullptr`). `clear_failures()` empties it.

The `DepthGuard` goes in `unary()` (and later in `c_not()`, `p_not()`, `statement()`). `DepthExceeded` is caught in the public entry points. The result is one diagnostic, `слишком глубокая вложенность (больше 1000 уровней)`, at the position of the token where the limit was exceeded, plus the lexical diagnostics.

`Primary` chooses by peek: `IDENT` + `(` → Call; `IDENT` + `[` → ArrayAccess; `IDENT` alone → Var; `KW_LENGTH` → Call (`expect('(')`).

Node positions: a node's position is the position of its first token. A parenthesized expression keeps the inner node's position. `Binary` takes the position of `lhs`; `Neg` the position of the `-`.

Tree format (each line is `<Kind> [details] @L:C`, children indented by 2 spaces, output ends with `\n`): `IntLit 42`, `Var x`, `ArrayAccess a` + index, `Call f` + args, `Neg` + operand, `Binary +` + lhs, rhs.

S-expression format: `42`, `x`, `(index a i)`, `(call f a b)` / `(call f)`, `(neg x)`, `(+ a b)`.

- [ ] **Step 1: Write the failing tests in `ExprTest.cpp`** (helper `sx(src)`: `parse_expression`, `EXPECT_TRUE(diagnostics.empty())`, returns `sexpr`; helper `err(src)`: diagnostics via `format_diagnostic("t", d)` joined with `\n`)

```cpp
TEST(Expr, LeftAssociative) { EXPECT_EQ(sx("1 - 2 - 3"), "(- (- 1 2) 3)"); EXPECT_EQ(sx("8 / 4 / 2"), "(/ (/ 8 4) 2)"); }
TEST(Expr, MulBindsTighter) { EXPECT_EQ(sx("1 + 2 * 3"), "(+ 1 (* 2 3))"); }
TEST(Expr, UnaryMinusTightest) {
  EXPECT_EQ(sx("-a * b"), "(* (neg a) b)"); EXPECT_EQ(sx("- - a"), "(neg (neg a))"); EXPECT_EQ(sx("a - -b"), "(- a (neg b))");
}
TEST(Expr, Parens) { EXPECT_EQ(sx("(1 + 2) * 3"), "(* (+ 1 2) 3)"); }
TEST(Expr, CallsAndIndex) {
  EXPECT_EQ(sx("f(a, g(b), c[1])"), "(call f a (call g b) (index c 1))");
  EXPECT_EQ(sx("length(a)"), "(call length a)"); EXPECT_EQ(sx("f()"), "(call f)"); EXPECT_EQ(sx("a[i + 1]"), "(index a (+ i 1))");
}
TEST(Expr, HugeLiteralKeptVerbatim) { EXPECT_EQ(sx("99999999999999999999"), "99999999999999999999"); }  // Review Focus 3
TEST(Expr, TreeFormat) {
  auto r = parse_expression("a[i] + 1");
  EXPECT_EQ(print_tree(*r.node), "Binary + @1:1\n  ArrayAccess a @1:1\n    Var i @1:3\n  IntLit 1 @1:8\n");
}
TEST(Expr, Errors) {
  EXPECT_EQ(err("1 +"), "t:1:4: ошибка: ожидается выражение, найден конец файла");
  EXPECT_EQ(err("(1 + 2"), "t:1:7: ошибка: ожидается ')', найден конец файла");
  EXPECT_EQ(err("1 2"), "t:1:3: ошибка: ожидается конец файла, найдено '2'");
  EXPECT_EQ(err("f(a b)"), "t:1:5: ошибка: ожидается ',' или ')', найдено 'b'");
  EXPECT_EQ(err("x = @"), "t:1:3: ошибка: ожидается конец файла, найдено '='\nt:1:5: ошибка: недопустимый символ '@'");
}
TEST(Expr, DepthLimit) {
  auto r = parse_expression(std::string(5000, '(') + "1" + std::string(5000, ')'));
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "слишком глубокая вложенность (больше 1000 уровней)");
}
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run: `cmake --build build 2>&1 | tail -3`
Expected: compilation error (`parse_expression` is not defined).

- [ ] **Step 3: Implement `Ast.cpp`, `AstTree.cpp` (expressions), `ParserImpl.*`, `ParseExpr.cpp`, and `Parser.cpp::parse_expression`**

`parse_expression` = `lex` → `ParserImpl` → `expr()` → `expect(kEof)`. On failure, add `farthest_diagnostic()` (if present) to the lexical diagnostics, then `sort_diagnostics`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build -R funnyparse_unit --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 5: Commit**

```bash
git add ParserLIB
git commit -m "lab2: AST, PEG core with farthest-failure tracking, expression rules"
```

---

### Task 3: Conditions and predicates

**Files:**
- Create: `ParserLIB/src/ParseCond.cpp`
- Modify: `ParserLIB/src/ParserImpl.h` (add `condition()`, `predicate()` and helper rules), `ParserLIB/src/Parser.cpp`, `ParserLIB/Include/Parser.h`, `ParserLIB/src/AstTree.cpp`, `ParserLIB/Include/AstPrinter.h`, `ParserLIB/test/SExpr.*`
- Test: `ParserLIB/test/CondTest.cpp`

**Interfaces:**
- Consumes: `ParserImpl::expr()`, `labeled`, `DepthGuard` (Task 2); the `Cond` nodes from `Ast.h`.
- Produces:

```cpp
FragmentResult<CondPtr> parse_condition(std::string_view source);
FragmentResult<CondPtr> parse_predicate(std::string_view source);
std::string print_tree(const Cond& c);
std::string sexpr(const funny::Cond& c);   // test
// ParserImpl: CondPtr condition();  CondPtr predicate();  std::optional<VarDef> var_def();  std::optional<TypeKind> type();
```

- Grammar from spec §5: `Condition`/`COr`/`CAnd`/`CNot`/`CAtom` and `Predicate`/`POr`/`PAnd`/`PNot`/`PAtom`, `Quantifier`, `FormulaRef`, `Comparison`, `CmpOp`. Real PEG choice with rollback in `CAtom` (`Comparison` before `'(' Condition ')'`) and in `PAtom` (`Comparison` before `FormulaRef` before `'(' Predicate ')'`).
- `Condition` and `Predicate` are wrapped in `labeled("условие")` / `labeled("предикат")`. A `DepthGuard` goes in `c_not()`/`p_not()`. `CmpOp` is recorded as six separate items in the order `==`, `!=`, `>=`, `<=`, `>`, `<`.
- `->` is right-associative: `Condition <- COr ('->' Condition)?` (the `->` check is silent).
- Positions: `Logical`/`Compare` take the position of the left operand, `Not` the position of `not`, `Quantifier` the position of the `forall`/`exists` keyword, `BoolLit` its own token.
- Tree format: `BoolLit true`, `Compare ==` + lhs, rhs; `Not` + operand; `And`/`Or`/`Implies` + lhs, rhs; `Quantifier forall i: int` + body; `FormulaRef p` + args.
- S-expressions: `true`, `(== a b)`, `(not a)`, `(and a b)`, `(or a b)`, `(-> a b)`, `(forall i:int body)` / `(exists b:int[] body)`, `(ref p a b)`.

- [ ] **Step 1: Write the failing tests in `CondTest.cpp`** (helpers `csx`/`psx`/`cerr`/`perr`, same as in Task 2)

```cpp
TEST(Cond, Precedence) {
  EXPECT_EQ(csx("a > 0 and b > 0 or c > 0"), "(or (and (> a 0) (> b 0)) (> c 0))");
  EXPECT_EQ(csx("not a == b and c != d"), "(and (not (== a b)) (!= c d))");
  EXPECT_EQ(csx("true or false"), "(or true false)");
}
TEST(Cond, ImplicationRightAssoc) { EXPECT_EQ(csx("a > 0 -> b > 0 -> c > 0"), "(-> (> a 0) (-> (> b 0) (> c 0)))"); }
TEST(Cond, ParenAmbiguity) {
  EXPECT_EQ(csx("(a + b) > c"), "(> (+ a b) c)");
  EXPECT_EQ(csx("(a > b)"), "(> a b)");
  EXPECT_EQ(csx("((a > b) and (c > d))"), "(and (> a b) (> c d))");
}
TEST(Cond, DeepParensFinish) {   // memo + depth: 300 levels parse
  EXPECT_EQ(csx(std::string(300, '(') + "x > 0" + std::string(300, ')')), "(> x 0)");
}
TEST(Pred, QuantifiersAndRefs) {
  EXPECT_EQ(psx("forall(i: int | a[i] >= 0)"), "(forall i:int (>= (index a i) 0))");
  EXPECT_EQ(psx("exists(b: int[] | p(b))"), "(exists b:int[] (ref p b))");
  EXPECT_EQ(psx("sorted(a, n) and f(x) == 3"), "(and (ref sorted a n) (== (call f x) 3))");
}
TEST(Cond, Errors) {
  EXPECT_EQ(cerr("x = 1"), "t:1:3: ошибка: ожидается '==', '!=', '>=', '<=', '>' или '<', найдено '='");
  EXPECT_EQ(cerr("forall(i: int | i > 0)"), "t:1:1: ошибка: ожидается условие, найдено 'forall'");
  EXPECT_EQ(cerr("a < b < c"), "t:1:7: ошибка: ожидается конец файла, найдено '<'");
  EXPECT_EQ(perr("a > 0 -> b > 0"), "t:1:7: ошибка: ожидается конец файла, найдено '->'");
  EXPECT_EQ(perr("forall(i int | true)"), "t:1:10: ошибка: ожидается ':', найдено 'int'");
}
TEST(Cond, TreeFormat) {
  auto r = parse_predicate("not forall(i: int | i > 0)");
  EXPECT_EQ(print_tree(*r.node),
            "Not @1:1\n  Quantifier forall i: int @1:5\n    Compare > @1:21\n      Var i @1:21\n      IntLit 0 @1:25\n");
}
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run: `cmake --build build 2>&1 | tail -3`
Expected: compilation error (`parse_condition` is not defined).

- [ ] **Step 3: Implement `ParseCond.cpp`, the entry points, the tree printer and sexpr for `Cond`**

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build -R funnyparse_unit --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 5: Commit**

```bash
git add ParserLIB
git commit -m "lab2: conditions and predicates with PEG ordered choice"
```

---

### Task 4: Statements, declarations, module; `grammar.peg`

**Files:**
- Create: `ParserLIB/src/ParseStmt.cpp` (statements, `Function`/`Formula`, `Module`)
- Modify: `ParserLIB/src/ParserImpl.h`, `ParserLIB/src/Parser.cpp`, `ParserLIB/Include/Parser.h`, `ParserLIB/src/AstTree.cpp`, `ParserLIB/Include/AstPrinter.h`, `ParserLIB/test/SExpr.*`
- Create: `grammar.peg`
- Test: `ParserLIB/test/StmtTest.cpp`

**Interfaces:**
- Consumes: `condition()`, `predicate()`, `var_def()`, `type()`, `expr()` (Tasks 2–3).
- Produces:

```cpp
struct ParseResult {
  Module module;                        // with errors: whatever parsed successfully; do not print it
  std::vector<Diagnostic> diagnostics;  // sorted by offset
  bool ok() const { return diagnostics.empty(); }
};
ParseResult parse(std::string_view source);
std::string print_tree(const Stmt& s);
std::string print_tree(const Module& m);
std::string sexpr(const funny::Stmt& s);  std::string sexpr(const funny::Decl& d);   // test
// ParserImpl: StmtPtr statement();  std::optional<Decl> decl();  Module module(std::vector<Diagnostic>& out);
```

- In this task `module()` stops at the first error: `Decl+ EOF`; on failure it adds `farthest_diagnostic()` and returns what it has. Recovery comes in Task 7.
- `decl()`: a silent lookahead `IDENT '(' Params? ')' '=>'` (without recording failures: save the tracker, run, restore) → `Formula`, otherwise `Function`. Wrapped in `labeled("объявление функции или формулы")`.
- `statement()`: choose by the first token (`{`, `if`, `while`, `assert`, `assume`, `IDENT`), wrapped in `labeled("оператор")`, with a `DepthGuard`. Assignment by peek on the second token: `,` → Tuple (the call goes through `labeled("вызов функции")`), `[` → ArrayAssign, otherwise Simple (`expect('=')`).
- `else` binds to the nearest `if` (falls out of the greedy `('else' Statement)?`).
- `a[i] = v;` → `Assign{target=a, value=Call{"set", [Var a, i, v]}, sugar=ArrayUpdate}`. The `Call` and the inner `Var a` take the position of `a`.
- `Function.name.pos` and `Formula.name.pos` are the position of the name. `Stmt.pos` is the first token. `Assign.target.pos` is the position of the variable.
- Tree format:
  - `Block` + statements;
  - `If` + groups `cond`, `then`, `else` (optional);
  - `While` + `cond`, `invariant` (optional), `body`;
  - `Assert`/`Assume` + child;
  - `Assign x`, or with sugar `Assign a [sugar: array_update]`, + value;
  - `TupleAssign q, r` + call;
  - `Function f` + groups `params` (`a: int[] @L:C`), `requires`, `returns`, `ensures`, `uses` (`i @L:C` / `t: int @L:C`), `body`;
  - `Formula p` + `params`, `body`;
  - `Module` (no position) + declarations.

  A group is a line with its name only (no position), with its content one level deeper. Empty or absent groups are not printed.
- S-expressions:
  - `(block s…)`;
  - `(if c t)` / `(if c t e)`;
  - `(while c body)` / `(while c (inv p) body)`;
  - `(assert p)`, `(assume p)`;
  - `(= x v)`, `(=sugar a (call set a i v))`, `(= (q r) (call f a))`;
  - `(function f (params a:int[]) (requires p) (returns r:int) (ensures p) (uses i t:int) body)`, with empty or absent groups omitted;
  - `(formula p (params x:int) body)`.

- [ ] **Step 1: Write the failing tests in `StmtTest.cpp`** (helper `msx(src)`: `parse`, `EXPECT_TRUE(ok())`, the sexpr of each decl joined with `\n`)

```cpp
TEST(Module, FunctionWithoutKeyword) {
  EXPECT_EQ(msx("f(x: int) returns y: int y = x;"), "(function f (params x:int) (returns y:int) (= y x))");
}
TEST(Module, FormulaAndFullHeader) {
  EXPECT_EQ(msx("p(x: int) => x > 0\n"
                "g(a: int[]) requires p(1) returns r: int, s: int[] ensures true uses i, t: int { }"),
            "(formula p (params x:int) (> x 0))\n"
            "(function g (params a:int[]) (requires (ref p 1)) (returns r:int s:int[]) (ensures true) (uses i t:int) (block))");
}
TEST(Stmt, ArraySugarAndTuple) {
  EXPECT_EQ(msx("f(v: int) returns a: int[] { a[0] = v; }"),
            "(function f (params v:int) (returns a:int[]) (block (=sugar a (call set a 0 v))))");
  EXPECT_EQ(msx("f(a: int, b: int) returns q: int, r: int q, r = divmod(a, b);"),
            "(function f (params a:int b:int) (returns q:int r:int) (= (q r) (call divmod a b)))");
  EXPECT_EQ(msx("f(y: int) returns x: int x = g(y);"), "(function f (params y:int) (returns x:int) (= x (call g y)))");
}
TEST(Stmt, DanglingElseBindsInner) {
  EXPECT_EQ(msx("f(a: int, b: int) returns x: int if (a > 0) if (b > 0) x = 1; else x = 2;"),
            "(function f (params a:int b:int) (returns x:int) (if (> a 0) (if (> b 0) (= x 1) (= x 2))))");
}
TEST(Stmt, WhileAssertAssume) {
  EXPECT_EQ(msx("f(n: int) returns i: int { i = 0; while (i < n) invariant i <= n { i = i + 1; } assert i == n; assume true; }"),
            "(function f (params n:int) (returns i:int) (block (= i 0) (while (< i n) (inv (<= i n)) (block (= i (+ i 1)))) "
            "(assert (== i n)) (assume true)))");
}
TEST(Module, TreeFormat) {
  auto r = parse("f(a: int[]) returns r: int\n  r = a[0];");
  EXPECT_EQ(print_tree(r.module),
            "Module\n  Function f @1:1\n    params\n      a: int[] @1:3\n    returns\n      r: int @1:21\n"
            "    body\n      Assign r @2:3\n        ArrayAccess a @2:7\n          IntLit 0 @2:9\n");
}
TEST(Module, EmptyInputIsError) {
  auto r = parse("");
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]),
            "t:1:1: ошибка: ожидается объявление функции или формулы, найден конец файла");
}
TEST(Stmt, DeepStatementNesting) {   // Review Focus 1
  std::string src = "f() returns r: int " + std::string(5000, '{') + std::string(5000, '}');
  auto r = parse(src);
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "слишком глубокая вложенность (больше 1000 уровней)");
}
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run: `cmake --build build 2>&1 | tail -3`
Expected: compilation error (`parse` is not defined).

- [ ] **Step 3: Implement `ParseStmt.cpp`, `parse()`, the printers and sexpr for statements and declarations**

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build -R funnyparse_unit --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 5: Write `grammar.peg`**

Content:
- the grammar from spec §5, verbatim, with a `# funny.ru.md: <section>` comment on each rule (Функции и типы, Определения формул, Операторы, Выражения, Условия, Предикаты, Кванторы, Ссылки на формулы);
- a header: terminals are lexer tokens, the notation is PEG (`/` is ordered choice, `&` is lookahead);
- the precedence table from spec §5;
- a note that the parser chooses some alternatives by peek. This is equivalent to the ordered choice because those alternatives diverge on the second token.

- [ ] **Step 6: Commit**

```bash
git add ParserLIB grammar.peg
git commit -m "lab2: statements, functions, formulas, module; final grammar.peg"
```

---

### Task 5: JSON printer and `docs/ast.md`

**Files:**
- Create: `ParserLIB/src/AstJson.cpp`
- Modify: `ParserLIB/Include/AstPrinter.h`
- Create: `docs/ast.md`
- Test: `ParserLIB/test/JsonTest.cpp`

**Interfaces:**
- Consumes: `Ast.h`, `parse`, `parse_expression` (Tasks 2–4).
- Produces: `std::string print_json(const Module& m); std::string print_json(const Expr& e);`

JSON format:
- Pretty-printed with 2-space indentation; `"key": value`; an empty array is `[]`; the output ends with `\n`.
- Every object starts with `"kind"`, then `"pos": {"line": L, "col": C, "offset": O}` (`Module` has no `pos`), then the fields below in the order given. Absent optional fields are `null`.

Fields by node:
- `Module`: `decls`.
- `Function`: `name`, `params`, `requires`, `returns`, `ensures`, `uses`, `body`. Its `pos` is the name's position.
- `Formula`: `name`, `params`, `body`.
- `VarDef`: `name`, `type` (`"int"`/`"int[]"`).
- `LocalDef`: `name`, `type` (`null` when omitted).
- `Block`: `body`.
- `If`: `cond`, `then`, `else`.
- `While`: `cond`, `invariant`, `body`.
- `Assert` / `Assume`: `pred`.
- `Assign`: `target` (`{"name", "pos"}`), `value`, `sugar` (`null` or `"array_update"`).
- `TupleAssign`: `targets` (`[{"name", "pos"}]`), `call`.
- `IntLit`: `value` (string, the lexeme).
- `Var`: `name`.
- `ArrayAccess`: `array`, `index`.
- `Call`: `callee`, `args`.
- `Neg`: `operand`.
- `Binary`: `op`, `lhs`, `rhs`.
- `BoolLit`: `value` (bool).
- `Compare`: `op`, `lhs`, `rhs`.
- `Not`: `operand`.
- `And` / `Or` / `Implies`: `lhs`, `rhs`.
- `Quantifier`: `quantifier` (`"forall"`/`"exists"`), `var` (VarDef), `body`.
- `FormulaRef`: `name`, `args`.

Strings are escaped (`\"`, `\\`, control characters as `\u00XX`).

- [ ] **Step 1: Write the failing tests in `JsonTest.cpp`**

```cpp
TEST(Json, Expression) {
  auto r = parse_expression("-x");
  EXPECT_EQ(print_json(*r.node),
            "{\n  \"kind\": \"Neg\",\n  \"pos\": {\"line\": 1, \"col\": 1, \"offset\": 0},\n  \"operand\": {\n"
            "    \"kind\": \"Var\",\n    \"pos\": {\"line\": 1, \"col\": 2, \"offset\": 1},\n    \"name\": \"x\"\n  }\n}\n");
}
TEST(Json, ModuleShape) {
  auto r = parse("f() returns r: int r = 1;");
  const std::string j = print_json(r.module);
  EXPECT_NE(j.find("\"kind\": \"Function\""), std::string::npos);
  EXPECT_NE(j.find("\"requires\": null"), std::string::npos);
  EXPECT_NE(j.find("\"params\": []"), std::string::npos);
  EXPECT_NE(j.find("\"sugar\": null"), std::string::npos);
  EXPECT_NE(j.find("\"value\": \"1\""), std::string::npos);
}
TEST(Json, SugarMarked) {
  auto r = parse("f(v: int) returns a: int[] a[0] = v;");
  EXPECT_NE(print_json(r.module).find("\"sugar\": \"array_update\""), std::string::npos);
}
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run: `cmake --build build 2>&1 | tail -3`
Expected: compilation error (`print_json` is not defined).

- [ ] **Step 3: Implement `AstJson.cpp`** with a small `JsonWriter` (indentation stack) and `std::visit` over the nodes.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build -R funnyparse_unit --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 5: Write `docs/ast.md`**

Contents:
- the node kinds and fields (the table above), plus `Pos`;
- how to call the API from C++ (`parse`, `ParseResult`, `print_tree`, `print_json`);
- a full example: the JSON for `f(a: int[]) returns r: int r = a[0];`, generated by the code and pasted in;
- the null and sugar conventions;
- a note that `requires`/`ensures`/`invariant` defaults are not filled in by the parser.

- [ ] **Step 6: Commit**

```bash
git add ParserLIB docs/ast.md
git commit -m "lab2: JSON AST output and AST format documentation"
```

---

### Task 6: Snapshot runner, full CLI, positive corpus

**Files:**
- Create: `ParserLIB/Include/Snapshot.h`, `ParserLIB/src/Snapshot.cpp`
- Modify: `ParserMain/funnyparse.cpp`, `ParserLIB/test/CMakeLists.txt` (ctest entries), `ParserMain/CMakeLists.txt`
- Create: `tests/positive/*.funny`, `*.ast`, and `01_sum.json` / `05_formulas.json`
- Test: `ParserLIB/test/SnapshotTest.cpp`

**Interfaces:**
- Consumes: `parse`, `print_tree(Module)`, `print_json(Module)`, `format_diagnostic`, `lex` (Tasks 1–5).
- Produces:

```cpp
namespace funny {
std::string render_diagnostics(std::string_view label, const std::vector<Diagnostic>& ds);  // lines with "\n" each
struct SnapshotResult { std::filesystem::path source; std::string kind; bool passed; std::string expected; std::string actual; };
// tests_dir/positive/X.funny → X.ast (required), X.json (optional); tests_dir/negative/X.funny → X.err (required).
// bless == true: writes actual into the expected files (creates them) and marks everything passed.
std::vector<SnapshotResult> run_snapshots(const std::filesystem::path& tests_dir, bool bless);
}
```

- Inside snapshots the label is the file name without directories (`03_unclosed_paren.funny`). A positive case that has errors gives actual = its diagnostics. A negative case with no errors gives actual = `(нет ошибок)\n`. Files are read in binary mode. Cases are processed sorted by name.
- CLI (`kUsage` in Lab1 style):
  - `funnyparse [--json] [FILE]`: reads FILE or stdin. AST to stdout only if there are no diagnostics; diagnostics to stderr with label = FILE as typed, or `<stdin>`.
  - `funnyparse tokens [FILE]`
  - `funnyparse test [--bless] [DIR]`: DIR defaults to `tests`. Prints `PASS|FAIL  <file> (<kind>)` per case; on FAIL, prints the expected and actual text after it. The last line is `N/M passed`.

  Exit codes as in the Global Constraints. An unreadable file prints `не удалось открыть '<path>'` and exits 2.
- ctest:
  - `funnyparse_cases`: `COMMAND funnyparse_cli test`, `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}`.
  - `funnyparse_missing_file`: `COMMAND funnyparse_cli no_such_file.funny`, `PASS_REGULAR_EXPRESSION "не удалось открыть"`.
- Positive corpus. Each file covers its own constructs; together they cover every rule in `grammar.peg`. Comments are ASCII.
  - `01_sum.funny`: sum of an array, `while` + `invariant`, `uses`, `requires`/`ensures`, `length`.
  - `02_max.funny`: `if`/`else`, nested blocks.
  - `03_divmod_tuple.funny`: a function with two returns and a tuple assignment.
  - `04_array_update.funny`: `a[i] = v`, copy `b = a`.
  - `05_formulas.funny`: formulas, `forall`/`exists`, formula references in `ensures` and `invariant`.
  - `06_assert_assume.funny`
  - `07_precedence.funny`: unary minus, `* /` against `+ -`, `not`/`and`/`or`/`->`.
  - `08_comments_layout.funny`: comments, blank lines, tabs.
  - `09_minimal.funny`: `f() returns r: int r = 0;`, a body without a block.
  - `10_uses_untyped.funny`: `uses` with and without types, an empty block `{ }`.

- [ ] **Step 1: Write the failing test `SnapshotTest.cpp`**

```cpp
TEST(Snapshots, AllCasesPass) {
  auto results = funny::run_snapshots("tests", /*bless=*/false);
  ASSERT_GE(results.size(), 10u);
  for (const auto& r : results)
    EXPECT_TRUE(r.passed) << r.source << " (" << r.kind << ")\nexpected:\n" << r.expected << "\nactual:\n" << r.actual;
}
TEST(Snapshots, RenderDiagnostics) {
  EXPECT_EQ(funny::render_diagnostics("a.funny", {{funny::Pos{2, 3, 7}, "m"}}), "a.funny:2:3: ошибка: m\n");
}
```

- [ ] **Step 2: Run the test and confirm it fails**

Run: `cmake --build build 2>&1 | tail -3`
Expected: compilation error (`run_snapshots` is not defined).

- [ ] **Step 3: Implement `Snapshot.cpp` and the full CLI**

- [ ] **Step 4: Write the 10 positive programs; generate the snapshots with `--bless`; check each `.ast` by hand**

Run: `cmake --build build && ./build/ParserMain/funnyparse test --bless && touch tests/positive/01_sum.json tests/positive/05_formulas.json && ./build/ParserMain/funnyparse test --bless`

(bless creates `.json` files only when the file already exists, which is why the `touch`.)

Then read every `tests/positive/*.ast` and check by hand:
- the tree structure matches the source program;
- precedence matches spec §5;
- positions match the start of the corresponding token;
- `a[i] = v` is shown as `Assign a [sugar: array_update]` with `Call set`.

Fix the code (not the snapshot) on any mismatch.

- [ ] **Step 5: Run all tests and confirm they pass**

Run: `ctest --test-dir build --output-on-failure`
Expected: `100% tests passed` (funnyparse_unit, funnyparse_cases, funnyparse_missing_file).

- [ ] **Step 6: Commit**

```bash
git add ParserLIB ParserMain tests/positive
git commit -m "lab2: snapshot runner, funnyparse CLI, positive test corpus"
```

---

### Task 7: Error recovery and negative corpus

**Files:**
- Modify: `ParserLIB/src/ParseStmt.cpp` (the block loop and module loop with recovery; the unclosed-`}` message)
- Create: `tests/negative/*.funny` + `*.err` (25 cases listed below)
- Test: the negative corpus via `SnapshotTest.AllCasesPass` and `funnyparse_cases`

**Interfaces:**
- Consumes: `ParserImpl::{mark, reset, peek, farthest_diagnostic, clear_failures}`, `statement()`, `decl()` (Tasks 2–4); `run_snapshots` (Task 6).
- Produces: `parse()` with several diagnostics. The signature does not change.

Recovery algorithm:

```
block():  expect('{') else fail; open = position of '{'
  loop:
    if at('}'): advance; return Block            # silent check
    if at(kEof): out += "ожидается '}' (для '{' в L:C), найден конец файла" at EOF; return Block
    start = mark()
    if s = statement(): append; continue
    if d = farthest_diagnostic(): out += d
    clear_failures(); reset(start); skip_statement()

skip_statement(): depth = 0
  loop: t = peek()
    kEof → stop
    '{' → depth++, advance
    '}' → if depth == 0: stop (do not consume); else depth--, advance
    ';' → advance; if depth == 0: stop
    otherwise advance
  (the statement starts with neither '}' nor kEof, so at least one token is consumed)

module(): loop until kEof:
  start = mark()
  if d = decl(): append; continue
  if diag = farthest_diagnostic(): out += diag
  clear_failures(); reset(start); advance()        # >= 1 token
  skip until kEof, or a token at brace depth 0 and paren depth 0 such that
    peek is IDENT, peek(1) is '(', and (the previous token is '}' or ';', or IDENT starts a line)
```

The block diagnostics accumulate in the same `out` list that `module()` gets. If `module()` runs without a single declaration (empty input), the result is one diagnostic as in Task 4.

Negative corpus: `.funny` files and the exact content of `.err`. Each `.err` line has the form `<file>:<L>:<C>: ошибка: <msg>`, and the file ends with `\n`. Sources are listed exactly, with `\n` after the last line unless stated otherwise.

| # / file | Source | `.err` (without the `<file>:` prefix) |
|---|---|---|
| `01_empty` | (empty file) | `1:1: ожидается объявление функции или формулы, найден конец файла` |
| `02_comment_only` | `// nothing here` | `2:1: ожидается объявление функции или формулы, найден конец файла` |
| `03_unclosed_paren` | `f(x: int) returns r: int {` / `  if (x > 0 { r = 1; }` / `}` | `2:13: ожидается ')', найдено '{'` |
| `04_unclosed_brace` | `f(x: int) returns r: int {` / `  r = 1;` | `3:1: ожидается '}' (для '{' в 1:26), найден конец файла` |
| `05_unclosed_bracket` | `f(a: int[], i: int) returns y: int {` / `  y = a[i;` / `}` | `2:10: ожидается ']', найдено ';'` |
| `06_extra_token_in_expr` | `f() returns x: int {` / `  x = 1 2;` / `}` | `2:9: ожидается ';', найдено '2'` |
| `07_extra_token_after_decl` | `f() returns r: int r = 1;` / `)` | `2:1: ожидается объявление функции или формулы, найдено ')'` |
| `08_assign_colon_eq` | `f() returns x: int {` / `  x := 1;` / `}` | `2:5: ожидается '=', найдено ':'` |
| `09_assign_in_condition` | `f(x: int) returns y: int {` / `  if (x = 1) y = 2;` / `}` | `2:9: ожидается '==', '!=', '>=', '<=', '>' или '<', найдено '='` |
| `10_eq_as_statement` | `f(x: int) returns y: int {` / `  y == 1;` / `}` | `2:5: ожидается '=', найдено '=='` |
| `11_bad_int` | `f() returns x: int {` / `  x = 007;` / `}` | `2:7: некорректное число '007': ведущие нули не допускаются` |
| `12_bad_char` | `f() returns x: int {` / `  x = @;` / `}` | `2:7: недопустимый символ '@'` |
| `13_non_ascii_comment` | `f() returns x: int {` / `  x = 1; // привет` / `}` | `2:13: недопустимые символы в комментарии (разрешён только ASCII)` |
| `14_missing_returns` | `f(x: int) {` / `  x = 1;` / `}` | `1:11: ожидается 'requires' или 'returns', найдено '{'` |
| `15_param_without_type` | `f(x) returns r: int {` / `  r = 1;` / `}` | `1:4: ожидается ':', найдено ')'` |
| `16_unclosed_array_type` | `f(a: int[) returns r: int {` / `  r = 1;` / `}` | `1:10: ожидается ']', найдено ')'` |
| `17_missing_semicolon_assign` | `f() returns x: int {` / `  x = 1` / `  x = 2;` / `}` | `3:3: ожидается ';', найдено 'x'` |
| `18_missing_semicolon_assert` | `f(x: int) returns r: int {` / `  assert x > 0` / `  r = x;` / `}` | `3:3: ожидается ';', найдено 'r'` |
| `19_missing_semicolon_assume` | `f(x: int) returns r: int {` / `  r = x;` / `  assume r > 0` / `}` | `4:1: ожидается ';', найдено '}'` |
| `20_else_without_if` | `f(x: int) returns r: int {` / `  else r = 1;` / `}` | `2:3: ожидается оператор, найдено 'else'` |
| `21_quantifier_in_condition` | `f(x: int) returns r: int {` / `  if (forall(i: int \| i > 0)) r = 1;` / `}` | `2:7: ожидается условие, найдено 'forall'` |
| `22_implication_in_requires` | `f(x: int) requires x > 0 -> x > 1 returns r: int {` / `  r = x;` / `}` | `1:26: ожидается 'returns', найдено '->'` |
| `23_function_keyword` | `function f() returns r: int {` / `  r = 1;` / `}` | `1:1: ожидается объявление функции или формулы, найдено 'function'` |
| `24_three_errors` | `f(x: int) returns r: int {` / `  r = x +;` / `}` / `g(x: int) returns r: int {` / `  r = (x;` / `}` / `h(x) returns r: int {` / `  r = 1;` / `}` | `2:10: ожидается выражение, найдено ';'` / `5:9: ожидается ')', найдено ';'` / `7:4: ожидается ':', найдено ')'` |
| `25_keyword_as_param` (Review Focus 4) | `f(int: int) returns r: int r = 1;` | `1:3: ожидается идентификатор или ')', найдено 'int'` |

(`/` in the table separates lines; `\|` is a literal `|`.)

- [ ] **Step 1: Create the 25 files in `tests/negative/` with the sources and `.err` files from the table**

- [ ] **Step 2: Run the corpus and confirm the multi-error cases fail**

Run: `cmake --build build && ./build/ParserMain/funnyparse test`
Expected: FAIL at least on `03`, `04`, `24` (no recovery yet; the unclosed-`}` message is not yet the special one).

- [ ] **Step 3: Implement recovery in `block()`/`module()` and the unclosed-`}` message using the algorithm above**

- [ ] **Step 4: Run all tests and confirm they pass**

Run: `ctest --test-dir build --output-on-failure`
Expected: `100% tests passed`. If a case fails because of message wording, the table is the source of truth, not the code's current output. Wording may change only by agreement, and then this table and the spec examples change too.

- [ ] **Step 5: Commit**

```bash
git add ParserLIB tests/negative
git commit -m "lab2: panic-mode recovery at statement and declaration level; negative corpus"
```

---

### Task 8: Termination guarantee and README

**Files:**
- Create: `ParserLIB/test/FuzzTest.cpp`
- Modify: `ParserLIB/test/CMakeLists.txt` (add `FuzzTest.cpp`; `set_tests_properties(funnyparse_unit PROPERTIES TIMEOUT 120)`)
- Create: `README.md` (in `Lab2/`; the root `.gitignore` ignores it and stays untouched, so it is **not** committed)

**Interfaces:**
- Consumes: `parse`, `lex` (Tasks 1–7), the corpus `tests/positive/*.funny`.

- [ ] **Step 1: Write `FuzzTest.cpp`**

```cpp
// Invariant for every input: parse() returns; every diagnostic has line >= 1, col >= 1, offset <= src.size();
// diagnostics are sorted by offset; with !ok(), no more than one diagnostic per token, plus one.
void check_invariants(std::string_view src);

TEST(Fuzz, EveryTokenPrefixOfPositiveCorpus) {
  // for each tests/positive/*.funny: for each token t in lex(src).tokens → check_invariants(src.substr(0, t.pos.offset))
}
TEST(Fuzz, RandomTokenSequences) {
  // std::mt19937 rng(12345); 500 sequences of length 1..60 from the lexeme pool
  // {"f","x","a","(",")","{","}","[","]",";",",",":","=","==","<","+","-","*","if","else","while","returns",
  //  "requires","ensures","uses","invariant","assert","assume","forall","|","=>","->","and","or","not","int",
  //  "true","1","007","@"}, joined by a space → check_invariants
}
TEST(Fuzz, FinishesQuickly) {
  // the two loops above in one run under a std::chrono timer: EXPECT_LT(elapsed, 10s)
}
```

- [ ] **Step 2: Run the tests**

Run: `cmake --build build && ctest --test-dir build -R funnyparse_unit --output-on-failure`
Expected: `100% tests passed`. A hang or crash is a bug in recovery or in the "≥ 1 token" guarantee. Fix it in the code (superpowers:systematic-debugging).

- [ ] **Step 3: Write `README.md`**

Sections:
1. What it is (HW2, PEG over the HW1 lexer).
2. Build and tests (the command from the Global Constraints; GTest via the system package or `conan install`).
3. Running `funnyparse` with an example: a short program, its tree output, the `--json` output, and the output of one error example. Paste real output.
4. Why PEG and not LL(1)/LR (spec §1).
5. Operator precedence and associativity (table from spec §5).
6. Error handling: farthest failure, the message template, silent continuations, recovery points, the depth limit.
7. Deviations from `funny.ru.md` and the assignment (spec §10, items 1–9).
8. Tests: corpus layout, the `.ast`/`.json`/`.err` format, `funnyparse test --bless`, the fuzz tests.
9. Project layout and links to `grammar.peg`, `docs/ast.md`, `docs/funny.ru.md`.

- [ ] **Step 4: Final check**

Run: `rm -rf build && cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed`, with no clang-tidy warnings in the new code.

- [ ] **Step 5: Commit**

```bash
git add ParserLIB
git commit -m "lab2: fuzz/termination tests"
```
