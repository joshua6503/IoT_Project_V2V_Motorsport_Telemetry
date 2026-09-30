# Host unit tests for firmware/common (Unity + gcc).
#
#   make            fetch Unity (first time), build and run all tests
#   make clean      remove build output
#   make CC=clang   use another host compiler
#
# Unity is fetched at a pinned tag instead of being vendored in the repo.

CC        ?= gcc
UNITY_TAG ?= v2.6.1
UNITY_DIR := vendor/unity
OUT       := out

ROOT     := ../..
COMMON   := $(ROOT)/firmware/common
IFACE    := $(ROOT)/interfaces/c

CFLAGS   += -std=c99 -Wall -Wextra -Werror -pedantic -O0 -g
CPPFLAGS += -I$(COMMON)/include -I$(IFACE) -I$(UNITY_DIR)/src

SRC_COMMON := $(COMMON)/src/alert_logic.c
TESTS      := test_alert_logic

.PHONY: all test clean
all: test

test: $(addprefix $(OUT)/,$(TESTS))
	@for t in $^; do echo "== $$t"; ./$$t || exit 1; done

$(OUT)/%: %.c $(SRC_COMMON) $(UNITY_DIR)/src/unity.c | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(UNITY_DIR)/src/unity.c:
	git clone --depth 1 --branch $(UNITY_TAG) https://github.com/ThrowTheSwitch/Unity.git $(UNITY_DIR)

$(OUT):
	mkdir -p $(OUT)

clean:
	rm -rf $(OUT)
