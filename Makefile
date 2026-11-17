# Build configuration for STUN Implementation
# Based on RFC 8489 - Session Traversal Utilities for NAT

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I.
BUILD_DIR := build
SRC_DIR := src
TEST_DIR := test
INC_DIR := inc
SUBDIRS_DIR := .  # STUN files are at root level

.PHONY: all clean test install

all: BUILD_DIR $(BUILD_DIR)/stun

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/stun: $(SRC_DIR)/main.cpp $(SRC_DIR)/stun.cpp $(SRC_DIR)/http_parser.cc $(INC_DIR)/stun.h
	$(CXX) $(CXXFLAGS) -o $@ $^

test: all test/stun_test

test/stun_test: $(TEST_DIR)/stun_test.cpp $(TEST_DIR)/test_stun.hpp $(SRC_DIR)/main.cpp $(INC_DIR)/stun.h
	g++ $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)/*

install: BUILD_DIR 
	mkdir -p $(BUILD_DIR)/lib $(BUILD_DIR)/bin
	cp $(BUILD_DIR)/stun lib/ 2>/dev/null || true
	cp $(INC_DIR)/stun.h inc/
	cp $(INC_DIR)/http_parser.h inc/
	cp $(INC_DIR)/ipvx.h inc/

.PHONY: clean install
