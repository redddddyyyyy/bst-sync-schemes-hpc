CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -pthread -Iinclude
LDFLAGS := -pthread

USE_PAPI ?= 0
PAPI_HOME ?= $(HOME)/papi-local

ifeq ($(USE_PAPI),1)
CFLAGS  += -DUSE_PAPI=1 -I$(PAPI_HOME)/include
LDFLAGS += -L$(PAPI_HOME)/lib -Wl,-rpath,$(PAPI_HOME)/lib -lpapi
else
CFLAGS  += -DUSE_PAPI=0
endif

BIN := bin
SRC := src

COMMON := $(SRC)/bst_seq.c $(SRC)/papi_util.c $(SRC)/workload.c

PROGS := $(BIN)/bench $(BIN)/bst_cg $(BIN)/bst_rwlock $(BIN)/bst_ideal

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/bench: $(BIN) $(SRC)/bench.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bench.c $(COMMON) $(LDFLAGS)

$(BIN)/bst_cg: $(BIN) $(SRC)/bst_cg.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_cg.c $(COMMON) $(LDFLAGS)

$(BIN)/bst_rwlock: $(BIN) $(SRC)/bst_rwlock.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_rwlock.c $(COMMON) $(LDFLAGS)

$(BIN)/bst_ideal: $(BIN) $(SRC)/bst_ideal.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_ideal.c $(COMMON) $(LDFLAGS)

clean:
	rm -rf $(BIN) *.o

TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,$(BIN)/tests/%,$(TEST_SRCS))

$(BIN)/tests:
	mkdir -p $(BIN)/tests

$(BIN)/tests/%: tests/%.c $(COMMON) | $(BIN)/tests
	$(CC) $(CFLAGS) -Itests -o $@ $< $(COMMON) $(LDFLAGS)

tests: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== Running $$t =="; $$t; done
	@echo "All tests passed."

.PHONY: tests
