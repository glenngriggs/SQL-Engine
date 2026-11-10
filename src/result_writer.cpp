#include "sillyql/result_writer.hpp"

namespace sillyql {

void ResultWriter::cell(std::string_view text) {
  if (!quiet_) {
    out_ << text << ' ';
  }
}

void ResultWriter::cell(const Column& column, RowId row) {
  if (!quiet_) {
    column.write(out_, row);
    out_ << ' ';
  }
}

void ResultWriter::cell(const Value& value) {
  if (!quiet_) {
    out_ << value << ' ';
  }
}

void ResultWriter::cell(const AggregateValue& value) {
  if (!quiet_) {
    writeAggregateValue(out_, value);
    out_ << ' ';
  }
}

void ResultWriter::endRow() {
  if (!quiet_) {
    out_ << '\n';
  }
}

}  // namespace sillyql
