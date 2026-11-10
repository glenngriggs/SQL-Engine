# SillyQL

An in-memory, column-oriented database engine for a small SQL-like command language, written in
C++20. It reads commands from standard input and supports typed tables, hash and ordered
(balanced-tree) indexes, filtered reads and deletes, equi-joins, grouped aggregation, CSV bulk
loading, and `EXPLAIN` for the query planner's choices.

```text
$ ./build/release/bin/silly < demo.sql
% New table trades with column(s) ts symbol side price qty created
% Loaded 200 rows into trades from position 0 to 199
% Generated bst index for table trades on column price, with 183 distinct keys
% Plan: ordered-index range scan on price (<)
% symbol price qty
NVDA 129.02 500
NVDA 129.04 300
NVDA 129.1 1000
NVDA 129.18 100
NVDA 129.4 500
Printed 5 matching rows from trades
% symbol AVG(price)
AAPL 188.911
AMZN 185.212
GOOG 172.053
MSFT 420.281
NVDA 130.02
Aggregated 55 rows from trades
% Thanks for being silly!
```

(Produced by `CREATE`, `LOAD`, `GENERATE ... bst INDEX ON price`, `EXPLAIN PRINT ...`,
`PRINT ... WHERE price < 129.5` and `AGGREGATE FROM trades AVG price GROUP BY symbol WHERE
qty >= 500` on [`examples/trades.csv`](examples/trades.csv). Every value printed in a result row
is followed by a space; the listing above omits those trailing spaces.)

## Features

* **Typed, columnar tables.** `int` (32-bit), `double`, `bool`, `string` columns, each stored as a
  contiguous typed vector.
* **Indexes.** `hash` (equality) and `bst` (ordered: equality and ranges), maintained on insert,
  renumbered in place after delete, replaced by a new `GENERATE`.
* **Predicates** `<  <=  =  !=  >=  >` in `PRINT`, `DELETE` and `AGGREGATE`, with a planner that
  chooses between a typed full scan, a hash probe, and an ordered point lookup or range scan.
* **Equi-join** of two tables (or a table with itself), probing an existing hash index or
  building a temporary hash table.
* **AGGREGATE** `COUNT | SUM | AVG | MIN | MAX` with optional `GROUP BY` and `WHERE`; 64-bit
  integer sums and compensated (Neumaier) floating-point sums.
* **LOAD** a CSV file into a table: header columns in any order, quoted fields, type-checked
  (string values must be single tokens, as everywhere else in the language), all-or-nothing.
* **EXPLAIN** any `PRINT`, `DELETE`, `JOIN` or `AGGREGATE` to see the access path without running it.
* **CLI flags** `-q/--quiet` (suppress result rows), `-t/--timing` (per-command latency on stderr),
  `-h/--help`, parsed with getopt_long's conventions (combined short flags, long-option prefixes,
  `--`).

## Design highlights

The full write-up is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

* **Column store with typed scan kernels.** `Column` wraps
  `std::variant<vector<int32_t>, vector<double>, vector<uint8_t>, vector<string>>`. A filter
  visits the variant once, then runs a template kernel (`scanValues<T>`) on the raw array with the
  operator hoisted out of the loop. Selection is branch-free: row ids are written to a block
  buffer unconditionally and the cursor advances by the comparison result. Measured on a
  50%-selective scan of 1M rows: the columnar layout alone is 2.6x faster than a row-of-variants
  model of the original layout, and branch-free selection a further 5.7x (15x combined).
* **Interfaces where the choice is made at runtime, templates where it isn't.** `Index` is an
  abstract base class owned through `std::unique_ptr<Index>` (the kind is picked by `GENERATE`).
  The implementations, `HashIndex<K>` and `OrderedIndex<K>`, are templates over the column's
  storage type that share their maintenance code through `PostingsIndex<Map>` and differ only in
  the container and the lookups it can answer. Keys are raw ints/doubles/strings, and the join
  probes with the column's raw keys after one `dynamic_cast` per join: no variant dispatch or
  virtual call per row.
* **A real front end.** `TokenStream` → `Parser` → `Command`, where `Command` is a `std::variant`
  of command structs. The executor dispatches with `std::visit`, so an unhandled command is a
  compile error. The parser binds names to column positions and types literals by column. Each
  command reads its fixed header (e.g. `FROM <t> WHERE <col> <op>`) in full before checking it,
  as the original did, so the legacy line-based error recovery is reproduced exactly even when a
  command spans lines.
* **Errors are exceptions with a hierarchy.** `SqlError` subclasses carry the exact user-facing
  message; the REPL prints it, discards the rest of the line and continues. `InputError` is fatal
  (exit 1). `Table::append` and `Table::erase` give the strong exception guarantee: they allocate
  everything before modifying the table. `main()` is about 30 lines.
* **Deterministic output.** Documented ordering rules (row order for scans and equality probes,
  key order for ordered range scans, left-then-right row order for joins) are asserted by a
  randomized differential test. Aggregates accumulate in row order whatever the access path, so
  floating-point results do not depend on which index exists.
* **Measured planner decisions.** Benchmarks showed that probing an ordered index for a join is
  slower than building a temporary hash table at every size measured, so the planner only probes
  hash indexes. Output is identical either way.
* **Modern C++20 throughout.** Concepts (`StorageType`), `<=>`, `std::span`, `std::from_chars`,
  heterogeneous lookup in `std::unordered_map`, designated initializers, RAII ownership, no raw
  `new`/`delete`, const-correct interfaces. Built with
  `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`; clang-tidy clean.

## Build, test, benchmark

Requirements: a C++20 compiler (tested with g++ 13.3), CMake 3.20 or newer. GoogleTest 1.15.2 and
Google Benchmark 1.9.1 are fetched automatically.

```bash
make              # cmake --preset release && cmake --build --preset release
make test         # ctest --preset release
make asan         # build and test with AddressSanitizer + UndefinedBehaviorSanitizer
make bench        # run the Google Benchmark suite (release build)
make format       # clang-format all sources
make clean
```

Or with CMake directly:

```bash
cmake --preset release            # also: debug, asan
cmake --build --preset release -j
ctest --preset release -j
./build/release/bin/sillyql_benchmarks
```

Options: `SILLYQL_BUILD_TESTS` and `SILLYQL_BUILD_BENCHMARKS` (`ON` by default when SillyQL is the
top-level project, `OFF` when it is added to another project with `add_subdirectory`; the `asan`
preset turns benchmarks off). The `debug` and `asan` presets also enable libstdc++ assertions
(`-D_GLIBCXX_ASSERTIONS`, bounds-checked `operator[]`).

`cmake --install build/release --prefix <dir>` installs `silly`, `libsillyql.a`, the headers and
a CMake package, so another project can `find_package(sillyql)` and link `sillyql::sillyql`.

## Command language

Commands are whitespace-separated tokens and may span lines; each prints a `% ` prompt first.
Names and string values are single tokens. A token starting with `#` where a command is expected
begins a comment that runs to the end of the line (so a trailing `# note` after a command is a
comment too).

| Command | Syntax |
|---------|--------|
| Create a table | `CREATE <table> <N> <type1> ... <typeN> <col1> ... <colN>` (types: `int double bool string`) |
| Insert rows | `INSERT INTO <table> <N> ROWS`, then N rows of values |
| Load a CSV file | `LOAD INTO <table> FROM <path.csv>` **(new)** |
| Print rows | `PRINT FROM <table> <N> <col1> ... <colN> ALL` |
| | `PRINT FROM <table> <N> <col1> ... <colN> WHERE <col> <op> <value>` |
| Delete rows | `DELETE FROM <table> WHERE <col> <op> <value>` |
| Build an index | `GENERATE FOR <table> {hash \| bst} INDEX ON <col>` |
| Join | `JOIN <t1> AND <t2> WHERE <c1> = <c2> AND PRINT <N> <col> {1\|2} ...` |
| Aggregate | `AGGREGATE FROM <table> <FUNC> <col> [GROUP BY <col>] [WHERE <col> <op> <value>]` **(new, one line)** |
| Explain | `EXPLAIN <PRINT \| DELETE \| JOIN \| AGGREGATE command>` **(new)** |
| Drop a table | `REMOVE <table>` |
| Exit | `QUIT` |

`<op>` is one of `<`, `<=`, `=`, `!=`, `>=`, `>` (`<=`, `>=`, `!=` are new). `FUNC` is one of
`COUNT`, `SUM`, `AVG`, `MIN`, `MAX`; `SUM` and `AVG` need an `int` or `double` column.

### Output

| Command | Output (result rows and their header are omitted with `-q`) |
|---------|------------------------------------------------------------|
| CREATE | `New table <t> with column(s) <c1> ... <cN> created` |
| INSERT / LOAD | `Added <n> rows to <t> from position <first> to <last>` / `Loaded <n> rows into <t> from position <first> to <last>` |
| PRINT | header, rows, then `Printed <n> matching rows from <t>` |
| DELETE | `Deleted <n> rows from <t>` |
| GENERATE | `Generated <hash\|bst> index for table <t> on column <c>, with <k> distinct keys` |
| JOIN | header, rows, then `Printed <n> rows from joining <t1> to <t2>` |
| AGGREGATE | header `[<group col>] FUNC(<col>)`, one row per group in ascending key order, then `Aggregated <n> rows from <t>` |
| EXPLAIN | `Plan: <access path>` |
| REMOVE | `Table <t> removed` |
| QUIT | `Thanks for being silly!` |

Every value in a header or result row is followed by one space. Doubles print with default
`iostream` formatting (6 significant digits), bools as `true`/`false`. `AVG` is always a double;
`SUM` of an int column is an exact 64-bit integer. Without `GROUP BY`, an aggregate over no rows
prints `0` for `COUNT` and `NULL` otherwise.

**Row order:** full scans and equality lookups print in insertion (row) order; range predicates
answered by a `bst` index print in ascending key order (row order among equal keys); joins print
the left table's rows in row order, each followed by its matching right rows in row order.

### Errors

Command errors are printed to stdout; the rest of the line on which the error was detected is
discarded and the next command is read. A command's fixed header (everything up to its first
variable-length part, e.g. `INTO <t> <N> ROWS` or `FROM <t> WHERE <col> <op>`) is always read in
full first, as the original did:

```text
Error: unrecognized command
Error during <CMD>: <table> does not name a table in the database
Error during <CMD>: <column> does not name a column in <table>
Error during CREATE: Cannot create already existing table <table>
Error during <CMD>: <detail>        e.g. Invalid column type float, Expected WHERE, found x
```

Two errors are fatal; they are printed to stderr and the program exits with status 1:

```text
Error: Unknown command line option      (bad command-line argument)
Error: Reading from cin has failed      (input ended before QUIT, or a malformed number/value)
```

## CLI usage

```text
Usage: silly [options] < inputfile
  -h, --help     Show this help message and exit
  -q, --quiet    Suppress result rows (summaries and errors are still printed)
  -t, --timing   Print each command's elapsed time in microseconds to stderr
```

```bash
./build/release/bin/silly < examples/market_demo.sql         # full output
./build/release/bin/silly -q < examples/market_demo.sql      # summaries only
./build/release/bin/silly -qt < examples/market_demo.sql 2> timings.txt
```

With `-t`, each command, including one that fails with an error, adds a line such as
`[timing] LOAD 66 us` to stderr. The clock (`steady_clock`) starts once the command's keyword has
arrived and covers parsing and execution, so time spent waiting for input is not counted. stdout
is byte-identical to a run without `-t`. When standard input is a terminal the prompt is flushed
before each read; otherwise output is fully buffered (`std::cin` is untied from `std::cout`).

The market-data example ([`examples/market_demo.sql`](examples/market_demo.sql)) loads 200
synthetic trades generated deterministically by
[`examples/generate_trades.py`](examples/generate_trades.py), runs range queries against a `bst`
index on price, aggregates by symbol and side, explains plans before and after adding a hash
index, joins against a watchlist, and re-aggregates after a delete. It is also a golden test.

## Benchmarks

Release build (`-O3`), Google Benchmark, median of 5 repetitions, on this development machine:
32 vCPUs at 2.42 GHz (WSL2, Linux 5.15), 48 KiB L1d / 2 MiB L2 per core, 36 MiB L3, g++ 13.3.
Tables have unique int ids, 64 symbols, and prices uniform in [0, 1000).

**Point lookup** (`WHERE id = k`, one matching row), time per query:

| Rows | Full scan | Hash index | Ordered index |
|-----:|----------:|-----------:|--------------:|
| 10,000 | 3.32 µs | 19 ns | 142 ns |
| 100,000 | 33.1 µs | 29 ns | 259 ns |
| 1,000,000 | 336 µs | 99 ns | 782 ns |

**Range query** (`WHERE price < 10`, about 1% of rows):

| Rows | Full scan | Ordered index |
|-----:|----------:|--------------:|
| 10,000 | 4.42 µs | 0.36 µs |
| 100,000 | 43.1 µs | 5.95 µs |
| 1,000,000 | 443 µs | 253 µs |

At 1M rows the ordered index wins by only 2x: collecting 10,000 rows means walking 10,000 tree
nodes scattered in memory, while the scan streams one 8 MB column sequentially at about 2 billion
rows per second.

**Storage layout** (`price < 500`, about 50% of rows selected). The baseline models the original
layout: a `vector<vector<Value>>` row store, each cell read with a typed `std::get_if` on its
variant, one branch per row.

| Rows | Columnar, branch-free kernel | Columnar, branch per row | Rows of variants, branch per row |
|-----:|-----------------------------:|-------------------------:|---------------------------------:|
| 10,000 | 4.86 µs | 23.1 µs | 26.3 µs |
| 100,000 | 46.9 µs | 313 µs | 536 µs |
| 1,000,000 | 0.59 ms | 3.39 ms | 8.75 ms |

At 1M rows the layout alone is worth 2.6x (8.75 vs 3.39 ms: one 8 MB array streamed instead of
a pointer chase per row), and branch-free selection a further 5.7x (3.39 vs 0.59 ms: no
misprediction on every other row), 15x combined. At 10,000 rows everything fits in cache and the
layout matters little; the branch dominates.

**Join** (N trades x N reference rows on a unique int key):

| Rows | Temporary hash table | Existing hash index | Existing ordered index (forced) |
|-----:|---------------------:|--------------------:|--------------------------------:|
| 10,000 | 0.22 ms | 0.023 ms | 0.65 ms |
| 100,000 | 2.18 ms | 0.40 ms | 13.8 ms |
| 1,000,000 | 36.2 ms | 15.2 ms | 497 ms |

The temporary table is one hash node per distinct key plus a flat `next` array chaining equal
keys, with string keys as views into the column. The last column is why the planner never
probes an ordered index for a join.

**Insert throughput:** `INSERT` of 100,000 rows from command text (tokenize, parse literals,
append) runs at 7.8M rows/s (168 MiB/s of input). Appending a prepared 1M-row batch to a table
that already holds 1M rows (every column grows and moves its elements) takes 18.1 ms without an
index and 33.8 ms while maintaining a hash index on a string column; a batch appended to a new
table adopts the batch's buffers in O(1).

**Delete with an index** (`-t`, median of 10 single-row `DELETE ... WHERE id = k` on a 1M-row
table): 0.7 ms without an index, 9 ms with a hash index and 15 ms with a `bst` index. The index
is renumbered in place; rebuilding it, as an earlier version did, took 28 ms and 119 ms.
A script of 200,000 one-row `INSERT`s and 2,000 `PRINT`s on a hash-indexed table followed by 500
`DELETE`s with a `bst` index runs in 1.9 s (19 s with the rebuild).

## Testing

`ctest` runs 319 tests; all pass in both the `release` and `asan` (ASan + UBSan, with libstdc++
assertions) presets.

* **255 unit tests** (GoogleTest, `tests/unit`, one file per component): values, literals,
  columns, tables, catalog, shared index maintenance (typed over both kinds), each index kind,
  token stream, parser (every error message, and how many tokens each error consumes), planner,
  scan kernels, joins, aggregation, CSV loading, result writer, executor, session (including
  timing against a slow input stream), CLI options.
* **Randomized differential tests** (fixed seeds): random inserts, deletes, index changes and
  queries over int, string, double and bool columns against tables with no index, a hash index
  and an ordered index must match a naive reference model and honour each access path's ordering
  rule. A second test runs random scripts through the full session with and without `GENERATE`,
  and a third checks both join strategies, and self-joins, against a nested-loop join.
* **64 integration tests** (`tests/integration`): 25 golden scripts, each run in verbose and
  quiet mode with stdout compared byte for byte, plus CLI tests for flags, exit codes and stderr.
  The legacy golden files, and the `multiline_headers` and `double_underflow` cases, were
  produced by the original implementation.

CI (`.github/workflows/ci.yml`) builds and tests the `release` and `asan` presets on Ubuntu 24.04,
smoke-runs the benchmarks, and runs a lint job that fails on any `clang-format` difference or
`clang-tidy` warning (`make lint` runs the same checks locally).

## Behaviour changes from the original implementation

Output is byte-identical to the original for every script it handled correctly (checked
against all legacy goldens and against the original binary on 4,000 randomized scripts, half of
them with commands split across lines at random points, each in verbose and quiet mode).
The differences are deliberate:

1. **Bad column in a `PRINT` or `JOIN` column list** now aborts the command after the error
   message (no rows, no summary). The original printed the error, skipped the rest of the line
   and kept reading column names from the *next* line.
2. **Self-joins** (`JOIN t AND t ...`) print each column from the side it names. The original
   chose the row by comparing table addresses, which are equal in a self-join, so every column
   came from the left row. The `self_join` golden was regenerated with the fixed engine.
3. **Invalid `CREATE`s are errors**: unknown column types (`Invalid column type float`; the
   original silently dropped the column, misaligning names and types), duplicate column names,
   and zero columns.
4. **Invalid values in `GENERATE`/`JOIN` are errors**: an unknown index type (the original reported
   a "generated" index with 0 keys and left the table inconsistent), joining columns of different
   types (undefined behaviour in the original), and a table number other than 1 or 2.
5. **Keywords and operators are validated.** The original skipped over keywords (`INTO`, `ROWS`,
   `FROM`, `WHERE`, `AND`, ...) without checking them and treated an unknown operator as "matches
   nothing". These now produce `Error during <CMD>: Expected ..., found ...` or
   `Invalid comparison operator ...`. As in the original, a command's whole fixed header is read
   before anything in it is checked, so an error consumes exactly the tokens it always did.
6. **`INSERT INTO t 0 ROWS`** prints `Added 0 rows to t` instead of
   `... from position 0 to 18446744073709551615`.
7. **Input errors fail fast.** When input ends before `QUIT`, or a number cannot be read, the
   original re-ran the previous command with failed reads (printing garbage) before exiting with
   status 1. The engine now prints `Error: Reading from cin has failed` to stderr immediately and
   exits with status 1.
8. **Command line.** An unknown option prints the documented `Error: Unknown command line option`
   (the original printed it without the `Error: ` prefix, after getopt's own message); positional
   arguments (including a lone `-` and anything after `--`) are rejected instead of ignored;
   `--help` describes the new `-t` flag. getopt_long's other conventions are kept: combined short
   flags, unambiguous long-option prefixes (`--qui`), `--`, and stopping at `-h`.
9. **Joins never probe a `bst` index** (see Benchmarks). Output is unchanged; only `EXPLAIN` shows it.

## Repository layout

```text
apps/main.cpp          command-line entry point
include/sillyql/       public headers of the sillyql library
src/                   library implementation
tests/unit/            GoogleTest unit tests, one file per component
tests/integration/     golden scripts and expected output, CSV fixtures, CLI cases, runner
benchmarks/            Google Benchmark suite
examples/              market-data demo script, CSV and its generator
docs/ARCHITECTURE.md   design notes
```
