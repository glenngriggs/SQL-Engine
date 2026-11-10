#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "sillyql/hash_index.hpp"
#include "sillyql/ordered_index.hpp"
#include "sillyql/planner.hpp"
#include "sillyql/table.hpp"

/// Equi-join execution.
namespace sillyql {

namespace detail {

/// For each probe row in order, emits the rows the index holds under the same key.
template <class TypedIndex, class Key, class Emit>
void probeIndex(const std::vector<Key>& probe, const TypedIndex& index, Emit& emit) {
  for (std::size_t row = 0; row < probe.size(); ++row) {
    for (const RowId match : index.equalRange(probe[row])) {
      emit(static_cast<RowId>(row), match);
    }
  }
}

/// Builds a temporary hash table over `build` and probes it with every row of `probe`.
///
/// The table maps each distinct key to its first build row, and `next` chains the build rows
/// that share a key: one hash node per distinct key, no per-key vector, and string keys are
/// views into the build column (which cannot change during the join) rather than copies.
template <class Key, class Emit>
void buildAndProbe(const std::vector<Key>& probe, const std::vector<Key>& build, Emit& emit) {
  using HashKey = std::conditional_t<std::is_same_v<Key, std::string>, std::string_view, Key>;
  std::unordered_map<HashKey, RowId> first;
  first.reserve(build.size());
  std::vector<RowId> next(build.size(), kNoRow);
  // Inserting rows from last to first at the front of their chain leaves every chain in
  // ascending row order.
  for (std::size_t row = build.size(); row-- > 0;) {
    const auto [it, inserted] = first.try_emplace(HashKey(build[row]), static_cast<RowId>(row));
    if (!inserted) {
      next[row] = it->second;
      it->second = static_cast<RowId>(row);
    }
  }
  for (std::size_t row = 0; row < probe.size(); ++row) {
    const auto it = first.find(HashKey(probe[row]));
    if (it == first.end()) {
      continue;
    }
    for (RowId match = it->second; match != kNoRow; match = next[match]) {
      emit(static_cast<RowId>(row), match);
    }
  }
}

}  // namespace detail

/// Joins `left` and `right` on `left.leftColumn = right.rightColumn` (columns of the same type)
/// and calls `onMatch(leftRow, rightRow)` for every matching pair. Returns the number of pairs.
///
/// Output order is identical for both strategies: left rows in row order and, for each, the
/// matching right rows in ascending row order (index postings and the temporary table's chains
/// are both in row order). Both loops run on the raw typed key vectors; probing an existing index
/// recovers its concrete type with one dynamic_cast per join, not a virtual call per row.
template <class OnMatch>
std::size_t executeJoin(const Table& left, std::size_t leftColumn, const Table& right,
                        std::size_t rightColumn, JoinStrategy strategy, OnMatch onMatch) {
  std::size_t matches = 0;
  auto emit = [&](RowId leftRow, RowId rightRow) {
    onMatch(leftRow, rightRow);
    ++matches;
  };
  left.column(leftColumn).visit([&](const auto& probe) {
    using Key = ElementOf<decltype(probe)>;
    if (strategy == JoinStrategy::BuildTemporaryHashTable) {
      detail::buildAndProbe(probe, right.column(rightColumn).values<Key>(), emit);
      return;
    }
    const Index* index = right.indexOn(rightColumn);
    if (const auto* hash = dynamic_cast<const HashIndex<Key>*>(index)) {
      detail::probeIndex(probe, *hash, emit);
    } else if (const auto* ordered = dynamic_cast<const OrderedIndex<Key>*>(index)) {
      detail::probeIndex(probe, *ordered, emit);
    } else {
      throw std::logic_error("no index on the join column to probe");
    }
  });
  return matches;
}

}  // namespace sillyql
