# Формат AST (HW2)

AST — результат `funny::parse`. Его можно напечатать деревом (`print_tree`, по умолчанию в `funnyparse`) или в JSON
(`print_json`, флаг `funnyparse --json`). Этот документ описывает JSON и C++-API, на которые опираются следующие этапы
(T0/T1).

## C++-API

```cpp
#include "Parser.h"      // funny::parse, funny::ParseResult
#include "AstPrinter.h"  // funny::print_tree, funny::print_json

funny::ParseResult r = funny::parse(source);   // source — std::string_view
if (!r.ok()) {
  for (const funny::Diagnostic& d : r.diagnostics)
    std::cerr << funny::format_diagnostic("file.funny", d) << '\n';
} else {
  std::cout << funny::print_json(r.module);
}
```

Узлы описаны в `ParserLIB/Include/Ast.h`: это `struct` с `std::variant` внутри (`Expr::node`, `Cond::node`,
`Stmt::node`, `Decl`). Обход — `std::visit`. Дети хранятся в `std::unique_ptr`. При ошибках `r.module` содержит то, что
удалось разобрать, но печатать и использовать его дальше не следует.

Условия (`if`/`while`) и предикаты (`requires`/`ensures`/`invariant`/`assert`/`assume`/формулы) используют общий тип
узла `Cond`; грамматика гарантирует, что в условиях нет `Quantifier`/`FormulaRef`, а в предикатах — `Implies`.

## Общие правила JSON

- Каждый объект-узел начинается с `"kind"`, затем `"pos"`, затем поля узла в порядке из таблицы.
- `"pos": {"line": L, "col": C, "offset": O}` — позиция первого токена узла: строка и столбец с 1 (столбец — номер
  байта в строке; алфавит Funny — ASCII), `offset` — байтовое смещение с 0. У `Module` позиции нет.
- Отсутствующие необязательные части — `null`; пустые списки — `[]`.
- Отступ — 2 пробела, вывод заканчивается переводом строки.

## Узлы

| kind | Поля (после kind, pos) | Примечание |
|---|---|---|
| `Module` | `decls` | без `pos` |
| `Function` | `name`, `params`, `requires`, `returns`, `ensures`, `uses`, `body` | `pos` — позиция имени; `requires`/`ensures` — `null`, если опущены |
| `Formula` | `name`, `params`, `body` | `body` — предикат |
| `VarDef` | `name`, `type` | `type`: `"int"` / `"int[]"` |
| `LocalDef` | `name`, `type` | `type` — `null`, если тип не указан |
| `Block` | `body` | список операторов |
| `If` | `cond`, `then`, `else` | `else` — `null`, если нет |
| `While` | `cond`, `invariant`, `body` | `invariant` — `null`, если опущен |
| `Assert`, `Assume` | `pred` | |
| `Assign` | `target`, `value`, `sugar` | `target` = `{"name", "pos"}`; `sugar`: `null` или `"array_update"` |
| `TupleAssign` | `targets`, `call` | `targets` — список `{"name", "pos"}`; `call` — узел `Call` |
| `IntLit` | `value` | строка — лексема как в исходнике (без проверки диапазона) |
| `Var` | `name` | |
| `ArrayAccess` | `array`, `index` | `array` — имя массива |
| `Call` | `callee`, `args` | в т. ч. `length(a)` и `set(...)` из сахара |
| `Neg` | `operand` | унарный минус |
| `Binary` | `op`, `lhs`, `rhs` | `op`: `"+"`, `"-"`, `"*"`, `"/"` |
| `BoolLit` | `value` | `true` / `false` (JSON-булево) |
| `Compare` | `op`, `lhs`, `rhs` | `op`: `"=="`, `"!="`, `">="`, `"<="`, `">"`, `"<"` |
| `Not` | `operand` | |
| `And`, `Or`, `Implies` | `lhs`, `rhs` | цепочки `and`/`or` — левые, `->` — правая |
| `Quantifier` | `quantifier`, `var`, `body` | `quantifier`: `"forall"` / `"exists"`; `var` — `VarDef` |
| `FormulaRef` | `name`, `args` | ссылка на формулу в предикате |

## Соглашения

- **Сахар обновления массива.** `a[i] = v;` хранится как `Assign` с `target = a`, `value = Call set(a, i, v)` и
  `"sugar": "array_update"`. Позиции `Call` и внутреннего `Var a` — позиция `a` в исходнике.
- **Умолчания не подставляются.** Опущенные `requires` (по смыслу `true`), `ensures` (`false`) и `invariant` (`true`)
  в AST — `null`; подставлять умолчания — задача следующего этапа.
- **Скобки** в AST не представлены: `(e)` — это узел `e` с его собственной позицией.
- `x = f(y);` — обычный `Assign` с `Call` в `value`; `TupleAssign` — только при двух и более целях.

## Пример

`f(a: int[]) returns r: int r = a[0];`

```json
{
  "kind": "Module",
  "decls": [
    {
      "kind": "Function",
      "pos": {"line": 1, "col": 1, "offset": 0},
      "name": "f",
      "params": [
        {
          "kind": "VarDef",
          "pos": {"line": 1, "col": 3, "offset": 2},
          "name": "a",
          "type": "int[]"
        }
      ],
      "requires": null,
      "returns": [
        {
          "kind": "VarDef",
          "pos": {"line": 1, "col": 21, "offset": 20},
          "name": "r",
          "type": "int"
        }
      ],
      "ensures": null,
      "uses": [],
      "body": {
        "kind": "Assign",
        "pos": {"line": 1, "col": 28, "offset": 27},
        "target": {
          "name": "r",
          "pos": {"line": 1, "col": 28, "offset": 27}
        },
        "value": {
          "kind": "ArrayAccess",
          "pos": {"line": 1, "col": 32, "offset": 31},
          "array": "a",
          "index": {
            "kind": "IntLit",
            "pos": {"line": 1, "col": 34, "offset": 33},
            "value": "0"
          }
        },
        "sugar": null
      }
    }
  ]
}
```
