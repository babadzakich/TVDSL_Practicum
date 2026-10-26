# Формат аннотированного AST (HW3)

Результат `funny::analyze` (или `funny::parse` + `funny::check`) — AST из HW2, в котором семантический анализ
заполнил поля-аннотации. AST печатается деревом (`print_tree`, по умолчанию в `funnycheck`) или в JSON
(`print_json`, флаг `funnycheck --json`). Этот документ — контракт для следующих этапов (HW4 / проект: IR, оптимизации).

## C++-API

```cpp
#include "Sema.h"        // funny::analyze, funny::check
#include "AstPrinter.h"  // funny::print_tree, funny::print_json
#include "Symbols.h"     // funny::print_symbols, funny::GlobalScope

funny::AnalyzeResult r = funny::analyze(source);   // source — std::string_view
if (!r.ok()) {
  for (const funny::Diagnostic& d : r.diagnostics)
    std::cerr << funny::format_diagnostic("file.funny", d) << '\n';
} else {
  std::cout << funny::print_json(r.module);
}
```

- `r.parsed == false` — были синтаксические ошибки; в `r.diagnostics` только они, семантика не проверялась.
- `r.ok()` — ни синтаксических, ни семантических ошибок. Только в этом случае аннотации полные и AST можно
  передавать дальше. При семантических ошибках у части выражений тип остаётся пустым.
- `funny::check(Module&)` можно вызвать на результате `funny::parse` без ошибок; он заполняет аннотации на месте и
  возвращает список ошибок, отсортированный по позиции.

## Аннотации (поля с пометкой «HW3» в `ParserLIB/Include/Ast.h`)

| Поле C++ | Где | Значение |
|---|---|---|
| `ExprType Expr::type` | каждое выражение | `std::vector<TypeKind>`: один элемент — `int`/`int[]`; несколько — кортеж (только `Call` в `TupleAssign`); пусто — тип не вычислен |
| `VarKind VarRef::kind` | `Var` | `Param`, `Return`, `Local`, `Bound` (переменная квантора); `Unresolved` — до проверки или при ошибке |
| `std::optional<TypeKind> LocalDef::type` | `uses` | после проверки заполнен у всех локальных: указанный в исходнике или выведенный |
| `bool LocalDef::inferred` | `uses` | `true`, если тип выведен |

Узлы условий и предикатов (`Cond`: `Compare`, `And`, `Not`, `Quantifier`, `FormulaRef`, ...) отдельного поля типа
не имеют: все они имеют тип `bool`, и другого в Funny не бывает.

## Дерево (`print_tree`)

Формат HW2 (`<Kind> [детали] @L:C`, отступ 2 пробела) с добавками:

- у выражения после деталей — ` : <тип>`: `Binary + : int @3:10`, `Call divmod : (int, int) @8:10`;
- у переменной — вид символа в скобках: `Var a [param] : int[] @2:7`;
- у локальной с выведенным типом — `(inferred)`: `t: int[] (inferred) @1:45`.

До проверки (вывод `funnyparse` из HW2) этих добавок нет, и дерево совпадает с HW2 байт в байт.

## Общие правила JSON

- Каждый объект-узел начинается с `"kind"`, затем `"pos"`, затем поля узла в порядке из таблицы.
- `"pos": {"line": L, "col": C, "offset": O}` — позиция первого токена узла: строка и столбец с 1 (столбец — номер
  байта в строке; алфавит Funny — ASCII), `offset` — байтовое смещение с 0. У `Module` позиции нет.
- Отсутствующие необязательные части — `null`; пустые списки — `[]`.
- **У каждого узла-выражения последнее поле — `"type"`**: `"int"`, `"int[]"`, массив строк для кортежа
  (`["int", "int"]`) или `null`, если тип не вычислен.
- Отступ — 2 пробела, вывод заканчивается переводом строки.

## Узлы

| kind | Поля (после kind, pos) | Примечание |
|---|---|---|
| `Module` | `decls` | без `pos` |
| `Function` | `name`, `params`, `requires`, `returns`, `ensures`, `uses`, `body` | `pos` — позиция имени; `requires`/`ensures` — `null`, если опущены |
| `Formula` | `name`, `params`, `body` | `body` — предикат |
| `VarDef` | `name`, `type` | `type`: `"int"` / `"int[]"` |
| `LocalDef` | `name`, `type`, `inferred` | после проверки `type` заполнен всегда; `inferred: true` — тип выведен |
| `Block` | `body` | список операторов |
| `If` | `cond`, `then`, `else` | `else` — `null`, если нет |
| `While` | `cond`, `invariant`, `body` | `invariant` — `null`, если опущен |
| `Assert`, `Assume` | `pred` | |
| `Assign` | `target`, `value`, `sugar` | `target` = `{"name", "pos"}`; `sugar`: `null` или `"array_update"` |
| `TupleAssign` | `targets`, `call` | `targets` — список `{"name", "pos"}`; `call` — узел `Call` с типом-кортежем |
| `IntLit` | `value`, `type` | строка — лексема как в исходнике |
| `Var` | `name`, `symbol`, `type` | `symbol`: `"param"`, `"return"`, `"local"`, `"bound"` |
| `ArrayAccess` | `array`, `index`, `type` | `array` — имя массива; `type` всегда `"int"` |
| `Call` | `callee`, `args`, `type` | в т. ч. `length(a)` и `set(...)` из сахара |
| `Neg` | `operand`, `type` | унарный минус |
| `Binary` | `op`, `lhs`, `rhs`, `type` | `op`: `"+"`, `"-"`, `"*"`, `"/"` |
| `BoolLit` | `value` | `true` / `false` (JSON-булево) |
| `Compare` | `op`, `lhs`, `rhs` | `op`: `"=="`, `"!="`, `">="`, `"<="`, `">"`, `"<"` |
| `Not` | `operand` | |
| `And`, `Or`, `Implies` | `lhs`, `rhs` | цепочки `and`/`or` — левые, `->` — правая |
| `Quantifier` | `quantifier`, `var`, `body` | `quantifier`: `"forall"` / `"exists"`; `var` — `VarDef` |
| `FormulaRef` | `name`, `args` | ссылка на формулу в предикате |

## Соглашения (как в HW2)

- **Сахар обновления массива.** `a[i] = v;` хранится как `Assign` с `target = a`, `value = Call set(a, i, v)` и
  `"sugar": "array_update"`. Тип `set` — `(int[], int, int) -> int[]`; у `Call set` тип `"int[]"`. Позиции `Call` и
  внутреннего `Var a` — позиция `a` в исходнике. `set` не входит в таблицу символов: пользовательская функция с
  именем `set` не мешает сахару (сахар узнаётся по `sugar`, а не по имени).
- **Умолчания не подставляются.** Опущенные `requires` (по смыслу `true`), `ensures` (`false`) и `invariant` (`true`)
  — `null`.
- **Скобки** в AST не представлены: `(e)` — это узел `e` с его собственной позицией.
- `x = f(y);` — обычный `Assign`; `TupleAssign` — только при двух и более целях.

## Пример

`f(a: int[]) returns r: int r = a[0];` — `funnycheck --json`:

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
            "value": "0",
            "type": "int"
          },
          "type": "int"
        },
        "sugar": null
      }
    }
  ]
}
```
