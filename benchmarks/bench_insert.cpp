// Bulk insert throughput: end to end through the parser, and appending a prepared batch.

#include <benchmark/benchmark.h>

#include <optional>
#include <ostream>
#include <random>
#include <sstream>
#include <string>

#include "fixtures.hpp"
#include "sillyql/session.hpp"

namespace sillyql::bench {
namespace {

std::string insertScript(std::size_t rows) {
  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> symbol(0, 63);
  std::uniform_int_distribution<int> cents(0, 99'999);
  std::ostringstream script;
  script << "CREATE t 4 int string double int id symbol price qty\n"
         << "INSERT INTO t " << rows << " ROWS\n";
  for (std::size_t row = 0; row < rows; ++row) {
    script << row << " SYM" << symbol(rng) << ' ' << cents(rng) / 100.0 << ' ' << 100 << '\n';
  }
  script << "QUIT\n";
  return script.str();
}

/// INSERT of N rows from command text: tokenizing, literal parsing and columnar append.
void insertThroughParser(benchmark::State& state) {
  const auto rows = static_cast<std::size_t>(state.range(0));
  const std::string script = insertScript(rows);
  for (auto _ : state) {
    std::istringstream in(script);
    std::ostringstream out;
    Session session(in, out, out, {.quiet = true, .timing = false});
    benchmark::DoNotOptimize(session.run());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rows));
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(script.size()));
}

/// A batch holding a copy of every row of `source`.
RowBatch copyOf(const Table& source) {
  RowBatch batch = source.emptyBatch();
  for (std::size_t column = 0; column < source.columnCount(); ++column) {
    Column copy = source.column(column);
    batch.columns[column].appendFrom(std::move(copy));
  }
  batch.rows = source.rowCount();
  return batch;
}

/// Table::append of a prepared N-row batch to a table that already holds N rows (so every column
/// must grow and move its elements), with and without an index to maintain.
void appendBatch(benchmark::State& state, std::optional<IndexKind> index) {
  const auto rows = static_cast<std::size_t>(state.range(0));
  const Table source = makeTrades(rows);
  for (auto _ : state) {
    state.PauseTiming();
    Table table("trades", {source.schema().begin(), source.schema().end()});
    table.append(copyOf(source));
    if (index) {
      table.createIndex(*index, 1);
    }
    RowBatch batch = copyOf(source);
    state.ResumeTiming();

    table.append(std::move(batch));
    benchmark::DoNotOptimize(table.rowCount());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rows));
}

BENCHMARK(insertThroughParser)->Arg(10'000)->Arg(100'000)->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(appendBatch, no_index, std::nullopt)
    ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(appendBatch, hash_index_on_symbol, IndexKind::Hash)
    ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace sillyql::bench
