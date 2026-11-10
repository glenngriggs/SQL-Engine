# Convenience wrapper around the CMake presets (see CMakePresets.json).
#   make          build the release preset
#   make test     build and run every test (release)
#   make asan     build and run every test under AddressSanitizer + UBSan
#   make bench    build and run the benchmarks (release)
#   make format   apply .clang-format to all sources
#   make lint     check formatting and run clang-tidy (as CI does)
#   make clean    remove all build trees

JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
SOURCES := $(shell find include src apps tests benchmarks -name '*.cpp' -o -name '*.hpp')

.PHONY: all release test asan bench format lint clean

all: release

build/%/CMakeCache.txt:
	cmake --preset $*

release: build/release/CMakeCache.txt
	cmake --build --preset release --parallel $(JOBS)

test: release
	ctest --preset release --parallel $(JOBS)

asan: build/asan/CMakeCache.txt
	cmake --build --preset asan --parallel $(JOBS)
	ctest --preset asan --parallel $(JOBS)

bench: release
	./build/release/bin/sillyql_benchmarks

format:
	clang-format -i $(SOURCES)

lint: build/release/CMakeCache.txt
	clang-format --dry-run --Werror $(SOURCES)
	find src apps tests/unit benchmarks -name '*.cpp' \
	  | xargs -P $(JOBS) -n 1 clang-tidy -p build/release --quiet --warnings-as-errors='*'

clean:
	rm -rf build
