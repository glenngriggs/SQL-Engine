# SQL-Engine

In-memory database engine written in C++20 that runs a small SQL-like language (SillyQL). Tables are stored column by column, similar to column-oriented databases like kdb+ and DuckDB, and the engine supports hash and ordered indexes, joins, aggregation, and loading data from CSV files.

## Overview

The engine includes:

- A **parser** that:
  - Reads commands from standard input as a stream of tokens
  - Turns each command into a typed command object
  - Reports bad table/column names and syntax errors without stopping the program

- An **executor** that:
  - Creates, removes, and inserts into tables
  - Runs filtered `PRINT` and `DELETE` queries
  - Joins two tables on a shared column
  - Computes `COUNT`, `SUM`, `AVG`, `MIN`, and `MAX` with an optional `GROUP BY`
  - Loads rows from CSV files

- **Columnar storage** where:
  - Each column is one contiguous typed vector (`int`, `double`, `bool`, or `string`)
  - Filters run directly over the column arrays instead of going row by row
  - Deletes compact every column in a single pass

- **Indexes** that:
  - Share one abstract `Index` interface
  - Come in two types: `hash` for equality lookups and `bst` for ordered/range lookups
  - Are kept up to date on inserts and deletes

A small query planner chooses between a full scan, a hash lookup, and an ordered range scan for each query. `EXPLAIN` prints which one it would use.

More detail on the design is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Project Structure

```
.
├── apps/
│   └── main.cpp               # Entry point for the silly executable
├── include/sillyql/           # Public headers
│   ├── value.hpp              # Value type (std::variant of int/double/bool/string)
│   ├── column.hpp             # Typed column storage
│   ├── table.hpp              # Table made of columns + optional index
│   ├── index.hpp              # Abstract Index interface
│   ├── hash_index.hpp         # Hash index (equality lookups)
│   ├── ordered_index.hpp      # Ordered index (equality + range lookups)
│   ├── parser.hpp             # Parser that builds command objects
│   ├── planner.hpp            # Picks scan vs index for each query
│   ├── executor.hpp           # Runs commands against the catalog of tables
│   └── ...
├── src/                       # Library implementation
├── tests/
│   ├── unit/                  # GoogleTest unit tests, one file per component
│   └── integration/
│       ├── cases/             # .sql scripts with expected output
│       ├── data/              # CSV files used by the tests
│       └── cli/               # Command-line flag and exit code tests
├── benchmarks/                # Google Benchmark suite
├── examples/
│   ├── market_demo.sql        # Demo script on market data
│   ├── trades.csv             # 200 sample trades
│   └── generate_trades.py     # Script that generates trades.csv
├── docs/ARCHITECTURE.md
├── CMakeLists.txt
├── CMakePresets.json
└── Makefile
```

## Building

Requires CMake 3.20+ and a C++20 compiler (tested with g++ 13). GoogleTest and Google Benchmark are downloaded by CMake the first time you build.

```bash
make              # release build, binary ends up in build/release/bin/silly
make asan         # build and test with AddressSanitizer + UBSan
make clean
```

## Usage

Commands are read from standard input:

```bash
./build/release/bin/silly < examples/market_demo.sql
./build/release/bin/silly -q < examples/market_demo.sql    # only print summary lines
./build/release/bin/silly -t < examples/market_demo.sql    # print time per command to stderr
```

### Commands

```
CREATE <table> <N> <type1> ... <typeN> <col1> ... <colN>
INSERT INTO <table> <N> ROWS
LOAD INTO <table> FROM <file.csv>
PRINT FROM <table> <N> <col1> ... <colN> ALL
PRINT FROM <table> <N> <col1> ... <colN> WHERE <col> <op> <value>
DELETE FROM <table> WHERE <col> <op> <value>
GENERATE FOR <table> <hash|bst> INDEX ON <col>
JOIN <table1> AND <table2> WHERE <col1> = <col2> AND PRINT <N> <col> <1|2> ...
AGGREGATE FROM <table> <COUNT|SUM|AVG|MIN|MAX> <col> [GROUP BY <col>] [WHERE <col> <op> <value>]
EXPLAIN <PRINT|DELETE|JOIN|AGGREGATE command>
REMOVE <table>
QUIT
```

Column types are `int`, `double`, `bool`, and `string`. `<op>` can be `<`, `<=`, `=`, `!=`, `>=`, or `>`. Anything after a `#` is a comment.

### Example

Input:

```
CREATE trades 4 string string double int symbol side price qty
INSERT INTO trades 6 ROWS
AAPL buy 189.50 100
AAPL sell 190.25 50
MSFT buy 420.10 200
MSFT sell 419.80 300
NVDA buy 129.40 500
AAPL buy 188.90 400
GENERATE FOR trades bst INDEX ON price
PRINT FROM trades 3 symbol price qty WHERE price < 200
AGGREGATE FROM trades SUM qty GROUP BY symbol
EXPLAIN PRINT FROM trades 1 symbol WHERE price >= 400
QUIT
```

Output:

```
% New table trades with column(s) symbol side price qty created
% Added 6 rows to trades from position 0 to 5
% Generated bst index for table trades on column price, with 6 distinct keys
% symbol price qty
NVDA 129.4 500
AAPL 188.9 400
AAPL 189.5 100
AAPL 190.25 50
Printed 4 matching rows from trades
% symbol SUM(qty)
AAPL 550
MSFT 500
NVDA 500
Aggregated 6 rows from trades
% Plan: ordered-index range scan on price (>=)
% Thanks for being silly!
```

Since there's a `bst` index on price, the range query comes back sorted by price.

## Running Tests

```bash
make test
```

- `tests/unit` has unit tests for each part of the engine (parser, columns, indexes, planner, joins, aggregation, CSV loading, etc.)
- `tests/integration` runs full scripts through the executable and compares the output to the expected files, in both normal and quiet mode
- A randomized test runs the same inserts, deletes, and queries on tables with no index, a hash index, and a bst index, and checks that they all return the same rows

All tests also pass under AddressSanitizer and UBSan (`make asan`). CI runs both builds on every push.

## Benchmarks

```bash
make bench
```

Some results on a 1 million row table (release build):

| Query | Time |
|---|---|
| Lookup by id, full scan | 336 µs |
| Lookup by id, hash index | 99 ns |
| Lookup by id, bst index | 782 ns |
| Filter matching ~50% of rows, column storage | 0.59 ms |
| Same filter, row-based storage (original design) | 8.75 ms |
| Join, using an existing hash index | 15 ms |
| Join, building a temporary hash table | 36 ms |

## References

- Abadi, Boncz, Harizopoulos, Idreos & Madden, *The Design and Implementation of Modern Column-Oriented Database Systems*, Foundations and Trends in Databases, 2013
