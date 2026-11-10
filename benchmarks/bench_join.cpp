// Equi-join: probing an existing index on the right table vs building a temporary hash table.
// The strategy is forced here (rather than chosen by planJoin) so every combination is measured;
// these numbers are why the planner never probes an ordered index.

#include <benchmark/benchmark.h>

#include <optional>

#include "fixtures.hpp"
#include "sillyql/join.hpp"

namespace sillyql::bench {
namespace {

/// Joins N trades with N reference rows on id; every trade matches exactly one reference row.
void join(benchmark::State& state, std::optional<IndexKind> rightIndex) {
  const auto rows = static_cast<std::size_t>(state.range(0));
  const Table left = makeTrades(rows);
  Table right = makeTrades(rows);
  if (rightIndex) {
    right.createIndex(*rightIndex, 0);
  }
  const JoinStrategy strategy =
      rightIndex ? JoinStrategy::ProbeExistingIndex : JoinStrategy::BuildTemporaryHashTable;
  for (auto _ : state) {
    std::size_t checksum = 0;
    std::size_t matches =
        executeJoin(left, 0, right, 0, strategy,
                    [&checksum](RowId leftRow, RowId rightRow) { checksum += leftRow ^ rightRow; });
    benchmark::DoNotOptimize(matches);
    benchmark::DoNotOptimize(checksum);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rows));
}

BENCHMARK_CAPTURE(join, temporary_hash_table, std::nullopt)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(join, existing_hash_index, IndexKind::Hash)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(join, existing_ordered_index, IndexKind::Ordered)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace sillyql::bench
