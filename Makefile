# Build configuration for STUN Implementation
# Based on RFC 8489 - Session Traversal Utilities for NAT

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I.
BUILD_DIR := build
SRC_DIR := src
TEST_DIR := test
INC_DIR := inc

.PHONY: all clean test install

all: BUILD_DIR $(BUILD_DIR)/stun

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/stun: $(SRC_DIR)/main.cpp $(SRC_DIR)/stun.cpp $(SRC_DIR)/http_parser.cc
	$(CXX) $(CXXFLAGS) -o $@ $^
	$(CXXFLAGS) -o $@ ../test/stun_test

test: all test/stun_test

clean:
	rm -rf $(BUILD_DIR)/*

install: BUILD_DIR 
	mkdir -p $(BUILD_DIR)/lib $(BUILD_DIR)/bin
	cp bin/stun lib/ 2>/dev/null || true

.PHONY: clean install
