#include "sillyql/catalog.hpp"

#include <utility>

#include "sillyql/errors.hpp"

namespace sillyql {

bool Catalog::contains(std::string_view name) const {
  return tables_.contains(name);
}

const Table* Catalog::find(std::string_view name) const {
  const auto it = tables_.find(name);
  return it == tables_.end() ? nullptr : &it->second;
}

Table* Catalog::find(std::string_view name) {
  const auto it = tables_.find(name);
  return it == tables_.end() ? nullptr : &it->second;
}

const Table& Catalog::require(std::string_view verb, std::string_view name) const {
  if (const Table* table = find(name)) {
    return *table;
  }
  throw TableNotFoundError(verb, name);
}

Table& Catalog::require(std::string_view verb, std::string_view name) {
  if (Table* table = find(name)) {
    return *table;
  }
  throw TableNotFoundError(verb, name);
}

Table& Catalog::create(std::string name, std::vector<ColumnSpec> schema) {
  if (contains(name)) {
    throw TableExistsError(name);
  }
  Table table(name, std::move(schema));
  return tables_.emplace(std::move(name), std::move(table)).first->second;
}

bool Catalog::drop(std::string_view name) {
  const auto it = tables_.find(name);
  if (it == tables_.end()) {
    return false;
  }
  tables_.erase(it);
  return true;
}

}  // namespace sillyql
