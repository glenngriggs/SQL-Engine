// Point lookups and range queries through the planner: full scan vs hash vs ordered index.

#include <benchmark/benchmark.h>

#include <optional>
#include <random>

#include "fixtures.hpp"
#include "sillyql/planner.hpp"

namespace sillyql::bench {
namespace {

void pointLookup(benchmark::State& state, std::optional<IndexKind> index) {
  const auto rows = static_cast<std::size_t>(state.range(0));
  Table table = makeTrades(rows);
  if (index) {
    table.createIndex(*index, 0);
  }
  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<std::int32_t> key(0, static_cast<std::int32_t>(rows) - 1);
  for (auto _ : state) {
    const Predicate where{0, CompareOp::Equal, Value(key(rng))};
    const std::vector<RowId> result = executeScan(table, planScan(table, &where));
    benchmark::DoNotOptimize(result.data());
  }
  state.SetItemsProcessed(state.iterations());
}

/// `price < 10` selects about 1% of the rows.
void rangeQuery(benchmark::State& state, std::optional<IndexKind> index) {
  const auto rows = static_cast<std::size_t>(state.range(0));
  Table table = makeTrades(rows);
  if (index) {
    table.createIndex(*index, 2);
  }
  const Predicate where{2, CompareOp::Less, Value(10.0)};
  const ScanPlan plan = planScan(table, &where);
  std::size_t selected = 0;
  for (auto _ : state) {
    const std::vector<RowId> result = executeScan(table, plan);
    selected = result.size();
    benchmark::DoNotOptimize(result.data());
  }
  state.counters["selected"] = static_cast<double>(selected);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_CAPTURE(pointLookup, full_scan, std::nullopt)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(pointLookup, hash_index, IndexKind::Hash)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(pointLookup, ordered_index, IndexKind::Ordered)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_CAPTURE(rangeQuery, full_scan, std::nullopt)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(rangeQuery, ordered_index, IndexKind::Ordered)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMicrosecond);

}  // namespace
}  // namespace sillyql::bench
