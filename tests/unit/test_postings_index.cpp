// Maintenance shared by both index kinds (PostingsIndex): extend, renumber after a delete,
// truncate (rollback) and typed equality probes, run against HashIndex and OrderedIndex alike.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "sillyql/hash_index.hpp"
#include "sillyql/ordered_index.hpp"

namespace sillyql {
namespace {

template <class IndexType>
class PostingsIndexTest : public ::testing::Test {
 protected:
  /// Keys 7, 8, 7, 9, 7 at rows 0..4.
  PostingsIndexTest() {
    for (const std::int32_t key : {7, 8, 7, 9, 7}) {
      keys_.append(Value(key));
    }
    index_.extend(keys_, 0);
  }

  [[nodiscard]] std::vector<RowId> rowsOf(std::int32_t key) const {
    const std::span<const RowId> rows = index_.equalRange(key);
    return {rows.begin(), rows.end()};
  }

  Column keys_{ColumnType::Int};
  IndexType index_{0};
};

using IndexTypes = ::testing::Types<HashIndex<std::int32_t>, OrderedIndex<std::int32_t>>;
TYPED_TEST_SUITE(PostingsIndexTest, IndexTypes);

TYPED_TEST(PostingsIndexTest, EqualRangeIsTypedAndInRowOrder) {
  EXPECT_EQ(this->index_.distinctKeys(), 3U);
  EXPECT_EQ(this->rowsOf(7), (std::vector<RowId>{0, 2, 4}));
  EXPECT_EQ(this->rowsOf(9), (std::vector<RowId>{3}));
  EXPECT_TRUE(this->rowsOf(5).empty());
}

TYPED_TEST(PostingsIndexTest, ExtendIndexesOnlyNewRows) {
  this->keys_.append(Value(8));
  this->keys_.append(Value(10));
  this->index_.extend(this->keys_, 5);
  EXPECT_EQ(this->rowsOf(8), (std::vector<RowId>{1, 5}));
  EXPECT_EQ(this->rowsOf(10), (std::vector<RowId>{6}));
  EXPECT_EQ(this->index_.distinctKeys(), 4U);
}

TYPED_TEST(PostingsIndexTest, RenumberDropsDeletedRowsAndShiftsTheRest) {
  // Delete rows 0 and 3: rows 1, 2, 4 become 0, 1, 2. Key 9 loses its only row.
  const std::vector<RowId> newIds{kNoRow, 0, 1, kNoRow, 2};
  this->index_.renumber(newIds);
  EXPECT_EQ(this->rowsOf(7), (std::vector<RowId>{1, 2}));
  EXPECT_EQ(this->rowsOf(8), (std::vector<RowId>{0}));
  EXPECT_TRUE(this->rowsOf(9).empty());
  EXPECT_EQ(this->index_.distinctKeys(), 2U);
}

TYPED_TEST(PostingsIndexTest, RenumberMatchesARebuildFromTheCompactedColumn) {
  const std::vector<std::uint8_t> keep{1, 0, 0, 1, 1};
  const std::vector<RowId> newIds{0, kNoRow, kNoRow, 1, 2};
  this->index_.renumber(newIds);
  this->keys_.compact(keep);
  TypeParam rebuilt(0);
  rebuilt.extend(this->keys_, 0);
  for (const std::int32_t key : {7, 8, 9}) {
    EXPECT_EQ(this->index_.lookup(CompareOp::Equal, Value(key)),
              rebuilt.lookup(CompareOp::Equal, Value(key)))
        << key;
  }
  EXPECT_EQ(this->index_.distinctKeys(), rebuilt.distinctKeys());
}

TYPED_TEST(PostingsIndexTest, TruncateForgetsRowsFromTheGivenId) {
  this->index_.truncate(2);
  EXPECT_EQ(this->rowsOf(7), (std::vector<RowId>{0}));
  EXPECT_EQ(this->rowsOf(8), (std::vector<RowId>{1}));
  EXPECT_TRUE(this->rowsOf(9).empty());
  EXPECT_EQ(this->index_.distinctKeys(), 2U);
}

}  // namespace
}  // namespace sillyql
