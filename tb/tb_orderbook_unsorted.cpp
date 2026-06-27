// orderbook_unsorted unit test: checked directly against the golden model at
// top-of-book + qty sums (its internal slot order has no sorted counterpart).
#include "Vorderbook_unsorted.h"
#include "tb_orderbook_common.h"

static bool check_top(Vorderbook_unsorted* dut, orderbook_t* golden, const char* label) {
    bool ok = true;

    if ((bool)dut->error_o != golden->error) {
        printf("FAIL [%s] error: DUT=%d golden=%d\n", label, dut->error_o, golden->error);
        ok = false;
    }

    if (!golden->error) {
        if (dut->best_bid_price_o != golden->bid_prices[0] ||
            dut->best_bid_qty_o   != golden->bid_qtys[0]) {
            printf("FAIL [%s] best_bid: DUT=(%3u,%3u) golden=(%3u,%3u)\n", label,
                   dut->best_bid_price_o, dut->best_bid_qty_o,
                   golden->bid_prices[0], golden->bid_qtys[0]);
            ok = false;
        }
        if (dut->best_ask_price_o != golden->ask_prices[0] ||
            dut->best_ask_qty_o   != golden->ask_qtys[0]) {
            printf("FAIL [%s] best_ask: DUT=(%3u,%3u) golden=(%3u,%3u)\n", label,
                   dut->best_ask_price_o, dut->best_ask_qty_o,
                   golden->ask_prices[0], golden->ask_qtys[0]);
            ok = false;
        }

        uint32_t gold_bid_sum = 0, gold_ask_sum = 0;
        for (int i = 0; i < ORDERBOOK_N; i++) {
            gold_bid_sum += golden->bid_qtys[i];
            gold_ask_sum += golden->ask_qtys[i];
        }
        if (dut->bid_qty_sum_o != gold_bid_sum) {
            printf("FAIL [%s] bid_qty_sum: DUT=%u golden=%u\n", label, dut->bid_qty_sum_o, gold_bid_sum);
            ok = false;
        }
        if (dut->ask_qty_sum_o != gold_ask_sum) {
            printf("FAIL [%s] ask_qty_sum: DUT=%u golden=%u\n", label, dut->ask_qty_sum_o, gold_ask_sum);
            ok = false;
        }
    }

    return ok;
}

int main(int argc, char** argv) {
    g_avoid_sentinels = true;
    return run_orderbook_tb<Vorderbook_unsorted>(argc, argv, "obj_dir/orderbook_unsorted.vcd", check_top);
}
