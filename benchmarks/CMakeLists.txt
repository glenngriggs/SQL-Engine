add_executable(sillyql_benchmarks
  bench_insert.cpp
  bench_join.cpp
  bench_layout.cpp
  bench_lookup.cpp)
target_link_libraries(sillyql_benchmarks PRIVATE sillyql::sillyql benchmark::benchmark_main)
sillyql_set_warnings(sillyql_benchmarks)
