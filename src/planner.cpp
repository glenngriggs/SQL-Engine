#include "sillyql/planner.hpp"

#include <sstream>

#include "sillyql/scan.hpp"

namespace sillyql {

ScanPlan planScan(const Table& table, const Predicate* predicate) {
  ScanPlan plan{AccessPath::FullScan, predicate};
  if (predicate == nullptr) {
    return plan;
  }
  const Index* index = table.indexOn(predicate->column);
  if (index == nullptr || !index->supports(predicate->op)) {
    return plan;
  }
  if (index->kind() == IndexKind::Hash) {
    plan.path = AccessPath::HashProbe;
  } else {
    plan.path = predicate->op == CompareOp::Equal ? AccessPath::OrderedPointLookup
                                                  : AccessPath::OrderedRangeScan;
  }
  return plan;
}

std::vector<RowId> executeScan(const Table& table, const ScanPlan& plan) {
  if (plan.predicate == nullptr) {
    return allRows(table.rowCount());
  }
  const Predicate& where = *plan.predicate;
  if (plan.path == AccessPath::FullScan) {
    return scanColumn(table.column(where.column), where.op, where.value);
  }
  return table.indexOn(where.column)->lookup(where.op, where.value);
}

std::string describe(const Table& table, const ScanPlan& plan) {
  std::ostringstream text;
  text << "Plan: ";
  if (plan.predicate == nullptr) {
    text << "full scan of " << table.name() << " (no filter)";
    return text.str();
  }
  const Predicate& where = *plan.predicate;
  const std::string& column = table.spec(where.column).name;
  switch (plan.path) {
    case AccessPath::FullScan:
      text << "full scan of " << table.name() << " (filter: " << column << ' ' << symbol(where.op)
           << ' ' << where.value << ')';
      break;
    case AccessPath::HashProbe:
      text << "hash-index probe on " << column << " (=)";
      break;
    case AccessPath::OrderedPointLookup:
      text << "ordered-index point lookup on " << column << " (=)";
      break;
    case AccessPath::OrderedRangeScan:
      text << "ordered-index range scan on " << column << " (" << symbol(where.op) << ')';
      break;
  }
  return text.str();
}

JoinStrategy planJoin(const Table& right, std::size_t rightColumn) noexcept {
  // Only a hash index is worth probing: one expected O(1) probe per left row. Probing an ordered
  // index costs O(log n) cache-missing tree steps per left row, which measured slower than
  // building a temporary hash table at every size benchmarked (benchmarks/bench_join.cpp).
  const Index* index = right.indexOn(rightColumn);
  return index != nullptr && index->kind() == IndexKind::Hash
             ? JoinStrategy::ProbeExistingIndex
             : JoinStrategy::BuildTemporaryHashTable;
}

std::string describeJoin(const Table& right, std::size_t rightColumn, JoinStrategy strategy) {
  const std::string target = right.name() + '.' + right.spec(rightColumn).name;
  const Index* index = right.indexOn(rightColumn);
  if (strategy == JoinStrategy::ProbeExistingIndex) {
    return "Plan: hash join probing existing " + std::string(index->name()) + " index on " + target;
  }
  std::string plan = "Plan: hash join building temporary hash table on " + target;
  if (index != nullptr) {
    plan += " (existing " + std::string(index->name()) + " index not probed)";
  }
  return plan;
}

}  // namespace sillyql
