CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -pthread -Iinclude
LDFLAGS := -pthread

USE_PAPI ?= 0
PAPI_HOME ?= $(HOME)/papi-local

ifeq ($(USE_PAPI),1)
CFLAGS  += -DUSE_PAPI=1 -I$(PAPI_HOME)/include
LDFLAGS += -L$(PAPI_HOME)/lib -Wl,-rpath,$(PAPI_HOME)/lib -lpapi
endif

BIN := bin
SRC := src

COMMON := $(SRC)/bst_seq.c $(SRC)/papi_util.c

PROGS := $(BIN)/bench $(BIN)/bst_cg $(BIN)/bst_fg $(BIN)/bst_ideal

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/bench: $(BIN) $(SRC)/bench.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bench.c $(COMMON) $(LDFLAGS)

$(BIN)/bst_cg: $(BIN) $(SRC)/bst_cg.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_cg.c $(COMMON) $(LDFLAGS)

$(BIN)/bst_fg: $(BIN) $(SRC)/bst_fg.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_fg.c $(COMMON) $(LDFLAGS)

$(BIN)/bst_ideal: $(BIN) $(SRC)/bst_ideal.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC)/bst_ideal.c $(COMMON) $(LDFLAGS)

clean:
	rm -rf $(BIN) *.o

