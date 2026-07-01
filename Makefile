CC      := gcc
CXX     := g++
CFLAGS  := -O2 -Wall -Wextra -std=c11 -pthread -Iinclude
CXXFLAGS := -O2 -Wall -Wextra -std=c++17 -pthread -Iinclude
LDFLAGS := -pthread -lstdc++

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

COMMON_C   := $(SRC)/bst_seq.c $(SRC)/papi_util.c $(SRC)/workload.c
COMMON_CXX := $(SRC)/bst_handover.cpp
COMMON     := $(COMMON_C) $(COMMON_CXX)

PROGS := $(BIN)/bench $(BIN)/bst_cg $(BIN)/bst_rwlock $(BIN)/bst_ideal

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

C_OBJS   := $(COMMON_C:.c=.o)
CXX_OBJS := $(COMMON_CXX:.cpp=.o)
ALL_OBJS := $(C_OBJS) $(CXX_OBJS)

$(BIN)/bench: $(BIN) $(SRC)/bench.c $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bench.c $(ALL_OBJS) $(LDFLAGS)

$(BIN)/bst_cg: $(BIN) $(SRC)/bst_cg.c $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_cg.c $(ALL_OBJS) $(LDFLAGS)

$(BIN)/bst_rwlock: $(BIN) $(SRC)/bst_rwlock.c $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_rwlock.c $(ALL_OBJS) $(LDFLAGS)

$(BIN)/bst_ideal: $(BIN) $(SRC)/bst_ideal.c $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_ideal.c $(ALL_OBJS) $(LDFLAGS)

clean:
	rm -rf $(BIN) *.o src/*.o

TEST_SRCS_C    := $(wildcard tests/test_*.c)
TEST_SRCS_CXX  := $(wildcard tests/test_*.cpp)
TEST_BINS_C    := $(patsubst tests/%.c,$(BIN)/tests/%,$(TEST_SRCS_C))
TEST_BINS_CXX  := $(patsubst tests/%.cpp,$(BIN)/tests/%,$(TEST_SRCS_CXX))
TEST_BINS      := $(TEST_BINS_C) $(TEST_BINS_CXX)

$(BIN)/tests:
	mkdir -p $(BIN)/tests

$(BIN)/tests/%: tests/%.c $(ALL_OBJS) | $(BIN)/tests
	$(CC) $(CFLAGS) -Itests -o $@ $< $(ALL_OBJS) $(LDFLAGS)

$(BIN)/tests/%: tests/%.cpp $(ALL_OBJS) | $(BIN)/tests
	$(CXX) $(CXXFLAGS) -Itests -o $@ $< $(ALL_OBJS) $(LDFLAGS)

tests: $(BIN)/bench $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== Running $$t =="; $$t; done
	@set -e; for sh in tests/test_*.sh; do \
		if [ -x "$$sh" ]; then echo "== Running $$sh =="; "$$sh"; fi; \
	done
	@echo "All tests passed."

.PHONY: tests

# ThreadSanitizer build (requires recent gcc/clang)
TSAN_FLAGS := -fsanitize=thread -g -O1
tsan: clean
	$(MAKE) USE_PAPI=0 \
	  CFLAGS="$(CFLAGS) $(TSAN_FLAGS)" \
	  CXXFLAGS="$(CXXFLAGS) $(TSAN_FLAGS)" \
	  LDFLAGS="$(LDFLAGS) -fsanitize=thread" \
	  $(BIN)/tests/test_handover_stress
	@echo "=== Running test_handover_stress under ThreadSanitizer ==="
	@# setarch -R disables ASLR: on kernels with high-entropy mmap layouts,
	@# ThreadSanitizer's runtime can fail to start with "unexpected memory
	@# mapping" before any of our code runs. This does not change what
	@# ThreadSanitizer checks for races; it only fixes process startup.
	@TSAN_OPTIONS=halt_on_error=1 setarch $$(uname -m) -R $(BIN)/tests/test_handover_stress
	@echo "=== TSan run finished cleanly ==="

.PHONY: tsan
