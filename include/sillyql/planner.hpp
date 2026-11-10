#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sillyql/predicate.hpp"
#include "sillyql/table.hpp"

/// The query planner: chooses an access path for a filtered read and a strategy for a join.
namespace sillyql {

enum class AccessPath : std::uint8_t {
  FullScan,            ///< Typed scan of the predicate column (or every row without a filter).
  HashProbe,           ///< Hash index on the predicate column, operator '='.
  OrderedPointLookup,  ///< Ordered index on the predicate column, operator '='.
  OrderedRangeScan,    ///< Ordered index on the predicate column, operator < <= >= >.
};

struct ScanPlan {
  AccessPath path = AccessPath::FullScan;
  const Predicate* predicate = nullptr;  ///< Borrowed from the command; nullptr selects every row.
};

/// Uses the table's index when it is on the predicate column and supports the operator;
/// otherwise falls back to a full scan ('!=' always scans). The plan refers to `predicate`
/// (which may be nullptr) rather than copying it, so the predicate must outlive the plan.
[[nodiscard]] ScanPlan planScan(const Table& table, const Predicate* predicate);

/// Row ids selected by `plan`, in the engine's output order: ascending row order for full scans
/// and equality probes, ascending key order (then row order) for ordered range scans.
[[nodiscard]] std::vector<RowId> executeScan(const Table& table, const ScanPlan& plan);

/// One line for EXPLAIN, e.g. "Plan: ordered-index range scan on price (<)".
[[nodiscard]] std::string describe(const Table& table, const ScanPlan& plan);

enum class JoinStrategy : std::uint8_t {
  ProbeExistingIndex,       ///< Probe the right table's existing index on the join column.
  BuildTemporaryHashTable,  ///< Build a hash table over the right join column for this query.
};

/// Probes the right table's index on the join column if it is a hash index; otherwise (no index,
/// or an ordered index, whose per-row tree probes are slower than a hash build) builds a
/// temporary hash table. Both strategies produce identical output in identical order.
[[nodiscard]] JoinStrategy planJoin(const Table& right, std::size_t rightColumn) noexcept;

/// One line for EXPLAIN, e.g. "Plan: hash join probing existing hash index on cars.vendor".
[[nodiscard]] std::string describeJoin(const Table& right, std::size_t rightColumn,
                                       JoinStrategy strategy);

}  // namespace sillyql
