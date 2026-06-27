REPO_ROOT := $(shell git -C .. rev-parse --show-toplevel)
RTL       := $(REPO_ROOT)/rtl
SW        := $(REPO_ROOT)/sw
OBJ       := obj_dir

VERILATOR := verilator
# -Wno-fatal: traders infer latches on combinational temp vars (intentional); don't abort
VFLAGS := --cc --exe --build --trace --assert -Wno-fatal

# random run knobs (NOPS=1000000)
SEED ?= 0
NOPS ?= 100000

PKG     := $(RTL)/orderbook_pkg.sv
TRADERS := $(RTL)/arb_trader.sv $(RTL)/momentum_trader.sv $(RTL)/ema_trader.sv
# all C golden models live in sw/, sharing common.h
MODELS  := $(SW)/orderbook.c $(SW)/arb_trader.c \
           $(SW)/momentum_trader.c $(SW)/ema_trader.c

.PHONY: all sim orderbook arb momentum ema chip vectors random clean

all: orderbook arb momentum ema chip

# ---------------- orderbook ----------------
# orderbook_sorted: unit test checks full book state directly.
# orderbook_unsorted: the impl that tapes out; checked directly against the
# same golden model (top-of-book + qty sums only, see tb_orderbook_unsorted.cpp).
# (--prefix keeps the Vorderbook* class names the cpps expect)
$(OBJ)/Vorderbook: $(RTL)/orderbook_sorted.sv tb_orderbook.cpp tb_orderbook_common.h \
                    $(SW)/orderbook.c $(SW)/orderbook.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module orderbook_sorted --prefix Vorderbook -GN=64 -CFLAGS "-I$(abspath $(SW))" \
		$(PKG) $(RTL)/orderbook_sorted.sv tb_orderbook.cpp $(SW)/orderbook.c

$(OBJ)/Vorderbook_unsorted: $(RTL)/orderbook_unsorted.sv tb_orderbook_unsorted.cpp tb_orderbook_common.h \
                    $(SW)/orderbook.c $(SW)/orderbook.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module orderbook_unsorted --prefix Vorderbook_unsorted -GN=64 -CFLAGS "-I$(abspath $(SW))" \
		$(PKG) $(RTL)/orderbook_unsorted.sv tb_orderbook_unsorted.cpp $(SW)/orderbook.c

orderbook: sim
sim: vectors random

vectors: $(OBJ)/Vorderbook $(OBJ)/Vorderbook_unsorted
	$(OBJ)/Vorderbook --vectors vectors/basic.vec vectors/edge.vec vectors/edge_sorted.vec
	$(OBJ)/Vorderbook_unsorted --vectors vectors/basic.vec vectors/edge.vec
random: $(OBJ)/Vorderbook $(OBJ)/Vorderbook_unsorted
	$(OBJ)/Vorderbook --random $(SEED) $(NOPS)
	$(OBJ)/Vorderbook_unsorted --random $(SEED) $(NOPS)

# ---------------- arb_trader ----------------
$(OBJ)/Varb_trader: $(RTL)/arb_trader.sv tb_arb_trader.cpp $(SW)/arb_trader.c $(SW)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module arb_trader -CFLAGS "-I$(abspath $(SW))" \
		$(PKG) $(RTL)/arb_trader.sv tb_arb_trader.cpp $(SW)/arb_trader.c

arb: $(OBJ)/Varb_trader
	$(OBJ)/Varb_trader --random $(SEED) $(NOPS)

# ---------------- momentum_trader ----------------
$(OBJ)/Vmomentum_trader: $(RTL)/momentum_trader.sv tb_momentum_trader.cpp $(SW)/momentum_trader.c $(SW)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module momentum_trader -GN=64 -CFLAGS "-I$(abspath $(SW))" \
		$(PKG) $(RTL)/momentum_trader.sv tb_momentum_trader.cpp $(SW)/momentum_trader.c

momentum: $(OBJ)/Vmomentum_trader
	$(OBJ)/Vmomentum_trader --random $(SEED) $(NOPS)

# ---------------- ema_trader ----------------
$(OBJ)/Vema_trader: $(RTL)/ema_trader.sv tb_ema_trader.cpp $(SW)/ema_trader.c $(SW)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module ema_trader -CFLAGS "-I$(abspath $(SW))" \
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
		--top-module hft_chip -GOB_UNSORTED=$(OB_UNSORTED) -CFLAGS "-I$(abspath $(SW))" \
		$(PKG) $(OB_RTL) $(TRADERS) $(RTL)/hft_core.sv $(RTL)/hft_chip.sv pad_stubs.sv \
		tb_hft_chip.cpp $(MODELS)

chip: $(OBJ)/Vhft_chip
	$(OBJ)/Vhft_chip --random $(SEED) $(NOPS)

# directed mean-reversion scenario
SCEN_NOPS ?= 2000
ema_scenario: $(OBJ)/Vhft_chip
	$(OBJ)/Vhft_chip --scenario ema $(SEED) $(SCEN_NOPS) ema_scenario.csv

# directed cross-market arbitrage scenario
arb_scenario: $(OBJ)/Vhft_chip
	$(OBJ)/Vhft_chip --scenario arb $(SEED) $(SCEN_NOPS) arb_scenario.csv

# directed order-flow-imbalance-leads-price momentum scenario
momentum_scenario: $(OBJ)/Vhft_chip
	$(OBJ)/Vhft_chip --scenario momentum $(SEED) $(SCEN_NOPS) momentum_scenario.csv

clean:
	rm -rf $(OBJ)
