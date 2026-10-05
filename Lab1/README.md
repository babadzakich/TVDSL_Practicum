# HW1. Лексер Funny: регулярные выражения → НКА → ДКА → минимальный ДКА

Генератор лексера для языка Funny на C++23:

1. список токенов (имя + регулярное выражение) вшит в код — `RegexLIB/src/TokenSpec.cpp`;
2. разбор регулярных выражений в AST (`Regex.cpp`);
3. НКА по Томпсону (`Nfa.cpp`);
4. ДКА построением подмножеств, ловушка = пустое множество состояний НКА (`Dfa.cpp`);
5. минимизация Хопкрофтом (`Dfa.cpp`);
6. таблица переходов + табличный лексер с максимальным совпадением (`Lexer.cpp`);
7. экспорт таблицы в JSON и C++-заголовок (`Export.cpp`).

## Сборка и тесты

Нужны CMake ≥ 3.25, компилятор с C++23 и GoogleTest.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Запуск (из корня проекта)

```sh
./build/RegexMain/funnylex build            # размеры автоматов + artifacts/dfa.json, artifacts/funny_lexer_table.h
./build/RegexMain/funnylex test             # прогон tests/cases.txt, статус каждого теста
./build/RegexMain/funnylex lex file.funny   # разбить файл (или stdin) на токены
./build/RegexMain/funnylex match 00 while ! @   # строка целиком через ДКА: ТОКЕН / REJECT / TRAP
```

Чтобы поменять токены, правь список в `RegexLIB/src/TokenSpec.cpp` (порядок = приоритет,
флаги `skip` — WS/COMMENT, `error` — BAD_INT) и заново запусти `funnylex build`.

Синтаксис регулярок: `a|b`, `ab`, `a*`, `a+`, `a?`, `(a)`, `.`, `[a-z]`, `[^\n]`,
экранирование `\n \t \r \xHH \d \w \s \<пунктуация>`.

## Формат тестов (`tests/cases.txt`)

```
match | "строка" | ОЖИДАНИЕ        # вся строка через ДКА: ТОКЕН | REJECT | TRAP
lex   | "строка" | T1 T2="лексема"  # разбиение на токены (skip-токены не выдаются)
```

В строках работает экранирование `\n \t \r \\ \" \xHH`, поэтому CRLF, табы и не-ASCII
байты записываются явно. `ERROR` — символ, попавший в ловушку.

## Подключение в лексер проекта (T0)

`artifacts/funny_lexer_table.h` не зависит от этой библиотеки (только `<cstddef>`,
`<cstdint>`, `<string_view>`), всё в нём `constexpr`:

```cpp
#include "funny_lexer_table.h"
using namespace funny_lexer;

for (std::size_t pos = 0; pos < src.size();) {
  const Match m = next_token(src, pos);   // TokenKind + длина, максимальное совпадение
  if (is_error(m.kind)) { /* ERROR или BAD_INT */ }
  else if (!is_skip(m.kind)) { /* emit(m.kind, src.substr(pos, m.length)) */ }
  pos += m.length;
}
```

Тест `GeneratedTableTest` сверяет этот файл с библиотечным лексером. Если поменяли токены и
забыли перезапустить `funnylex build`, тест упадёт. Альтернатива без C++ — `artifacts/dfa.json`:
`byte_to_class[256]`, `states[i].next[класс]`, `states[i].accept`, `start`, `trap`.

## Структура

```
RegexLIB/Include/, RegexLIB/src/   библиотека: Regex, Nfa, Dfa, TokenSpec, Lexer, Export, TestCases
RegexLIB/test/                     gtest
RegexMain/funnylex.cpp             CLI
tests/cases.txt                    табличные тесты
artifacts/                         dfa.json, funny_lexer_table.h, test_results.txt
REPORT.md                          отчёт
```
