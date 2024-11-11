# Makefile for building the gptp daemon

ARCH ?= generic

all: clean build

clean:
	@echo "Cleaning up..."
	rm -rf build

build:
	@echo "Building for $(ARCH)..."
	mkdir -p build
	cd build && cmake .. && make

build-with-iphlpapi:
	@echo "Building with IP Helper API for $(ARCH)..."
	mkdir -p build
	cd build && cmake -DIPHLAPI=ON .. && make

static-analysis:
	@echo "Running static code analysis..."
	cppcheck --enable=all --inconclusive --quiet .
	clang-tidy **/*.cpp -- -I.

resource-checks:
	@echo "Running resource management checks..."
	valgrind --leak-check=full ./daemon_cl

.PHONY: clean build build-with-iphlpapi static-analysis resource-checks
