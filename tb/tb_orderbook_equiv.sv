// Equivalence testbench: drives orderbook_sorted and orderbook_unsorted (via
// the wrapper) with identical random stimulus and checks that every observable
// output agrees on every cycle. No software golden model involved; this proves
// the two RTL implementations are behaviorally identical at top-of-book.
module tb_orderbook_equiv;
    import orderbook_pkg::*;

    parameter int N = 8;
    localparam int SUM_WIDTH = $clog2(N)+QTY_WIDTH;

    logic clk = 0;
    logic rst_n;

    logic   valid;
    op_t    op;
    side_t  side;
    price_t price;
    qty_t   qty;

    price_t s_bb_p, s_ba_p, u_bb_p, u_ba_p;
    qty_t   s_bb_q, s_ba_q, u_bb_q, u_ba_q;
    logic [SUM_WIDTH-1:0] s_bsum, s_asum, u_bsum, u_asum;
    logic s_err, u_err;

    orderbook #(.N(N), .UNSORTED(1'b0)) dut_sorted (
        .clk_i(clk), .rst_ni(rst_n), .valid_i(valid),
        .op_i(op), .side_i(side), .price_i(price), .qty_i(qty),
        .best_bid_price_o(s_bb_p), .best_bid_qty_o(s_bb_q),
        .best_ask_price_o(s_ba_p), .best_ask_qty_o(s_ba_q),
        .bid_qty_sum_o(s_bsum), .ask_qty_sum_o(s_asum), .error_o(s_err)
    );

    orderbook #(.N(N), .UNSORTED(1'b1)) dut_unsorted (
        .clk_i(clk), .rst_ni(rst_n), .valid_i(valid),
        .op_i(op), .side_i(side), .price_i(price), .qty_i(qty),
        .best_bid_price_o(u_bb_p), .best_bid_qty_o(u_bb_q),
        .best_ask_price_o(u_ba_p), .best_ask_qty_o(u_ba_q),
        .bid_qty_sum_o(u_bsum), .ask_qty_sum_o(u_asum), .error_o(u_err)
    );

    always #5 clk = ~clk;

    int unsigned nops = 200000;
    int unsigned mismatches = 0;

    function automatic void check();
        if (s_err !== u_err) begin
            $error("error mismatch: sorted=%b unsorted=%b", s_err, u_err);
            mismatches++;
        end
        // outputs only meaningful while not in the sticky error state
        if (!s_err) begin
            if (s_bb_p !== u_bb_p || s_bb_q !== u_bb_q) begin
                $error("best bid mismatch: sorted=%0d@%0d unsorted=%0d@%0d",
                       s_bb_q, s_bb_p, u_bb_q, u_bb_p);
                mismatches++;
            end
            if (s_ba_p !== u_ba_p || s_ba_q !== u_ba_q) begin
                $error("best ask mismatch: sorted=%0d@%0d unsorted=%0d@%0d",
                       s_ba_q, s_ba_p, u_ba_q, u_ba_p);
                mismatches++;
            end
            if (s_bsum !== u_bsum || s_asum !== u_asum) begin
                $error("sum mismatch: sorted b=%0d a=%0d unsorted b=%0d a=%0d",
                       s_bsum, s_asum, u_bsum, u_asum);
                mismatches++;
            end
        end
    endfunction

    initial begin
        int unsigned pmax;
        if (!$value$plusargs("nops=%d", nops)) nops = 200000;

        // price upper bound (lower bound is 1, clearing the bid sentinel). Default
        // spans the full price space minus the ask sentinel; +dense narrows it to
        // 2N so the N-slot book fills often and the eviction path is stressed.
        pmax = (1 << PRICE_WIDTH) - 2;
        if ($test$plusargs("dense")) pmax = 2*N;

        rst_n = 0; valid = 0;
        op = Insert; side = Bid; price = '0; qty = '0;
        repeat (3) @(posedge clk);
        rst_n = 1;

        for (int unsigned i = 0; i < nops; i++) begin
            @(negedge clk);
            valid = 1'b1;
            op    = ($urandom_range(0, 3) == 0) ? Remove : Insert; // 25% removes
            side  = side_t'($urandom_range(0, 1));
            price = price_t'($urandom_range(1, pmax));
            // full qty range minus the degenerate qty==0 (empty slot / error marker)
            qty   = qty_t'($urandom_range(1, (1 << QTY_WIDTH) - 1));

            @(posedge clk);
            #1;
            check();
            if (mismatches != 0) begin
                $fatal(1, "EQUIV FAIL at op %0d", i);
            end

            // sticky error reached: confirmed both agree, now reset to keep churning
            if (s_err) begin
                valid = 1'b0;
                rst_n = 1'b0;
                @(posedge clk); #1;
                check();
                rst_n = 1'b1;
            end
        end

        valid = 1'b0;
        @(posedge clk);
        $display("EQUIV PASS: %0d ops, 0 mismatches (N=%0d)", nops, N);
        $finish;
    end

endmodule
