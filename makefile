CC = gcc
PAPI_HOME = $(HOME)/papi-local

CFLAGS = -O2 -Wall -Wextra -std=c11 -Iinclude -I$(PAPI_HOME)/include -pthread
LDFLAGS = -L$(PAPI_HOME)/lib -Wl,-rpath,$(PAPI_HOME)/lib -lpapi -pthread

SRC = src/bench.c src/bst_seq.c src/papi_util.c
OBJ = $(SRC:.c=.o)

all: bench

bench: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) bench

