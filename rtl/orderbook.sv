module orderbook
    import orderbook_pkg::*;
#(
    parameter int N = 8
)(
    input logic clk_i,
    input logic rst_ni,

    input logic valid_i,

    input op_t op_i,
    input side_t side_i,
    input price_t price_i,
    input qty_t qty_i,

    output price_t [N-1:0] bid_prices_o,
    output qty_t   [N-1:0] bid_qtys_o,
    output price_t [N-1:0] ask_prices_o,
    output qty_t   [N-1:0] ask_qtys_o,

    output logic error_o
);

    typedef price_t[N-1:0] prices_t;
    typedef qty_t[N-1:0] qtys_t;

    prices_t bid_prices_d, bid_prices_q;
    qtys_t bid_qtys_d, bid_qtys_q;

    prices_t ask_prices_d, ask_prices_q;
    qtys_t ask_qtys_d, ask_qtys_q;

    logic error_d, error_q;

    always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni) begin
            error_q <= 1'b0;

            bid_prices_q <= '{default: DEFAULT_BID};
            bid_qtys_q <= 0;
            ask_prices_q <= '{default: DEFAULT_ASK};
            ask_qtys_q <= 0;
        end else begin
            error_q <= error_d;
        
            bid_prices_q <= bid_prices_d;
            bid_qtys_q <= bid_qtys_d;
            ask_prices_q <= ask_prices_d;
            ask_qtys_q <= ask_qtys_d;
        end
    end

    function automatic logic [$clog2(N)-1:0] first(
        input logic [N-1:0] array
    );
        first = 0;
        for (int i = N-1; i >= 0; i--) begin
            if (array[i])
                first = i[$clog2(N)-1:0];
        end
    endfunction

    always_comb begin
        prices_t cur_prices;
        prices_t new_prices;
        qtys_t   cur_qtys;
        qtys_t   new_qtys;
        logic[N-1:0] compares;
        logic[N-1:0] equals;
        logic any_eq;
        logic[$clog2(N)-1:0] eq_idx;
        logic[$clog2(N)-1:0] cmp_idx;
        logic pre_error;
        logic onehot;
        
        bid_prices_d = bid_prices_q;
        bid_qtys_d = bid_qtys_q;
        ask_prices_d = ask_prices_q;
        ask_qtys_d = ask_qtys_q;

        // assign defaults to avoid latches
        cur_prices = (side_i == Bid) ? bid_prices_q : ask_prices_q;
        cur_qtys   = (side_i == Bid) ? bid_qtys_q : ask_qtys_q;
        new_prices = cur_prices;
        new_qtys   = cur_qtys;
        equals     = '0;
        compares   = '0;
        any_eq     = 1'b0;
        eq_idx     = '0;
        cmp_idx    = '0;
        onehot     = 1'b0;
        pre_error  = 1'b0;

        error_d = error_q;

        if (valid_i && !error_q) begin

            for (int i = 0; i < N; i++) begin
                equals[i] = (cur_qtys[i] != 0) && (cur_prices[i] == price_i);
                compares[i] = (side_i == Bid) ? (price_i > cur_prices[i]) : (price_i < cur_prices[i]);
            end

            any_eq = |equals;
            eq_idx = first(equals);


            onehot = (equals & (equals - 1)) == 0;
            pre_error = (qty_i == 0) || (any_eq && !onehot);

            if (pre_error) begin
                error_d = 1'b1;
            end else begin
                unique case (op_i)
                    Insert: if (any_eq) begin
                        // overflow check
                        if (qty_i > '1 - cur_qtys[eq_idx]) begin
                            error_d = 1'b1;
                        end else begin
                            new_qtys[eq_idx] = cur_qtys[eq_idx] + qty_i;
                        end
                    end else if (|compares) begin
                        cmp_idx = first(compares); // only call first if we have at least one
                        for (int i = 1; i < N; i++) begin
                            new_prices[i] = (i > cmp_idx) ? cur_prices[i-1] : cur_prices[i];
                            new_qtys[i]   = (i > cmp_idx) ? cur_qtys[i-1] : cur_qtys[i];
                        end
                        new_prices[cmp_idx] = price_i;
                        new_qtys[cmp_idx]   = qty_i;
                    end
                    // else: price outside top-N tracked levels — silently drop
                    Remove: if (any_eq && cur_qtys[eq_idx] >= qty_i) begin
                        if (cur_qtys[eq_idx] == qty_i) begin
                            for (int i = 0; i < N-1; i++) begin
                                new_prices[i] = (i < eq_idx) ? cur_prices[i] : cur_prices[i+1];
                                new_qtys[i]   = (i < eq_idx) ? cur_qtys[i] : cur_qtys[i+1];
                            end
                            new_prices[N-1] = (side_i == Bid) ? DEFAULT_BID : DEFAULT_ASK;
                            new_qtys[N-1] = 0;
                        end else begin
                            new_qtys[eq_idx] = cur_qtys[eq_idx] - qty_i;
                        end
                    end else begin
                        error_d = 1'b1;
                    end
                    default: begin
                        error_d = 1'b1;
                    end
                endcase

                unique case (side_i)
                    Bid: begin
                        bid_prices_d = new_prices;
                        bid_qtys_d = new_qtys;
                    end
                    Ask: begin
                        ask_prices_d = new_prices;
                        ask_qtys_d = new_qtys;
                    end
                    default: begin
                        error_d = 1'b1;
                    end
                endcase
            end
        end
    end
    
    assign bid_prices_o = bid_prices_q;
    assign bid_qtys_o   = bid_qtys_q;
    assign ask_prices_o = ask_prices_q;
    assign ask_qtys_o   = ask_qtys_q;

    assign error_o = error_q;

endmodule
