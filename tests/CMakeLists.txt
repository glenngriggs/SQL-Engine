include(GoogleTest)

add_executable(sillyql_unit_tests
  unit/test_aggregate.cpp
  unit/test_catalog.cpp
  unit/test_cli_options.cpp
  unit/test_column.cpp
  unit/test_csv_loader.cpp
  unit/test_differential.cpp
  unit/test_executor.cpp
  unit/test_hash_index.cpp
  unit/test_join.cpp
  unit/test_literal.cpp
  unit/test_ordered_index.cpp
  unit/test_parser.cpp
  unit/test_planner.cpp
  unit/test_postings_index.cpp
  unit/test_result_writer.cpp
  unit/test_scan.cpp
  unit/test_session.cpp
  unit/test_table.cpp
  unit/test_token_stream.cpp
  unit/test_value.cpp)
target_link_libraries(sillyql_unit_tests PRIVATE sillyql::sillyql GTest::gtest_main)
target_compile_definitions(sillyql_unit_tests PRIVATE
  SILLYQL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/integration/data")
sillyql_set_warnings(sillyql_unit_tests)
gtest_discover_tests(sillyql_unit_tests DISCOVERY_TIMEOUT 60)

add_subdirectory(integration)
