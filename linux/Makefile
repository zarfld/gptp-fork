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

.PHONY: clean build
