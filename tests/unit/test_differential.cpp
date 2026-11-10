// Randomized differential tests (fixed seeds): the same operations applied to a table without an
// index, with a hash index and with an ordered index must select the same multiset of rows as a
// naive reference model, and each access path must honour its documented output order.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "sillyql/planner.hpp"
#include "sillyql/table.hpp"
#include "test_helpers.hpp"

namespace sillyql {
namespace {

using Row = std::tuple<std::int32_t, std::string, double, bool>;

constexpr std::array<CompareOp, 6> kOps{CompareOp::Less,         CompareOp::LessEqual,
                                        CompareOp::Equal,        CompareOp::NotEqual,
                                        CompareOp::GreaterEqual, CompareOp::Greater};
constexpr std::array<const char*, 5> kStrings{"a", "b", "c", "dd", "Z"};
constexpr std::array<double, 6> kDoubles{-1.0, 0.5, 1.0, 1.5, 2.25, 10.0};
constexpr std::size_t kColumns = 4;

class RandomWorkload {
 public:
  explicit RandomWorkload(std::uint32_t seed) : rng_(seed) {}

  std::size_t pick(std::size_t count) {
    return std::uniform_int_distribution<std::size_t>(0, count - 1)(rng_);
  }

  Row row() {
    return {static_cast<std::int32_t>(pick(12)), kStrings[pick(kStrings.size())],
            kDoubles[pick(kDoubles.size())], pick(2) == 1};
  }

  Predicate predicate() {
    const std::size_t column = pick(kColumns);
    const CompareOp op = kOps[pick(kOps.size())];
    switch (column) {
      case 0:
        return {column, op, Value(static_cast<std::int32_t>(pick(14)) - 1)};
      case 1:
        return {column, op, Value(kStrings[pick(kStrings.size())])};
      case 2:
        return {column, op, Value(kDoubles[pick(kDoubles.size())])};
      default:
        return {column, op, Value(pick(2) == 1)};
    }
  }

 private:
  std::mt19937 rng_;
};

/// The reference model's comparison, written independently of the engine's scan kernels.
template <class T>
bool evaluate(CompareOp op, const T& lhs, const T& rhs) {
  switch (op) {
    case CompareOp::Less:
      return lhs < rhs;
    case CompareOp::LessEqual:
      return lhs <= rhs;
    case CompareOp::Equal:
      return lhs == rhs;
    case CompareOp::NotEqual:
      return lhs != rhs;
    case CompareOp::GreaterEqual:
      return lhs >= rhs;
    case CompareOp::Greater:
      return lhs > rhs;
  }
  return false;
}

bool matches(const Row& row, const Predicate& where) {
  switch (where.column) {
    case 0:
      return evaluate(where.op, std::get<0>(row), where.value.get<std::int32_t>());
    case 1:
      return evaluate(where.op, std::get<1>(row), where.value.get<std::string>());
    case 2:
      return evaluate(where.op, std::get<2>(row), where.value.get<double>());
    default:
      return evaluate(where.op, std::get<3>(row), where.value.get<bool>());
  }
}

Table makeTable() {
  return Table("t", {{"k", ColumnType::Int},
                     {"s", ColumnType::String},
                     {"d", ColumnType::Double},
                     {"b", ColumnType::Bool}});
}

Row rowAt(const Table& table, RowId row) {
  return {table.column(0).values<std::int32_t>()[row], table.column(1).values<std::string>()[row],
          table.column(2).values<double>()[row], table.column(3).values<BoolStorage>()[row] != 0};
}

/// Checks the documented order: ordered range scans by (key, row id), everything else by row id.
void expectOutputOrder(const Table& table, const ScanPlan& plan, const std::vector<RowId>& rows) {
  for (std::size_t i = 1; i < rows.size(); ++i) {
    if (plan.path == AccessPath::OrderedRangeScan) {
      const Value previous = table.column(plan.predicate->column).valueAt(rows[i - 1]);
      const Value current = table.column(plan.predicate->column).valueAt(rows[i]);
      ASSERT_TRUE(previous < current || (previous == current && rows[i - 1] < rows[i]));
    } else {
      ASSERT_LT(rows[i - 1], rows[i]);
    }
  }
}

class DifferentialTest : public ::testing::TestWithParam<std::uint32_t> {};

TEST_P(DifferentialTest, AccessPathsAgreeWithReferenceModel) {
  RandomWorkload workload(GetParam());
  std::vector<Row> reference;
  std::array<Table, 3> tables{makeTable(), makeTable(), makeTable()};  // none, hash, ordered
  std::array<std::size_t, 4> pathsUsed{};

  for (int step = 0; step < 400; ++step) {
    const std::size_t action = workload.pick(10);
    if (action < 4) {
      std::vector<Row> rows(1 + workload.pick(6));
      for (Row& row : rows) {
        row = workload.row();
      }
      for (Table& table : tables) {
        RowBatch batch = table.emptyBatch();
        for (const Row& row : rows) {
          batch.columns[0].append(Value(std::get<0>(row)));
          batch.columns[1].append(Value(std::get<1>(row)));
          batch.columns[2].append(Value(std::get<2>(row)));
          batch.columns[3].append(Value(std::get<3>(row)));
        }
        batch.rows = rows.size();
        table.append(std::move(batch));
      }
      reference.insert(reference.end(), rows.begin(), rows.end());
    } else if (action < 5) {
      const std::size_t column = workload.pick(kColumns);
      tables[1].createIndex(IndexKind::Hash, column);
      tables[2].createIndex(IndexKind::Ordered, column);
    } else {
      const Predicate where = workload.predicate();
      std::vector<Row> expected;
      std::copy_if(reference.begin(), reference.end(), std::back_inserter(expected),
                   [&where](const Row& row) { return matches(row, where); });
      std::sort(expected.begin(), expected.end());

      const bool isDelete = action < 7;
      for (Table& table : tables) {
        const ScanPlan plan = planScan(table, &where);
        ++pathsUsed[static_cast<std::size_t>(plan.path)];
        const std::vector<RowId> rows = executeScan(table, plan);
        expectOutputOrder(table, plan, rows);

        std::vector<Row> actual;
        actual.reserve(rows.size());
        for (const RowId row : rows) {
          actual.push_back(rowAt(table, row));
        }
        std::sort(actual.begin(), actual.end());
        ASSERT_EQ(actual, expected) << "step " << step;

        if (isDelete) {
          ASSERT_EQ(table.erase(rows), expected.size());
        }
      }
      if (isDelete) {
        std::erase_if(reference, [&where](const Row& row) { return matches(row, where); });
      }
    }
    for (const Table& table : tables) {
      ASSERT_EQ(table.rowCount(), reference.size());
    }
  }
  // Every access path was exercised.
  for (const std::size_t uses : pathsUsed) {
    EXPECT_GT(uses, 0U);
  }
}

INSTANTIATE_TEST_SUITE_P(FixedSeeds, DifferentialTest, ::testing::Range<std::uint32_t>(1, 21));

/// Splits session output into one block per prompt.
std::vector<std::string> outputBlocks(const std::string& out) {
  std::vector<std::string> blocks;
  std::size_t start = 0;
  while (start < out.size()) {
    std::size_t next = out.find("\n% ", start);
    next = next == std::string::npos ? out.size() : next + 1;
    blocks.push_back(out.substr(start, next - start));
    start = next;
  }
  return blocks;
}

std::string sortedLines(const std::string& block) {
  std::vector<std::string> lines;
  std::istringstream in(block);
  for (std::string line; std::getline(in, line);) {
    lines.push_back(line);
  }
  std::sort(lines.begin(), lines.end());
  std::string joined;
  for (const std::string& line : lines) {
    joined += line + '\n';
  }
  return joined;
}

TEST(SessionDifferentialTest, IndexesNeverChangeWhichRowsCommandsSee) {
  RandomWorkload workload(2024);
  const std::array<const char*, kColumns> columns{"k", "s", "d", "b"};
  std::vector<std::string> commands;
  std::vector<std::size_t> generateAt;
  for (int i = 0; i < 300; ++i) {
    const std::size_t action = workload.pick(10);
    std::ostringstream command;
    if (action < 3) {
      const std::size_t count = 1 + workload.pick(4);
      command << "INSERT INTO t " << count << " ROWS\n";
      for (std::size_t r = 0; r < count; ++r) {
        const Row row = workload.row();
        command << std::get<0>(row) << ' ' << std::get<1>(row) << ' ' << std::get<2>(row) << ' '
                << (std::get<3>(row) ? "true" : "false") << '\n';
      }
    } else if (action < 4) {
      generateAt.push_back(commands.size());
      command << "GENERATE FOR t INDEXKIND INDEX ON " << columns[workload.pick(kColumns)] << '\n';
    } else {
      const Predicate where = workload.predicate();
      std::ostringstream predicate;
      predicate << columns[where.column] << ' ' << symbol(where.op) << ' ' << where.value;
      if (action < 5) {
        command << "DELETE FROM t WHERE " << predicate.str() << '\n';
      } else if (action < 6) {
        command << "AGGREGATE FROM t SUM d GROUP BY s WHERE " << predicate.str() << '\n';
      } else {
        command << "PRINT FROM t 4 d s k b WHERE " << predicate.str() << '\n';
      }
    }
    commands.push_back(command.str());
  }

  const auto runVariant = [&](const std::string& kind) {
    std::string script = "CREATE t 4 int string double bool k s d b\n";
    for (std::size_t i = 0; i < commands.size(); ++i) {
      const bool isGenerate =
          std::find(generateAt.begin(), generateAt.end(), i) != generateAt.end();
      if (!isGenerate) {
        script += commands[i];
      } else if (!kind.empty()) {
        std::string generate = commands[i];
        generate.replace(generate.find("INDEXKIND"), 9, kind);
        script += generate;
      }
    }
    std::vector<std::string> blocks = outputBlocks(test::runScript(script + "QUIT\n").out);
    std::erase_if(blocks,
                  [](const std::string& block) { return block.starts_with("% Generated"); });
    return blocks;
  };

  const std::vector<std::string> plain = runVariant("");
  ASSERT_EQ(plain.size(), commands.size() - generateAt.size() + 2);
  for (const char* kind : {"hash", "bst"}) {
    const std::vector<std::string> indexed = runVariant(kind);
    ASSERT_EQ(indexed.size(), plain.size()) << kind;
    for (std::size_t i = 0; i < plain.size(); ++i) {
      EXPECT_EQ(sortedLines(indexed[i]), sortedLines(plain[i])) << kind << " block " << i;
    }
  }
}

}  // namespace
}  // namespace sillyql
