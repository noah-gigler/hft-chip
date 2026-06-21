REPO_ROOT := $(shell git -C .. rev-parse --show-toplevel)
RTL       := $(REPO_ROOT)/rtl
SW        := $(REPO_ROOT)/sw/hft
MODEL     := model
OBJ       := obj_dir

VERILATOR := verilator
# -Wno-fatal: traders infer latches on combinational temp vars (intentional); don't abort
VFLAGS := --cc --exe --build --trace -Wno-fatal

# random run knobs (override: make sim SEED=2 NOPS=1000000)
SEED ?= 1
NOPS ?= 100000

PKG := $(RTL)/orderbook_pkg.sv

.PHONY: all sim orderbook arb momentum ema vectors random clean

all: orderbook arb momentum ema

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
$(OBJ)/Varb_trader: $(RTL)/arb_trader.sv tb_arb_trader.cpp \
                    $(MODEL)/arb_trader_model.c $(MODEL)/arb_trader_model.h $(MODEL)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module arb_trader -GN=32 -CFLAGS "-I$(CURDIR)" \
		$(PKG) $(RTL)/arb_trader.sv tb_arb_trader.cpp $(MODEL)/arb_trader_model.c

arb: $(OBJ)/Varb_trader
	$(OBJ)/Varb_trader --random $(SEED) $(NOPS)

# ---------------- momentum_trader ----------------
$(OBJ)/Vmomentum_trader: $(RTL)/momentum_trader.sv tb_momentum_trader.cpp \
                    $(MODEL)/momentum_trader_model.c $(MODEL)/momentum_trader_model.h $(MODEL)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module momentum_trader -GN=32 -CFLAGS "-I$(CURDIR)" \
		$(PKG) $(RTL)/momentum_trader.sv tb_momentum_trader.cpp $(MODEL)/momentum_trader_model.c

momentum: $(OBJ)/Vmomentum_trader
	$(OBJ)/Vmomentum_trader --random $(SEED) $(NOPS)

# ---------------- ema_trader ----------------
$(OBJ)/Vema_trader: $(RTL)/ema_trader.sv tb_ema_trader.cpp \
                    $(MODEL)/ema_trader_model.c $(MODEL)/ema_trader_model.h $(MODEL)/common.h $(PKG)
	$(VERILATOR) $(VFLAGS) -Mdir $(OBJ) \
		--top-module ema_trader -GN=32 -CFLAGS "-I$(CURDIR)" \
		$(PKG) $(RTL)/ema_trader.sv tb_ema_trader.cpp $(MODEL)/ema_trader_model.c

ema: $(OBJ)/Vema_trader
	$(OBJ)/Vema_trader --random $(SEED) $(NOPS)

clean:
	rm -rf $(OBJ)
