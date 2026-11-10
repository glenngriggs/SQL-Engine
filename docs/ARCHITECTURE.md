# SillyQL architecture

This document explains how a command travels through the engine, how data is laid out, and why
the main design decisions were made. File references are relative to the repository root.

## 1. Pipeline

```
 stdin ──► TokenStream ──► Parser ──────────► Command ──► Executor ──► ResultWriter ──► stdout
        (token_stream.hpp) (parser.hpp)       (variant)   (executor.hpp) (result_writer.hpp)
                             │ binds names        │            │
                             ▼                    │            ▼
                           Catalog ◄──────────────┴──────── Planner / Join / Aggregate / CSV
                           (catalog.hpp)                   (planner.hpp, join.hpp, ...)

 Session (session.hpp): prompt → read keyword → [clock starts] parse → execute → (timing) → repeat
```

`apps/main.cpp` only parses the command line and hands `std::cin`/`std::cout`/`std::cerr` to a
`Session`. Everything else lives in the `sillyql` static library so it can be unit tested and
benchmarked without a process boundary.

| Component | Responsibility |
|-----------|----------------|
| `Value` (`value.hpp`) | One typed cell: `std::variant<int32_t, double, bool, std::string>`. Equality, ordering (same type only), printing. Carries WHERE literals and MIN/MAX results; storage and indexes use raw typed vectors and keys. |
| `Column` (`column.hpp`) | One attribute stored contiguously as a typed vector. |
| `Table` (`table.hpp`) | Schema, one `Column` per attribute, row count, and an optional `std::unique_ptr<Index>`. |
| `Index`, `PostingsIndex<Map>`, `HashIndex<K>`, `OrderedIndex<K>` | Secondary indexes behind an abstract interface; shared maintenance in `PostingsIndex`. |
| `Catalog` | Tables by name. |
| `TokenStream` (`token_stream.hpp`) | Whitespace tokens from an `std::istream`, plus the two line-oriented operations the language needs. |
| `Parser` | Turns tokens into a bound `Command` (a `std::variant` of command structs). |
| `Planner` (`planner.hpp`) | Picks an access path for a predicate and a join strategy; produces EXPLAIN text. |
| `scan.hpp`, `join.hpp`, `aggregate.hpp`, `csv_loader.hpp` | Execution kernels. |
| `Executor` | Runs a command against the catalog and reports through the `ResultWriter`. |
| `ResultWriter` | Output formatting and the trailing-space convention; enforces quiet mode (row cells are no-ops). |
| `Session` | The REPL loop, error recovery and per-command timing. |

## 2. Front end: parse and bind in one pass

The language is token based, not line based: `INSERT` rows follow on later lines, and a command
may be split across lines. Two rules are line based, though:

* Error recovery prints one message and discards **the rest of the current line**. Whatever was
  on later lines is read as new commands. This is observable: after
  `INSERT INTO missing 2 ROWS`, both row lines are reported as `Error: unrecognized command`.
* `AGGREGATE`'s optional `GROUP BY` / `WHERE` clauses must be on the command's own line, so the
  parser can tell "no more clauses" from "the next command" (`TokenStream::hasMoreOnLine`). A
  `#` there starts a comment and ends the clauses, exactly as a trailing comment after any other
  command is read as the next (comment) command.

Because of the first rule, *where* an error is detected is part of the behaviour. The original
read each command's fixed-length header with one chain of `>>` extractions before checking any
of it (`JOIN` reads all ten tokens `t1 AND t2 WHERE c1 = c2 AND PRINT N` first), and checked the
variable-length parts (column lists, the `WHERE` value) as it read them. The `Parser` does the
same: `readHeader<N>()` reads the header, then checks keywords, then table names, then column
names, then operators and counts. A header split across lines therefore loses only the rest of
the line it ends on, never its continuation lines. A golden test (`multiline_headers`) and a
randomized differential run against the original binary with headers split at random points
check this.

The parser must also know the schema while it reads: literal syntax is type directed (`3` is an
int in one column and a double in another) and `INSERT` needs the table's width to know how many
tokens form a row. The `Parser` therefore binds as it goes: names are resolved against the
catalog, literals are parsed by their column's type, and the resulting `Command` holds column
positions and typed `Value`s. The executor never sees an unresolved name.

`Command` is a closed `std::variant` of plain structs (`CreateCmd`, `InsertCmd`, `PrintCmd`,
`DeleteCmd`, `GenerateCmd`, `JoinCmd`, `RemoveCmd`, `AggregateCmd`, `LoadCmd`, `ExplainCmd`,
`QuitCmd`, `CommentCmd`). The executor dispatches with `std::visit` over an overload set, so a
new command that is not handled is a compile error rather than a missed `else if`.

### Errors

```
std::runtime_error
├── SqlError                      recoverable: print what(), skip rest of line, continue
│   ├── UnknownCommandError       "Error: unrecognized command"
│   ├── TableNotFoundError        "Error during PRINT: t does not name a table in the database"
│   ├── ColumnNotFoundError       "Error during PRINT: c does not name a column in t"
│   ├── TableExistsError          "Error during CREATE: Cannot create already existing table t"
│   ├── SyntaxError               "Error during JOIN: Expected =, found =="
│   ├── SemanticError             "Error during CREATE: Invalid column type float"
│   └── LoadError                 "Error during LOAD: Line 3: 'x' is not a valid int value ..."
├── InputError                    fatal: "Error: Reading from cin has failed" on stderr, exit 1
└── UsageError                    fatal: "Error: Unknown command line option" on stderr, exit 1
std::logic_error
└── TypeMismatchError             programming error (e.g. ordering an int against a string)
```

`what()` is always the complete user-facing line, so the `Session` catch block is three lines.
`InputError` covers what the original treated as a stream failure: input ending before `QUIT`
and a token that cannot be read as the number the grammar requires. Literals follow stream
extraction's rules exactly (`literal.hpp`): `std::from_chars` does the parsing, plus the two
places where it differs from `operator>>`: a leading `+` is accepted, and a double too small to
represent (`1e-400`) reads as a signed zero instead of failing.

## 3. Storage: columnar tables

The original stored rows as `vector<vector<Field>>`, where `Field` was a tagged union. A filter on
one attribute walked every row's heap-allocated vector and dispatched on the tag of every cell.

`Table` instead keeps one `Column` per attribute, and a `Column` is
`std::variant<vector<int32_t>, vector<double>, vector<uint8_t>, vector<std::string>>`. This is
the column-store layout used by analytical engines and kdb+-style tick databases:

* **Bytes touched.** `WHERE price < 500` reads only the price column: 8 bytes per row in one
  contiguous array, every byte of every 64-byte cache line useful, and a sequential access
  pattern the hardware prefetcher follows. A row layout drags whole rows (and their heap
  indirections) through the cache to look at one field.
* **No per-cell dispatch.** The type check happens once per scan (`Column::visit`), not once per
  cell. Inside, `scanValues<T>` is a template instantiated per element type and per operator
  (the `switch` on the operator is hoisted out of the loop), so the inner loop is a plain
  `double < double` compare that the compiler can unroll.
* **Branch-free selection.** `selectRows` writes every row id into a 1024-entry stack buffer and
  advances the write cursor by the predicate's 0/1 result. There is no data-dependent branch, so
  a 50%-selective filter does not pay a misprediction on every other row.
* **Bools as bytes.** `vector<bool>` is a bit-packed proxy container; one byte per bool keeps the
  column an ordinary array that the same kernels scan.
* **32-bit row ids.** `RowId` is `uint32_t`, halving the size of index postings and selection
  vectors. A table is limited to 2^32 - 1 rows (`Table::append` checks).

The `bench_layout` benchmark separates the two effects on this machine, for a 50%-selective
filter on 1M rows: a branchy loop over rows of variants (a model of the original layout, each
cell read with a typed `std::get_if`) takes 8.75 ms; the same branchy loop over the typed column
3.39 ms (layout: 2.6x); the branch-free kernel on the column 0.59 ms (a further 5.7x, 15x in
total). See the README.

What columnar costs: reconstructing a whole row touches every column, and appending a row writes
to N vectors. Both are cheap here because output is formatted cell by cell anyway and inserts
arrive in batches (`RowBatch`).

### Inserts, deletes and batches

`INSERT` and `LOAD` first build a `RowBatch` (columns of the table's types), validating every
value, and only then call `Table::append`. A malformed row or CSV file therefore never reaches
the table. `Table::append` itself gives the strong exception guarantee:

1. `Column::reserveFor` allocates room in every column first (growing capacity geometrically, so
   a stream of one-row `INSERT`s stays amortised O(1) per row). This is the only step that can
   fail, and it changes nothing visible.
2. `Column::appendFrom` then moves the batch in, which cannot throw. A column that has never
   allocated (a new table) simply adopts the batch's buffer, so the first `INSERT` or `LOAD` into
   a table costs O(1) per column instead of moving every element.
3. If extending the index throws (`bad_alloc`), the index forgets the new rows
   (`Index::truncate`) and the columns are truncated back, so the table is exactly as before.

`DELETE` selects the doomed row ids, builds one keep-mask (`vector<uint8_t>`) and compacts every
column with it in a single stable pass (`Column::compact`), so surviving rows keep their
relative order. Everything that allocates (the mask, the index's renumbering table) is built
before the first column changes.

## 4. Indexes: runtime polymorphism outside, templates inside

`Index` is an abstract base class (`kind`, `name`, `column`, `supports`, `lookup`, `extend`,
`renumber`, `truncate`, `distinctKeys`), owned by the table through `std::unique_ptr<Index>`. The
index kind is chosen at runtime by `GENERATE`, so virtual dispatch is the natural fit at the
table boundary. A filtered read makes one virtual `lookup` call per query; maintenance makes one
call per `INSERT`/`LOAD`/`DELETE`.

The implementations are class templates over the column's storage type:

* `PostingsIndex<Map>` holds the map from key to postings list and implements everything both
  kinds share: `extend`, `renumber`, `truncate`, `distinctKeys`, and a typed (non-virtual)
  `equalRange(const Key&)`.
* `HashIndex<K>`: `PostingsIndex<std::unordered_map<K, std::vector<RowId>>>`, equality only.
* `OrderedIndex<K>`: `PostingsIndex<std::map<K, std::vector<RowId>>>`, equality and
  `< <= >= >` (its `lookup` only chooses the `lower_bound`/`upper_bound` pair for the operator).

`makeIndex(kind, type, column)` instantiates the right one through `visitStorageType`, the one
place where a runtime `ColumnType` selects a template instantiation (it also builds `Column`
storage and parses typed literals). Keys are stored as raw `int32_t`/`double`/`uint8_t`/
`std::string`, not as `Value`s, so hashing and comparing involve no variant dispatch, and
`storageKey<K>(value)` lets `lookup` probe with a reference to the `Value`'s string rather than a
copy. The join goes further: it recovers the concrete `HashIndex<K>` with one `dynamic_cast` per
join and probes `equalRange` with the probe column's raw keys, so there is no `Value` and no
virtual call per row.

**Invariant:** every postings list is in ascending row-id order. Rows are indexed in increasing
id order, and renumbering preserves relative order. The output-order rules below depend on this.

**Maintenance:** `INSERT`/`LOAD` index only the new rows (`extend`). `DELETE` shifts every
surviving row down, so it hands the index a renumbering table (`newIds[old]`, or `kNoRow` for a
deleted row, built once from the keep-mask by a prefix count) and `renumber` rewrites each
postings list in place, dropping deleted ids and erasing keys left without rows. That is O(n)
with no allocation. The earlier design rebuilt the index from scratch, reallocating a node and a
postings vector per key: a single-row `DELETE` on a 1M-row table took about 119 ms with a `bst`
index and 28 ms with a hash index (medians of 10, `-t`); renumbering takes 15 ms and 9 ms
(without an index, 0.7 ms). `GENERATE` replaces the previous index (a table has at most one).

**Trade-off: a postings vector per key.** Each distinct key owns a map node and a
`std::vector<RowId>`. That makes every maintenance operation above simple and incremental, but
for unique keys it costs roughly 90-100 bytes per row and two dependent pointer loads per probe.
Denser layouts exist (all postings in one array with per-key offsets, or a sorted
`vector<pair<Key, RowId>>` for the ordered index), but they turn the common one-row `INSERT` into
an O(n) merge or rebuild. The temporary join table, which is built once and never updated, does
use a flat layout (section 5).

## 5. Planner and output order

`planScan` chooses an `AccessPath` for an optional predicate:

| Index on the predicate column | `=` | `< <= >= >` | `!=` |
|-------------------------------|-----|-------------|------|
| none / on another column      | full scan | full scan | full scan |
| hash                          | hash probe | full scan | full scan |
| bst (ordered)                 | ordered point lookup | ordered range scan | full scan |

The plan borrows the command's predicate (`const Predicate*`) rather than copying its `Value`.
`executeScan` returns row ids in the order the engine prints them:

* full scan and equality probes: ascending row order;
* ordered range scan: ascending key order, and row order within a key.

These are exactly the legacy rules (a BST index returned range results in key order), and the
randomized differential test asserts them for every query.

**Joins.** `JOIN t1 AND t2` iterates `t1` in row order and, for each row, emits the matching
`t2` rows in ascending row order. If `t2` has a hash index on the join column the join probes
it; otherwise it builds a temporary hash table over `t2`'s column. That table is laid out flat:
an `unordered_map<K, RowId>` from each distinct key to its first row, plus one
`vector<RowId> next` that chains the rows sharing a key (filled from the last row to the first,
so every chain is in ascending row order). That is one hash node per distinct key and no
per-key vector, and string keys are `std::string_view`s into the column (which cannot change
during the join) instead of copies. Compared with the earlier `unordered_map<K, vector<RowId>>`
this halved the 1M-row join (75 ms to 36 ms).

The original also probed an ordered index when one existed, but `bench_join` shows that
`O(log n)` pointer-chasing probes into a `std::map` are slower than building a hash table at
every size measured (0.65 ms vs 0.22 ms at 10k rows; 497 ms vs 36 ms at 1M), so the planner no
longer probes ordered indexes. Both strategies produce identical output in identical order
(`test_join.cpp` checks every strategy, including self-joins, against a nested-loop reference
on random tables with duplicate keys on both sides), so this is invisible except in `EXPLAIN`.

`EXPLAIN` prints the planner's decision without executing:

```
Plan: full scan of trades (filter: qty > 100)
Plan: hash-index probe on symbol (=)
Plan: ordered-index point lookup on price (=)
Plan: ordered-index range scan on price (<)
Plan: hash join probing existing hash index on venues.symbol
Plan: hash join building temporary hash table on venues.symbol (existing bst index not probed)
```

## 6. Aggregation

`aggregate()` visits the value column once to get its element type `S`, then (for `GROUP BY`)
visits the group column to get its key type `K`, and accumulates into an
`unordered_map<K, Accumulator<S>>`: one expected O(1) probe per row instead of an O(log G)
chain of string comparisons in a tree. Only the G distinct groups are then sorted to print them
in ascending key order. On 1M rows with 1000 string groups this took `AVG ... GROUP BY` from
about 85 ms (with `std::map`) to about 19 ms.

* `COUNT` works on any column; `MIN`/`MAX` on any ordered type; `SUM`/`AVG` only on int and
  double (checked by the parser, so the executor cannot be asked to sum strings).
* Int sums accumulate in `int64_t` (the test data sums two 2e9 values exactly).
* Double sums use Neumaier compensated summation (`CompensatedSum`): the error stays
  `O(epsilon)` instead of growing with the number of terms (`CompensatedSumTest` checks 10M
  additions of 0.1). Overflow returns infinity instead of NaN.
* Rows are accumulated in ascending row-id order. An ordered-index range scan returns rows in key
  order, and floating-point addition is not associative, so without this the same query could
  print different digits depending on whether an index exists. Every other access path already
  returns ascending ids, so the sort runs only when `std::is_sorted` says it is needed.
* Without `GROUP BY` there is always one result row: `COUNT` of no rows is `0`, and the other
  functions are `NULL`, as in SQL.

## 7. LOAD (CSV)

`readCsv` reads a header line, maps each header name to a table column (any order; every column
exactly once), then parses each record with a small RFC-4180-style splitter (quoted fields,
`""` escapes, trimmed blanks, CRLF endings, blank lines skipped). Values go straight into a
`RowBatch` through the same literal parsers the command language uses (`literal.hpp`), and the
batch is appended (and indexed) only when the whole file is valid. A string value must be one
non-empty token without whitespace, the only kind of string the command language can produce,
print as one cell and match in a `WHERE`; a quoted field may still contain commas. Errors name
the line number and quote the offending value (`Line 3: '' is not a valid int value for column
a`), and a path that is not a regular file is reported as `Cannot open file`.

## 8. Testing strategy

* **Unit tests** (`tests/unit`, one file per component): values, literals, columns, tables, the
  catalog, the shared index maintenance (`test_postings_index.cpp`, typed over both kinds), each
  index kind, the token stream, the parser (including every error message and how many tokens
  each error consumes), the planner, scan kernels, joins, aggregation, CSV loading, the result
  writer, the executor, the session (including timing with a slow input stream), and CLI parsing.
* **Randomized differential tests** (`test_differential.cpp`, 20 fixed seeds x 400 operations)
  apply the same random inserts, deletes, index changes and queries over int, string, double
  and bool columns to a table with no index, one with a hash index and one with an ordered index,
  and compare each against a naive reference model (as multisets), while also asserting each
  access path's output order. A second test runs random scripts through the full `Session` with
  and without `GENERATE` commands. `test_join.cpp` does the same for joins (30 seeds).
* **Golden tests** (`tests/integration`) run every `cases/*.sql` script through the real binary in
  verbose and quiet mode and compare stdout byte for byte. The legacy cases' expected files come
  from the original implementation; the new cases were checked by hand (and the market demo
  against an independent Python computation). CLI tests check exit codes and stderr.
* **Sanitizers**: the `asan` preset runs the whole suite under AddressSanitizer and UBSan; CI runs
  both `release` and `asan`.
