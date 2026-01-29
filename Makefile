PROJECT_SOURCE_DIR := $(abspath ./)
BUILD_DIR ?= $(PROJECT_SOURCE_DIR)/build

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
    NUM_JOB := $(shell nproc)
else ifeq ($(UNAME_S), Darwin)
    NUM_JOB := $(shell sysctl -n hw.ncpu)
else
    NUM_JOB := 1
endif

clean:
	rm -rf $(BUILD_DIR) && rm -rf third_party/*build
.PHONY: clean

bot:
	cmake -S . -B $(BUILD_DIR) -G Ninja
	cmake --build $(BUILD_DIR) --target ciel_parser_bot --parallel $(NUM_JOB)
	CIELPARSER_CONFIG_PATH=$(PROJECT_SOURCE_DIR)/config.json $(BUILD_DIR)/ciel_parser_bot
.PHONY: bot

test:
	cmake -S . -B $(BUILD_DIR) -G Ninja
	cmake --build $(BUILD_DIR) --target ciel_parser_test --parallel $(NUM_JOB)
	$(BUILD_DIR)/ciel_parser_test
.PHONY: test

format:
	./format.sh $(PROJECT_SOURCE_DIR)/include $(PROJECT_SOURCE_DIR)/app $(PROJECT_SOURCE_DIR)/test
.PHONY: format
