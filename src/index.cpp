#include "sillyql/index.hpp"

#include "sillyql/hash_index.hpp"
#include "sillyql/ordered_index.hpp"

namespace sillyql {

std::string_view indexKindName(IndexKind kind) noexcept {
  return kind == IndexKind::Hash ? "hash" : "bst";
}

std::optional<IndexKind> parseIndexKind(std::string_view name) noexcept {
  if (name == "hash") {
    return IndexKind::Hash;
  }
  if (name == "bst") {
    return IndexKind::Ordered;
  }
  return std::nullopt;
}

std::unique_ptr<Index> makeIndex(IndexKind kind, ColumnType keyType, std::size_t column) {
  return visitStorageType(keyType, [kind, column](auto tag) -> std::unique_ptr<Index> {
    using Key = typename decltype(tag)::type;
    if (kind == IndexKind::Hash) {
      return std::make_unique<HashIndex<Key>>(column);
    }
    return std::make_unique<OrderedIndex<Key>>(column);
  });
}

}  // namespace sillyql
