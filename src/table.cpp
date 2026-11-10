#include "sillyql/table.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace sillyql {

Table::Table(std::string name, std::vector<ColumnSpec> schema)
    : name_(std::move(name)), schema_(std::move(schema)) {
  columns_.reserve(schema_.size());
  for (std::size_t i = 0; i < schema_.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      if (schema_[i].name == schema_[j].name) {
        throw std::invalid_argument("duplicate column name " + schema_[i].name);
      }
    }
    columns_.emplace_back(schema_[i].type);
  }
}

std::optional<std::size_t> Table::findColumn(std::string_view name) const noexcept {
  // Schemas are a handful of columns: a linear scan beats hashing the name.
  for (std::size_t i = 0; i < schema_.size(); ++i) {
    if (schema_[i].name == name) {
      return i;
    }
  }
  return std::nullopt;
}

RowBatch Table::emptyBatch() const {
  RowBatch batch;
  batch.columns.reserve(schema_.size());
  for (const ColumnSpec& spec : schema_) {
    batch.columns.emplace_back(spec.type);
  }
  return batch;
}

RowId Table::append(RowBatch&& batch) {
  if (batch.columns.size() != columns_.size()) {
    throw std::invalid_argument("row batch has the wrong number of columns");
  }
  for (std::size_t i = 0; i < columns_.size(); ++i) {
    if (batch.columns[i].type() != columns_[i].type() || batch.columns[i].size() != batch.rows) {
      throw std::invalid_argument("row batch does not match the schema of " + name_);
    }
  }
  if (batch.rows > std::numeric_limits<RowId>::max() - rowCount_) {
    throw std::length_error("table " + name_ + " cannot hold more rows");
  }

  // Allocate first: once every column has room, moving the batch in cannot throw, so the
  // columns never end up with different lengths.
  for (std::size_t i = 0; i < columns_.size(); ++i) {
    columns_[i].reserveFor(batch.columns[i]);
  }
  const auto first = static_cast<RowId>(rowCount_);
  for (std::size_t i = 0; i < columns_.size(); ++i) {
    columns_[i].appendFrom(std::move(batch.columns[i]));
  }
  rowCount_ += batch.rows;

  if (index_) {
    try {
      index_->extend(columns_[index_->column()], first);
    } catch (...) {
      index_->truncate(first);
      for (Column& column : columns_) {
        column.truncate(first);
      }
      rowCount_ = first;
      throw;
    }
  }
  return first;
}

std::size_t Table::erase(std::span<const RowId> rows) {
  std::vector<std::uint8_t> keep(rowCount_, 1);
  std::size_t erased = 0;
  for (const RowId row : rows) {
    std::uint8_t& flag = keep.at(row);
    erased += flag;
    flag = 0;
  }
  if (erased == 0) {
    return 0;
  }

  // Surviving rows move down to close the gaps; the index learns each row's new id instead of
  // being rebuilt, which would reallocate a postings list per key.
  std::vector<RowId> newIds;
  if (index_) {
    newIds.resize(rowCount_);
    RowId next = 0;
    for (std::size_t row = 0; row < rowCount_; ++row) {
      newIds[row] = keep[row] != 0 ? next++ : kNoRow;
    }
  }

  for (Column& column : columns_) {
    column.compact(keep);
  }
  rowCount_ -= erased;
  if (index_) {
    index_->renumber(newIds);
  }
  return erased;
}

std::size_t Table::createIndex(IndexKind kind, std::size_t column) {
  std::unique_ptr<Index> index = makeIndex(kind, spec(column).type, column);
  index->extend(columns_[column], 0);
  index_ = std::move(index);
  return index_->distinctKeys();
}

const Index* Table::indexOn(std::size_t column) const noexcept {
  return (index_ && index_->column() == column) ? index_.get() : nullptr;
}

}  // namespace sillyql
