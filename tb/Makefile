REPO_ROOT := $(shell git -C .. rev-parse --show-toplevel)
RTL       := $(REPO_ROOT)/rtl
SW        := $(REPO_ROOT)/sw
OBJ       := obj_dir

VERILATOR := verilator
# -Wno-fatal: traders infer latches on combinational temp vars (intentional); don't abort
VFLAGS := --cc --exe --build --trace --assert -Wno-fatal

# random run knobs (override: make sim SEED=2 NOPS=1000000)
SEED ?= 1
NOPS ?= 100000

PKG     := $(RTL)/orderbook_pkg.sv
TRADERS := $(RTL)/arb_trader.sv $(RTL)/momentum_trader.sv $(RTL)/ema_trader.sv
# all C golden models live in sw/, sharing common.h
MODELS  := $(SW)/orderbook.c $(SW)/arb_trader.c \
           $(SW)/momentum_trader.c $(SW)/ema_trader.c

.PHONY: all sim orderbook arb momentum ema chip equiv vectors random clean

all: orderbook arb momentum ema chip

# ---------------- orderbook ----------------
# unit test checks full book state, so it targets orderbook_sorted directly
# (--prefix keeps the Vorderbook class name the cpp expects)
$(OBJ)/Vorderbook: $(RTL)/orderbook_sorted.sv tb_orderbook.cpp $(SW)/orderbook.c $(SW)/orderbook.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module orderbook_sorted --prefix Vorderbook -GN=32 -CFLAGS "-I$(SW)" \
		$(PKG) $(RTL)/orderbook_sorted.sv tb_orderbook.cpp $(SW)/orderbook.c

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
# OB_UNSORTED selects the orderbook impl (0=sorted, 1=unsorted). Defaults to the
# taped-out config (unsorted). The chip TB stimulus avoids sentinel prices, so
# the single golden model is valid for both. Test sorted with: make clean chip OB_UNSORTED=0
OB_UNSORTED ?= 1
OB_RTL := $(RTL)/orderbook_sorted.sv $(RTL)/orderbook_unsorted.sv $(RTL)/orderbook.sv

$(OBJ)/Vhft_chip: $(RTL)/hft_chip.sv $(RTL)/hft_core.sv $(TRADERS) $(OB_RTL) pad_stubs.sv \
                   tb_hft_chip.cpp $(MODELS) $(SW)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module hft_chip -GOB_UNSORTED=$(OB_UNSORTED) -CFLAGS "-I$(SW)" \
		$(PKG) $(OB_RTL) $(TRADERS) $(RTL)/hft_core.sv $(RTL)/hft_chip.sv pad_stubs.sv \
		tb_hft_chip.cpp $(MODELS)

chip: $(OBJ)/Vhft_chip
	$(OBJ)/Vhft_chip --random $(SEED) $(NOPS)

# -------- orderbook equivalence (sorted vs unsorted, no golden model) --------
# Self-checking SV testbench: drives both impls through the wrapper with
# identical random stimulus and fails on any top-of-book divergence. +dense
# narrows the price range to 2N to stress the eviction path. No C model.
EQ_N    ?= 64
EQ_NOPS ?= 200000

$(OBJ)/Vtb_orderbook_equiv: tb_orderbook_equiv.sv $(OB_RTL) $(PKG)
	$(VERILATOR) --binary --assert -Mdir $(OBJ) \
		--top-module tb_orderbook_equiv -GN=$(EQ_N) \
		$(PKG) $(OB_RTL) tb_orderbook_equiv.sv

equiv: $(OBJ)/Vtb_orderbook_equiv
	$(OBJ)/Vtb_orderbook_equiv +nops=$(EQ_NOPS)
	$(OBJ)/Vtb_orderbook_equiv +dense +nops=$(EQ_NOPS)

# directed mean-reversion scenario
SCEN_NOPS ?= 5000
ema_scenario: $(OBJ)/Vhft_chip
	$(OBJ)/Vhft_chip --scenario ema $(SEED) $(SCEN_NOPS) ema_scenario.csv

clean:
	rm -rf $(OBJ)
