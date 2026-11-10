cmake_minimum_required(VERSION 3.20)

project(SillyQL
  VERSION 2.0.0
  DESCRIPTION "In-memory columnar SQL-like database engine"
  LANGUAGES CXX)

# PROJECT_IS_TOP_LEVEL needs CMake 3.21; this works from 3.20.
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}" SILLYQL_IS_TOP_LEVEL)

# Global settings only when SillyQL is the main project, never imposed on a parent project.
if(SILLYQL_IS_TOP_LEVEL)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  endif()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()

option(SILLYQL_BUILD_TESTS "Build the unit and integration tests" ${SILLYQL_IS_TOP_LEVEL})
option(SILLYQL_BUILD_BENCHMARKS "Build the Google Benchmark suite" ${SILLYQL_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)

add_library(sillyql STATIC
  src/aggregate.cpp
  src/catalog.cpp
  src/cli_options.cpp
  src/column.cpp
  src/compare_op.cpp
  src/csv_loader.cpp
  src/errors.cpp
  src/executor.cpp
  src/index.cpp
  src/token_stream.cpp
  src/literal.cpp
  src/parser.cpp
  src/planner.cpp
  src/result_writer.cpp
  src/scan.cpp
  src/session.cpp
  src/table.cpp
  src/value.cpp)
add_library(sillyql::sillyql ALIAS sillyql)
target_include_directories(sillyql PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(sillyql PUBLIC cxx_std_20)
sillyql_set_warnings(sillyql)

add_executable(silly apps/main.cpp)
target_link_libraries(silly PRIVATE sillyql::sillyql)
sillyql_set_warnings(silly)

# `cmake --install` provides the silly binary, the static library and its headers, and a CMake
# package so other projects can use find_package(sillyql) and link sillyql::sillyql.
install(TARGETS sillyql silly
  EXPORT sillyqlTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/sillyql DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT sillyqlTargets
  FILE sillyqlConfig.cmake
  NAMESPACE sillyql::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sillyql)

if(SILLYQL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(SILLYQL_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
