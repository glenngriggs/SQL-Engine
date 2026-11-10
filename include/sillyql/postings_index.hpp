#pragma once

#include <cstddef>
#include <iterator>
#include <span>
#include <vector>

#include "sillyql/index.hpp"

namespace sillyql {

/// The state and maintenance shared by both index kinds: a map from key to its postings list.
/// `Map` is std::unordered_map<Key, std::vector<RowId>> or std::map<Key, std::vector<RowId>>;
/// the derived classes add only what differs, which lookups the container can answer.
template <class Map>
class PostingsIndex : public Index {
 public:
  using Key = typename Map::key_type;
  using Index::Index;

  /// The rows whose key equals `key`, in ascending order, without copying. Typed, so a caller
  /// that already holds raw keys (the join) needs no Value and no virtual call per probe.
  [[nodiscard]] std::span<const RowId> equalRange(const Key& key) const {
    const auto it = postings_.find(key);
    if (it == postings_.end()) {
      return {};
    }
    return it->second;
  }

  void extend(const Column& column, RowId firstRow) override {
    const std::vector<Key>& keys = column.values<Key>();
    for (std::size_t row = firstRow; row < keys.size(); ++row) {
      postings_[keys[row]].push_back(static_cast<RowId>(row));
    }
  }

  void renumber(std::span<const RowId> newIds) noexcept override {
    for (auto it = postings_.begin(); it != postings_.end();) {
      std::vector<RowId>& rows = it->second;
      std::size_t kept = 0;
      for (const RowId row : rows) {
        if (newIds[row] != kNoRow) {
          rows[kept++] = newIds[row];
        }
      }
      rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(kept), rows.end());
      it = rows.empty() ? postings_.erase(it) : std::next(it);
    }
  }

  void truncate(RowId rowCount) noexcept override {
    for (auto it = postings_.begin(); it != postings_.end();) {
      std::vector<RowId>& rows = it->second;
      while (!rows.empty() && rows.back() >= rowCount) {
        rows.pop_back();
      }
      it = rows.empty() ? postings_.erase(it) : std::next(it);
    }
  }

  [[nodiscard]] std::size_t distinctKeys() const noexcept override { return postings_.size(); }

 protected:
  [[nodiscard]] const Map& postings() const noexcept { return postings_; }

 private:
  Map postings_;
};

}  // namespace sillyql
