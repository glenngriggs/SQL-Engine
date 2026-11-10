#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "sillyql/table.hpp"

namespace sillyql::bench {

inline constexpr std::uint32_t kSeed = 42;

/// A table of `rows` trades: id (a random permutation of 0..rows-1, so every id is unique),
/// symbol (one of 64), price (uniform in [0, 1000)) and qty.
inline Table makeTrades(std::size_t rows) {
  Table table("trades", {{"id", ColumnType::Int},
                         {"symbol", ColumnType::String},
                         {"price", ColumnType::Double},
                         {"qty", ColumnType::Int}});
  std::mt19937 rng(kSeed);
  std::vector<std::int32_t> ids(rows);
  std::iota(ids.begin(), ids.end(), 0);
  std::shuffle(ids.begin(), ids.end(), rng);
  std::uniform_real_distribution<double> price(0.0, 1000.0);
  std::uniform_int_distribution<std::int32_t> qty(1, 1000);
  std::uniform_int_distribution<int> symbol(0, 63);

  RowBatch batch = table.emptyBatch();
  for (Column& column : batch.columns) {
    column.reserve(rows);
  }
  for (const std::int32_t id : ids) {
    batch.columns[0].append(Value(id));
    batch.columns[1].append(Value("SYM" + std::to_string(symbol(rng))));
    batch.columns[2].append(Value(price(rng)));
    batch.columns[3].append(Value(qty(rng)));
  }
  batch.rows = rows;
  table.append(std::move(batch));
  return table;
}

}  // namespace sillyql::bench
