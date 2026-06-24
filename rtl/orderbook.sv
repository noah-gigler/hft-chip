// Orderbook wrapper exposing a unified top-of-book interface. UNSORTED selects
// the implementation at elaboration: 0 = orderbook_sorted (sorted shift
// register, full depth internally), 1 = orderbook_unsorted (unsorted pool).
// Both expose identical ports here, so hft_core is impl-agnostic and the
// equivalence testbench can instantiate this wrapper twice and compare.
module orderbook
    import orderbook_pkg::*;
#(
    parameter int N,
    parameter bit UNSORTED = 1'b1
)(
    input logic clk_i,
    input logic rst_ni,

    input logic valid_i,

    input op_t op_i,
    input side_t side_i,
    input price_t price_i,
    input qty_t qty_i,

    output price_t best_bid_price_o,
    output qty_t   best_bid_qty_o,
    output price_t best_ask_price_o,
    output qty_t   best_ask_qty_o,

    output logic [$clog2(N)+QTY_WIDTH-1:0] bid_qty_sum_o,
    output logic [$clog2(N)+QTY_WIDTH-1:0] ask_qty_sum_o,

    output logic error_o
);

    if (UNSORTED) begin : g_unsorted
        orderbook_unsorted #(.N(N)) i_ob (
            .clk_i, .rst_ni, .valid_i, .op_i, .side_i, .price_i, .qty_i,
            .best_bid_price_o, .best_bid_qty_o,
            .best_ask_price_o, .best_ask_qty_o,
            .bid_qty_sum_o, .ask_qty_sum_o,
            .error_o
        );
    end else begin : g_sorted
        price_t [N-1:0] bid_prices, ask_prices;
        qty_t   [N-1:0] bid_qtys,   ask_qtys;

        orderbook_sorted #(.N(N)) i_ob (
            .clk_i, .rst_ni, .valid_i, .op_i, .side_i, .price_i, .qty_i,
            .bid_prices_o (bid_prices),
            .bid_qtys_o   (bid_qtys),
            .ask_prices_o (ask_prices),
            .ask_qtys_o   (ask_qtys),
            .bid_qty_sum_o, .ask_qty_sum_o,
            .error_o
        );

        // sorted: best is level 0
        assign best_bid_price_o = bid_prices[0];
        assign best_bid_qty_o   = bid_qtys[0];
        assign best_ask_price_o = ask_prices[0];
        assign best_ask_qty_o   = ask_qtys[0];
    end

endmodule
