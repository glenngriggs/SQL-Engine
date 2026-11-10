// Why columnar: a typed scan over one contiguous column vs the same filter over rows stored as
// vectors of variant values, a model of the original implementation's row-of-tagged-unions
// layout. A third variant separates the two effects: the same typed column with a branch per row
// (layout only) vs branch-free selection (layout plus kernel).

#include <benchmark/benchmark.h>

#include <variant>
#include <vector>

#include "fixtures.hpp"
#include "sillyql/scan.hpp"

namespace sillyql::bench {
namespace {

/// `price < 500`: about half the rows match, the worst case for branch prediction.
const Value kThreshold(500.0);

void columnarTypedScan(benchmark::State& state) {
  const auto rows = static_cast<std::size_t>(state.range(0));
  const Table table = makeTrades(rows);
  for (auto _ : state) {
    const std::vector<RowId> result = scanColumn(table.column(2), CompareOp::Less, kThreshold);
    benchmark::DoNotOptimize(result.data());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rows));
}

/// Each cell is read with a typed std::get_if on its variant, the cheapest access this layout
/// allows, and the loop branches per row like the original.
void rowOfVariantsScan(benchmark::State& state) {
  const auto rows = static_cast<std::size_t>(state.range(0));
  const Table table = makeTrades(rows);
  std::vector<std::vector<Value>> rowStore(rows);
  for (std::size_t row = 0; row < rows; ++row) {
    for (std::size_t column = 0; column < table.columnCount(); ++column) {
      rowStore[row].push_back(table.column(column).valueAt(static_cast<RowId>(row)));
    }
  }
  const double threshold = kThreshold.get<double>();
  for (auto _ : state) {
    std::vector<RowId> result;
    for (std::size_t row = 0; row < rowStore.size(); ++row) {
      const double* price = std::get_if<double>(&rowStore[row][2].variant());
      if (price != nullptr && *price < threshold) {
        result.push_back(static_cast<RowId>(row));
      }
    }
    benchmark::DoNotOptimize(result.data());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rows));
}

/// The same typed column with a conventional branch per row instead of branch-free selection.
void columnarBranchyScan(benchmark::State& state) {
  const auto rows = static_cast<std::size_t>(state.range(0));
  const Table table = makeTrades(rows);
  const std::vector<double>& prices = table.column(2).values<double>();
  const double threshold = kThreshold.get<double>();
  for (auto _ : state) {
    std::vector<RowId> result;
    for (std::size_t row = 0; row < prices.size(); ++row) {
      if (prices[row] < threshold) {
        result.push_back(static_cast<RowId>(row));
      }
    }
    benchmark::DoNotOptimize(result.data());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rows));
}

BENCHMARK(columnarTypedScan)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(columnarBranchyScan)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(rowOfVariantsScan)
    ->RangeMultiplier(10)
    ->Range(10'000, 1'000'000)
    ->Unit(benchmark::kMicrosecond);

}  // namespace
}  // namespace sillyql::bench
