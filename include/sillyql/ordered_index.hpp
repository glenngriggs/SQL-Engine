#pragma once

#include <map>
#include <stdexcept>
#include <vector>

#include "sillyql/postings_index.hpp"

namespace sillyql {

/// Ordered (balanced-tree) index: a std::map from key to postings list. Answers equality and
/// range predicates in O(log n + matches), returning rows in ascending key order.
template <StorageType Key>
class OrderedIndex final : public PostingsIndex<std::map<Key, std::vector<RowId>>> {
  using Base = PostingsIndex<std::map<Key, std::vector<RowId>>>;

 public:
  using Base::Base;

  [[nodiscard]] IndexKind kind() const noexcept override { return IndexKind::Ordered; }

  [[nodiscard]] bool supports(CompareOp op) const noexcept override {
    return op != CompareOp::NotEqual;
  }

  [[nodiscard]] std::vector<RowId> lookup(CompareOp op, const Value& key) const override {
    const auto& postings = this->postings();
    const auto& k = storageKey<Key>(key);
    // The operator only selects which bounds delimit the matching keys.
    auto first = postings.begin();
    auto last = postings.end();
    switch (op) {
      case CompareOp::Less:
        last = postings.lower_bound(k);
        break;
      case CompareOp::LessEqual:
        last = postings.upper_bound(k);
        break;
      case CompareOp::Equal:
        first = postings.lower_bound(k);
        last = postings.upper_bound(k);
        break;
      case CompareOp::GreaterEqual:
        first = postings.lower_bound(k);
        break;
      case CompareOp::Greater:
        first = postings.upper_bound(k);
        break;
      case CompareOp::NotEqual:
        throw std::logic_error("ordered index does not answer != lookups");
    }
    std::vector<RowId> rows;
    for (auto it = first; it != last; ++it) {
      rows.insert(rows.end(), it->second.begin(), it->second.end());
    }
    return rows;
  }
};

}  // namespace sillyql
