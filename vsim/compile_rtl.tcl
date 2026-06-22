set ROOT ".."

if {[catch { vlog -incr -sv \
    -svinputport=compat \
    "+define+TARGET_RTL" \
    "+define+TARGET_SIMULATION" \
    "+define+SYNTHESIS" \
    "+define+SIMULATION" \
    "$ROOT/rtl/orderbook_pkg.sv" \
    "$ROOT/rtl/orderbook.sv" \
    "$ROOT/rtl/arb_trader.sv" \
    "$ROOT/rtl/momentum_trader.sv" \
    "$ROOT/rtl/ema_trader.sv" \
    "$ROOT/rtl/hft_chip.sv" \
}]} {return 1}
