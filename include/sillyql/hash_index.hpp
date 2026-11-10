#pragma once

#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "sillyql/postings_index.hpp"

namespace sillyql {

/// Equality-only index: an unordered_map from key to postings list. O(1) expected probes.
template <StorageType Key>
class HashIndex final : public PostingsIndex<std::unordered_map<Key, std::vector<RowId>>> {
  using Base = PostingsIndex<std::unordered_map<Key, std::vector<RowId>>>;

 public:
  using Base::Base;

  [[nodiscard]] IndexKind kind() const noexcept override { return IndexKind::Hash; }

  [[nodiscard]] bool supports(CompareOp op) const noexcept override {
    return op == CompareOp::Equal;
  }

  [[nodiscard]] std::vector<RowId> lookup(CompareOp op, const Value& key) const override {
    if (!supports(op)) {
      throw std::logic_error("hash index only answers equality lookups");
    }
    const std::span<const RowId> rows = this->equalRange(storageKey<Key>(key));
    return std::vector<RowId>(rows.begin(), rows.end());
  }
};

}  // namespace sillyql
