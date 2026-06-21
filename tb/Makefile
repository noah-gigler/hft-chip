REPO_ROOT := $(shell git -C .. rev-parse --show-toplevel)
RTL       := $(REPO_ROOT)/rtl
SW        := $(REPO_ROOT)/sw/hft
OBJ       := obj_dir

VERILATOR := verilator

# random run knobs (override: make sim SEED=2 NOPS=1000000)
SEED ?= 1
NOPS ?= 100000

.PHONY: all sim vectors random clean

all: sim

$(OBJ)/Vorderbook: $(RTL)/orderbook_pkg.sv $(RTL)/orderbook.sv \
                   tb_orderbook.cpp $(SW)/orderbook.c $(SW)/orderbook.h
	$(VERILATOR) --cc --exe --build --trace \
		-Mdir $(OBJ) \
		--top-module orderbook \
		-GN=32 \
		-CFLAGS "-I$(SW)" \
		$(RTL)/orderbook_pkg.sv \
		$(RTL)/orderbook.sv \
		tb_orderbook.cpp \
		$(SW)/orderbook.c

# directed vector files + constrained-random run
sim: vectors random

vectors: $(OBJ)/Vorderbook
	$(OBJ)/Vorderbook --vectors vectors/basic.vec vectors/edge.vec

random: $(OBJ)/Vorderbook
	$(OBJ)/Vorderbook --random $(SEED) $(NOPS)

clean:
	rm -rf $(OBJ)
