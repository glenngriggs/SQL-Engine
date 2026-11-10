#include <gtest/gtest.h>

#include "sillyql/catalog.hpp"
#include "sillyql/errors.hpp"

namespace sillyql {
namespace {

TEST(CatalogTest, CreatesFindsAndDropsTables) {
  Catalog catalog;
  const Table& created = catalog.create("t", {{"a", ColumnType::Int}});
  EXPECT_EQ(created.name(), "t");
  EXPECT_TRUE(catalog.contains("t"));
  EXPECT_EQ(catalog.find("t"), &created);
  EXPECT_EQ(catalog.find("u"), nullptr);
  EXPECT_TRUE(catalog.drop("t"));
  EXPECT_FALSE(catalog.drop("t"));
  EXPECT_FALSE(catalog.contains("t"));
}

TEST(CatalogTest, RejectsADuplicateName) {
  Catalog catalog;
  catalog.create("t", {{"a", ColumnType::Int}});
  EXPECT_THROW(catalog.create("t", {{"b", ColumnType::Bool}}), TableExistsError);
  EXPECT_EQ(catalog.find("t")->spec(0).name, "a");
}

TEST(CatalogTest, TablesStayAtTheirAddressWhenOthersAreAdded) {
  Catalog catalog;
  const Table* first = &catalog.create("first", {{"a", ColumnType::Int}});
  for (int i = 0; i < 100; ++i) {
    catalog.create("t" + std::to_string(i), {{"a", ColumnType::Int}});
  }
  EXPECT_EQ(catalog.find("first"), first);
}

TEST(CatalogTest, RequireNamesTheCommandInItsError) {
  const Catalog catalog;
  try {
    (void)catalog.require("DELETE", "ghost");
    FAIL() << "expected TableNotFoundError";
  } catch (const TableNotFoundError& error) {
    EXPECT_STREQ(error.what(), "Error during DELETE: ghost does not name a table in the database");
  }
}

}  // namespace
}  // namespace sillyql
