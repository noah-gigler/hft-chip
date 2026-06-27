// orderbook_sorted unit test: checks the full per-level book state (all N
// levels, both sides) against the golden C model on every cycle.
#include "Vorderbook.h"
#include "tb_orderbook_common.h"

static bool check_full(Vorderbook* dut, orderbook_t* golden, const char* label) {
    bool ok = true;

    if ((bool)dut->error_o != golden->error) {
        printf("FAIL [%s] error: DUT=%d golden=%d\n", label, dut->error_o, golden->error);
        ok = false;
    }

    if (!golden->error) {
        for (int i = 0; i < ORDERBOOK_N; i++) {
            uint16_t dp = get_price(&dut->bid_prices_o[0], i);
            uint8_t  dq = get_qty (&dut->bid_qtys_o[0],   i);
            if (dp != golden->bid_prices[i] || dq != golden->bid_qtys[i]) {
                printf("FAIL [%s] bid[%2d]: DUT=(%3u,%3u) golden=(%3u,%3u)\n",
                       label, i, dp, dq, golden->bid_prices[i], golden->bid_qtys[i]);
                ok = false;
            }
            uint16_t ap = get_price(&dut->ask_prices_o[0], i);
            uint8_t  aq = get_qty (&dut->ask_qtys_o[0],    i);
            if (ap != golden->ask_prices[i] || aq != golden->ask_qtys[i]) {
                printf("FAIL [%s] ask[%2d]: DUT=(%3u,%3u) golden=(%3u,%3u)\n",
                       label, i, ap, aq, golden->ask_prices[i], golden->ask_qtys[i]);
                ok = false;
            }
        }
    }

    return ok;
}

int main(int argc, char** argv) {
    return run_orderbook_tb<Vorderbook>(argc, argv, "obj_dir/orderbook.vcd", check_full);
}
