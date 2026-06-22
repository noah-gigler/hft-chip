REPO_ROOT := $(shell git -C .. rev-parse --show-toplevel)
RTL       := $(REPO_ROOT)/rtl
SW        := $(REPO_ROOT)/sw
OBJ       := obj_dir

VERILATOR := verilator
# -Wno-fatal: traders infer latches on combinational temp vars (intentional); don't abort
VFLAGS := --cc --exe --build --trace -Wno-fatal

# random run knobs (override: make sim SEED=2 NOPS=1000000)
SEED ?= 1
NOPS ?= 100000

PKG     := $(RTL)/orderbook_pkg.sv
TRADERS := $(RTL)/arb_trader.sv $(RTL)/momentum_trader.sv $(RTL)/ema_trader.sv
# all C golden models live in sw/, sharing common.h
MODELS  := $(SW)/orderbook.c $(SW)/arb_trader.c \
           $(SW)/momentum_trader.c $(SW)/ema_trader.c

.PHONY: all sim orderbook arb momentum ema chip vectors random clean

all: orderbook arb momentum ema chip

# ---------------- orderbook ----------------
$(OBJ)/Vorderbook: $(RTL)/orderbook.sv tb_orderbook.cpp $(SW)/orderbook.c $(SW)/orderbook.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module orderbook -GN=32 -CFLAGS "-I$(SW)" \
		$(PKG) $(RTL)/orderbook.sv tb_orderbook.cpp $(SW)/orderbook.c

orderbook: sim
sim: vectors random

vectors: $(OBJ)/Vorderbook
	$(OBJ)/Vorderbook --vectors vectors/basic.vec vectors/edge.vec
random: $(OBJ)/Vorderbook
	$(OBJ)/Vorderbook --random $(SEED) $(NOPS)

# ---------------- arb_trader ----------------
$(OBJ)/Varb_trader: $(RTL)/arb_trader.sv tb_arb_trader.cpp $(SW)/arb_trader.c $(SW)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module arb_trader -CFLAGS "-I$(SW)" \
		$(PKG) $(RTL)/arb_trader.sv tb_arb_trader.cpp $(SW)/arb_trader.c

arb: $(OBJ)/Varb_trader
	$(OBJ)/Varb_trader --random $(SEED) $(NOPS)

# ---------------- momentum_trader ----------------
$(OBJ)/Vmomentum_trader: $(RTL)/momentum_trader.sv tb_momentum_trader.cpp $(SW)/momentum_trader.c $(SW)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module momentum_trader -GN=32 -CFLAGS "-I$(SW)" \
		$(PKG) $(RTL)/momentum_trader.sv tb_momentum_trader.cpp $(SW)/momentum_trader.c

momentum: $(OBJ)/Vmomentum_trader
	$(OBJ)/Vmomentum_trader --random $(SEED) $(NOPS)

# ---------------- ema_trader ----------------
$(OBJ)/Vema_trader: $(RTL)/ema_trader.sv tb_ema_trader.cpp $(SW)/ema_trader.c $(SW)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module ema_trader -CFLAGS "-I$(SW)" \
		$(PKG) $(RTL)/ema_trader.sv tb_ema_trader.cpp $(SW)/ema_trader.c

ema: $(OBJ)/Vema_trader
	$(OBJ)/Vema_trader --random $(SEED) $(NOPS)

# ---------------- whole chip ----------------
$(OBJ)/Vcroc_chip: $(RTL)/croc_chip.sv $(RTL)/hft_core.sv $(TRADERS) $(RTL)/orderbook.sv pad_stubs.sv \
                   tb_croc_chip.cpp $(MODELS) $(SW)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module croc_chip -CFLAGS "-I$(SW)" \
		$(PKG) $(RTL)/orderbook.sv $(TRADERS) $(RTL)/hft_core.sv $(RTL)/croc_chip.sv pad_stubs.sv \
		tb_croc_chip.cpp $(MODELS)

chip: $(OBJ)/Vcroc_chip
	$(OBJ)/Vcroc_chip --random $(SEED) $(NOPS)

clean:
	rm -rf $(OBJ)
