REPO_ROOT := $(shell git -C .. rev-parse --show-toplevel)
RTL       := $(REPO_ROOT)/rtl
SW        := $(REPO_ROOT)/sw/hft
OBJ       := obj_dir

VERILATOR := verilator

.PHONY: all sim clean

all: sim

$(OBJ)/Vorderbook: $(RTL)/orderbook_pkg.sv $(RTL)/orderbook.sv \
                   tb_orderbook.cpp $(SW)/orderbook.c $(SW)/orderbook.h
	$(VERILATOR) --cc --exe --build \
		-Mdir $(OBJ) \
		--top-module orderbook \
		-pvalue+orderbook.N=32 \
		-CFLAGS "-I$(SW)" \
		$(RTL)/orderbook_pkg.sv \
		$(RTL)/orderbook.sv \
		tb_orderbook.cpp \
		$(SW)/orderbook.c

sim: $(OBJ)/Vorderbook
	$(OBJ)/Vorderbook

clean:
	rm -rf $(OBJ)
