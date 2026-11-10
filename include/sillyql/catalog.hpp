#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "sillyql/table.hpp"

namespace sillyql {

/// The set of tables in the database, keyed by name.
class Catalog {
 public:
  [[nodiscard]] bool contains(std::string_view name) const;

  /// The table called `name`, or nullptr.
  [[nodiscard]] const Table* find(std::string_view name) const;
  [[nodiscard]] Table* find(std::string_view name);

  /// The table called `name`; throws TableNotFoundError attributed to `verb` otherwise.
  [[nodiscard]] const Table& require(std::string_view verb, std::string_view name) const;
  [[nodiscard]] Table& require(std::string_view verb, std::string_view name);

  /// Creates an empty table. Throws TableExistsError if the name is taken.
  Table& create(std::string name, std::vector<ColumnSpec> schema);

  /// Removes the table called `name`; returns false if there is none.
  bool drop(std::string_view name);

 private:
  /// Lets the map be probed with a std::string_view without building a std::string.
  struct NameHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view name) const noexcept {
      return std::hash<std::string_view>{}(name);
    }
  };

  std::unordered_map<std::string, Table, NameHash, std::equal_to<>> tables_;
};

}  // namespace sillyql
