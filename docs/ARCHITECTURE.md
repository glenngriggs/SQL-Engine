# Architecture

Notes on how the engine is put together and why some of the main decisions were made.

## How a Command Runs

```
stdin -> TokenStream -> Parser -> Command -> Executor -> ResultWriter -> stdout
                          |                     |
                          v                     v
                       Catalog          Planner / Join / Aggregate / CSV loader
```

`apps/main.cpp` only parses the command line flags and hands the streams to a `Session`, which runs the read/parse/execute loop. Everything else is in the `sillyql` static library so it can be unit tested and benchmarked directly.

| Class | What it does |
|---|---|
| `Value` | One typed value, a `std::variant<int32_t, double, bool, std::string>` |
| `Column` | One column of a table stored as a typed vector |
| `Table` | Schema, one `Column` per attribute, and an optional index |
| `Index` | Abstract index interface, implemented by `HashIndex<K>` and `OrderedIndex<K>` |
| `Catalog` | All tables, looked up by name |
| `TokenStream` | Splits input into whitespace separated tokens |
| `Parser` | Turns tokens into a `Command` |
| `Planner` | Picks how to run a query (scan vs index) and produces `EXPLAIN` text |
| `Executor` | Runs a `Command` against the catalog |
| `ResultWriter` | Handles output formatting and quiet mode |
| `Session` | The main loop, error recovery, and `-t` timing |

## Parser

The parser checks table and column names as it reads, instead of leaving that to the executor. It has to, because how a value is parsed depends on the column it belongs to (`3` is an int in one column and a double in another), and `INSERT` needs to know how many columns the table has to know how many tokens make up a row. So by the time a `Command` reaches the executor, every name has already been turned into a column position and every literal into a typed `Value`.

`Command` is a `std::variant` of plain structs (`CreateCmd`, `InsertCmd`, `PrintCmd`, `JoinCmd`, `AggregateCmd`, etc.). The executor handles them with `std::visit`, so if a new command is added and not handled, it's a compile error instead of a silently missing `else if`.

Each command reads its whole fixed header (for example `FROM <table> WHERE <col> <op>`) before checking any of it. This matches how the original version read input, so error recovery skips exactly the same tokens it used to, even when a command is split across lines.

### Errors

Errors are exceptions, and `what()` is always the exact message the user should see.

```
SqlError                  print message, skip rest of the line, keep going
├── UnknownCommandError
├── TableNotFoundError
├── ColumnNotFoundError
├── TableExistsError
├── SyntaxError
├── SemanticError
└── LoadError
InputError                fatal, "Error: Reading from cin has failed", exit 1
UsageError                fatal, "Error: Unknown command line option", exit 1
```

## Column Storage

The original version stored rows as `vector<vector<Field>>`, where `Field` was a tagged union. Filtering on one column meant walking every row's separately allocated vector and checking the type of every cell.

Now each `Table` holds one `Column` per attribute, and a `Column` is a `std::variant` of `vector<int32_t>`, `vector<double>`, `vector<uint8_t>`, or `vector<std::string>`. This is the same idea as column-oriented databases and kdb+:

- A query like `WHERE price < 500` only reads the price column, which is one contiguous array. Every byte that gets pulled into cache is actually used, and the access pattern is sequential so the hardware prefetcher keeps up.
- The type is checked once per scan instead of once per cell. The scan itself is a template (`scanValues<T>`) instantiated per type and operator, so the inner loop is just a plain `double < double` comparison.
- Selection is branch-free. Every row id gets written into a buffer, and the write position only moves forward when the comparison is true. That avoids a branch misprediction on every other row when about half the rows match.
- Bools are stored as one byte each, since `vector<bool>` is a bit-packed special case that doesn't work with the same scan code.
- Row ids are 32-bit (`uint32_t`), which halves the size of index entries.

On a filter that matches about half of 1M rows, the old row layout takes 8.75 ms, the column layout with a normal branchy loop takes 3.39 ms, and the column layout with the branch-free scan takes 0.59 ms.

The downside is that rebuilding a full row touches every column, and inserting a row writes to N vectors. Neither matters much here since output is printed cell by cell anyway and inserts come in batches.

### Inserts and Deletes

`INSERT` and `LOAD` first parse everything into a `RowBatch` and only add it to the table once every value has been checked. So a bad row or a bad CSV file never leaves the table half updated. `Table::append` reserves space in every column before moving anything in, so if it runs out of memory the table is left exactly as it was.

`DELETE` finds the rows to remove, builds one keep-mask, and compacts every column with it in a single pass. Rows that survive keep their original order.

## Indexes

`Index` is an abstract base class, and a `Table` owns its index through a `std::unique_ptr<Index>`. The index type is picked at runtime by `GENERATE`, so a virtual interface makes sense here. It only costs one virtual call per query, not per row.

The two implementations are templates on the column's type:

- `HashIndex<K>` uses `std::unordered_map<K, std::vector<RowId>>` and only answers `=`
- `OrderedIndex<K>` uses `std::map<K, std::vector<RowId>>` and answers `=`, `<`, `<=`, `>=`, `>`

Both inherit their shared update logic from `PostingsIndex<Map>`. Keys are stored as raw `int`/`double`/`string` values instead of `Value`s, so hashing and comparing don't go through the variant.

When rows are inserted, only the new rows get added to the index. When rows are deleted, every row after them shifts down, so the index gets a table mapping old row ids to new ones and updates its lists in place. An earlier version rebuilt the whole index after every delete. Deleting one row from a 1M row table took 119 ms with a `bst` index that way, and it now takes 15 ms.

The tradeoff is that every distinct key has its own small vector of row ids, which uses more memory than a flat layout. A flat layout would make single row inserts O(n) though, so I kept the vectors.

## Query Planner

For a `WHERE` clause, the planner picks:

| Index on the column | `=` | `<` `<=` `>=` `>` | `!=` |
|---|---|---|---|
| none | full scan | full scan | full scan |
| hash | hash lookup | full scan | full scan |
| bst | ordered lookup | ordered range scan | full scan |

Full scans and equality lookups return rows in insertion order. Range scans on a `bst` index return rows in key order. These are the same rules as the original version, and the randomized tests check them.

### Joins

`JOIN t1 AND t2` goes through `t1` in row order and, for each row, prints the matching `t2` rows in row order. If `t2` has a hash index on the join column it uses that. Otherwise it builds a temporary hash table that maps each key to its first matching row, plus one `next` array that chains rows with the same key together. String keys are `std::string_view`s into the column instead of copies. That layout cut the 1M row join from 75 ms to 36 ms compared to a map of vectors.

The original version also used a `bst` index for joins when there was one. Benchmarks showed that's slower than just building a hash table at every size (497 ms vs 36 ms at 1M rows), so the planner doesn't do that anymore. The output is the same either way.

## Aggregation

`AGGREGATE` reads the value column once and groups rows with an `unordered_map`. Only the distinct groups are sorted at the end so they print in key order. Switching from `std::map` to `unordered_map` took a 1M row `GROUP BY` from about 85 ms to 19 ms.

- `SUM` and `AVG` only work on int and double columns, and the parser checks that
- Int sums use `int64_t` so they don't overflow
- Double sums use Neumaier (compensated) summation so the rounding error doesn't grow with the number of rows
- Rows are always added up in row order, even when an index returns them in key order, so the result doesn't change depending on which index exists

## CSV Loading

`LOAD` reads a header row, matches each header name to a table column (in any order), and then parses each line. It supports quoted fields and `""` escapes. Values go through the same parsing code as the command language, and nothing is added to the table unless the whole file is valid. Errors include the line number and the bad value.

## Testing

- **Unit tests** (`tests/unit`): one file per class, covering values, columns, tables, both index types, the parser and all its error messages, the planner, scans, joins, aggregation, CSV loading, and the CLI
- **Randomized tests**: run the same random inserts, deletes, and queries against a table with no index, a hash index, and a bst index, and compare all three to a simple reference implementation. Joins are checked the same way against a nested loop join.
- **Integration tests** (`tests/integration`): run each `.sql` script through the real executable in normal and quiet mode and compare the output byte for byte. The expected output for the original test cases came from the original version.
- Everything also runs under AddressSanitizer and UBSan, locally with `make asan` and in CI.
