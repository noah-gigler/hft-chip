// Unsorted orderbook. Stores active levels in an unsorted pool (a slot is empty
// iff its qty is 0) and extracts best bid/ask via a balanced comparator tree,
// instead of maintaining a sorted shift register like orderbook.sv. Observable
// outputs (best bid/ask, qty sums, error) are equivalent; only the internal
// slot ordering differs. See CLAUDE.md for the rationale.
module orderbook_unsorted
    import orderbook_pkg::*;
#(
    parameter int N
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

    localparam int SUM_WIDTH = $clog2(N)+QTY_WIDTH;
    localparam int IDX_WIDTH = $clog2(N);

    typedef price_t [N-1:0] prices_t;
    typedef qty_t   [N-1:0] qtys_t;

    // a level carried through the reduction tree. qty == 0 marks an empty slot.
    typedef struct packed {
        price_t               price;
        qty_t                 qty;
        logic [IDX_WIDTH-1:0] idx;
    } lvl_t;

    typedef lvl_t lvl_arr_t [N];

    prices_t bid_prices_d, bid_prices_q;
    qtys_t   bid_qtys_d,   bid_qtys_q;
    prices_t ask_prices_d, ask_prices_q;
    qtys_t   ask_qtys_d,   ask_qtys_q;

    logic error_d, error_q;

    logic [SUM_WIDTH-1:0] bid_sum_d, bid_sum_q;
    logic [SUM_WIDTH-1:0] ask_sum_d, ask_sum_q;

    always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni) begin
            error_q      <= 1'b0;
            bid_prices_q <= '0;
            bid_qtys_q   <= '0;
            ask_prices_q <= '0;
            ask_qtys_q   <= '0;
            bid_sum_q    <= '0;
            ask_sum_q    <= '0;
        end else begin
            error_q      <= error_d;
            bid_prices_q <= bid_prices_d;
            bid_qtys_q   <= bid_qtys_d;
            ask_prices_q <= ask_prices_d;
            ask_qtys_q   <= ask_qtys_d;
            bid_sum_q    <= bid_sum_d;
            ask_sum_q    <= ask_sum_d;
        end
    end

    // index of the lowest set bit (0 if none), log-depth (see orderbook.sv).
    function automatic logic [IDX_WIDTH-1:0] first(input logic [N-1:0] array);
        logic [N-1:0] lsb;
        lsb = array & (-array);
        first = '0;
        for (int i = 0; i < N; i++)
            if (lsb[i]) first |= i[IDX_WIDTH-1:0];
    endfunction

    // keep the more extreme of two levels: prefer the non-empty one; among two
    // non-empty prefer the larger price when want_max, else the smaller.
    function automatic lvl_t pick(input lvl_t a, b, input logic want_max);
        if (a.qty == 0) return b;
        if (b.qty == 0) return a;
        return ((a.price >= b.price) == want_max) ? a : b;
    endfunction

    // balanced log-depth reduction over the pool to the most extreme level.
    function automatic lvl_t reduce(input lvl_arr_t arr, input logic want_max);
        lvl_arr_t tmp;
        tmp = arr;
        for (int stride = 1; stride < N; stride <<= 1)
            for (int i = 0; i + stride < N; i += (stride << 1))
                tmp[i] = pick(tmp[i], tmp[i+stride], want_max);
        return tmp[0];
    endfunction

    function automatic lvl_arr_t cand_of(input prices_t prices, input qtys_t qtys);
        for (int i = 0; i < N; i++)
            cand_of[i] = '{price: prices[i], qty: qtys[i], idx: i[IDX_WIDTH-1:0]};
    endfunction

    always_comb begin
        prices_t cur_prices, new_prices;
        qtys_t   cur_qtys,   new_qtys;
        lvl_t    worst;
        logic [N-1:0] equals, empties;
        logic any_eq, any_empty, evict;
        logic [IDX_WIDTH-1:0] eq_idx, empty_idx;
        logic pre_error;
        logic signed [QTY_WIDTH+1:0] sum_delta;

        bid_prices_d = bid_prices_q;
        bid_qtys_d   = bid_qtys_q;
        ask_prices_d = ask_prices_q;
        ask_qtys_d   = ask_qtys_q;
        bid_sum_d    = bid_sum_q;
        ask_sum_d    = ask_sum_q;
        sum_delta    = '0;

        cur_prices = (side_i == Bid) ? bid_prices_q : ask_prices_q;
        cur_qtys   = (side_i == Bid) ? bid_qtys_q   : ask_qtys_q;
        new_prices = cur_prices;
        new_qtys   = cur_qtys;
        equals     = '0;
        empties    = '0;
        any_eq     = 1'b0;
        any_empty  = 1'b0;
        evict      = 1'b0;
        eq_idx     = '0;
        empty_idx  = '0;
        pre_error  = 1'b0;
        worst      = '0;

        error_d = error_q;

        if (valid_i && !error_q) begin
            for (int i = 0; i < N; i++) begin
                equals[i]  = (cur_qtys[i] != 0) && (cur_prices[i] == price_i);
                empties[i] = (cur_qtys[i] == 0);
            end

            any_eq    = |equals;
            eq_idx    = first(equals);
            any_empty = |empties;
            empty_idx = first(empties);

            // testbench-only invariant (not synthesized): a valid op matches at
            // most one active level, since the book keeps active prices distinct.
`ifndef SYNTHESIS
            assert (!any_eq || $onehot0(equals))
                else $error("orderbook_unsorted: duplicate active price, equals=%b", equals);
`endif

            pre_error = (qty_i == 0);

            if (pre_error) begin
                error_d = 1'b1;
            end else begin
                unique case (op_i)
                    Insert: if (any_eq) begin
                        if (qty_i > '1 - cur_qtys[eq_idx]) begin
                            error_d = 1'b1;
                        end else begin
                            new_qtys[eq_idx] = cur_qtys[eq_idx] + qty_i;
                            sum_delta = signed'({2'b0, qty_i});
                        end
                    end else if (any_empty) begin
                        new_prices[empty_idx] = price_i;
                        new_qtys[empty_idx]   = qty_i;
                        sum_delta = signed'({2'b0, qty_i});
                    end else begin
                        // book full: evict the worst level if the new price beats it.
                        // worst bid = lowest price (want_max=0), worst ask = highest (want_max=1).
                        worst = reduce(cand_of(cur_prices, cur_qtys), side_i == Ask);
                        evict = (side_i == Bid) ? (price_i > worst.price) : (price_i < worst.price);
                        if (evict) begin
                            new_prices[worst.idx] = price_i;
                            new_qtys[worst.idx]   = qty_i;
                            sum_delta = signed'({2'b0, qty_i}) - signed'({2'b0, worst.qty});
                        end
                        // else: price worse than every tracked level — silently drop
                    end
                    Remove: if (any_eq && cur_qtys[eq_idx] >= qty_i) begin
                        new_qtys[eq_idx] = cur_qtys[eq_idx] - qty_i; // 0 frees the slot
                        sum_delta = -signed'({2'b0, qty_i});
                    end else begin
                        error_d = 1'b1;
                    end
                    default: error_d = 1'b1;
                endcase

                unique case (side_i)
                    Bid: begin
                        bid_prices_d = new_prices;
                        bid_qtys_d   = new_qtys;
                        bid_sum_d    = SUM_WIDTH'(signed'({1'b0, bid_sum_q}) + sum_delta);
                    end
                    Ask: begin
                        ask_prices_d = new_prices;
                        ask_qtys_d   = new_qtys;
                        ask_sum_d    = SUM_WIDTH'(signed'({1'b0, ask_sum_q}) + sum_delta);
                    end
                    default: error_d = 1'b1;
                endcase
            end
        end
    end

    // L1 extraction: best bid = max active price, best ask = min active price.
    lvl_t best_bid, best_ask;
    always_comb begin
        best_bid = reduce(cand_of(bid_prices_q, bid_qtys_q), 1'b1);
        best_ask = reduce(cand_of(ask_prices_q, ask_qtys_q), 1'b0);
    end

    assign best_bid_price_o = (best_bid.qty != 0) ? best_bid.price : DEFAULT_BID;
    assign best_bid_qty_o   = best_bid.qty;
    assign best_ask_price_o = (best_ask.qty != 0) ? best_ask.price : DEFAULT_ASK;
    assign best_ask_qty_o   = best_ask.qty;

    assign bid_qty_sum_o = bid_sum_q;
    assign ask_qty_sum_o = ask_sum_q;

    assign error_o = error_q;

endmodule
