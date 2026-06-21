module arb_trader
    import orderbook_pkg::*;
#(
    parameter int N = 8,
    parameter int ARB_THRESHOLD = 2
)(
    input  logic           clk_i,
    input  logic           rst_ni,

    // orderbook #1
    input  price_t [N-1:0] bid_prices0_i,
    input  qty_t   [N-1:0] bid_qtys0_i,
    input  price_t [N-1:0] ask_prices0_i,
    input  qty_t   [N-1:0] ask_qtys0_i,

    // orderbook #2
    input  price_t [N-1:0] bid_prices1_i,
    input  qty_t   [N-1:0] bid_qtys1_i,
    input  price_t [N-1:0] ask_prices1_i,
    input  qty_t   [N-1:0] ask_qtys1_i,

    // private order feed
    input logic            order_filled_i,
    input price_t          filled_price_i, // ununsed (average price if over multiple levels)
    input qty_t            filled_qty_i,
    input side_t           filled_side_i,

    // trade output
    output logic           valid_o,
    output logic           market_o,
    output side_t          side_o,
    output price_t         price_o,
    output qty_t           qty_o,

    output logic           error_o
);

    typedef enum logic [1:0] {
        IDLE    = 2'd0,
        TRADE1  = 2'd1,
        TRADE2  = 2'd2,
        FLATTEN = 2'd3
    } state_t;

    state_t state_q, state_d;

    price_t ask_price_q,  ask_price_d;
    price_t bid_price_q,  bid_price_d;
    qty_t   arb_qty_q,    arb_qty_d;

    logic   ask_market_q, ask_market_d;
    logic   bid_market_q, bid_market_d;

    logic[1:0] pending_q, pending_d;
    logic signed[2*QTY_WIDTH-1:0] residual_q, residual_d;
    logic error_q, error_d;

    always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni) begin
            state_q      <= IDLE;
            ask_price_q  <= '0;
            bid_price_q  <= '0;
            arb_qty_q    <= '0;
            ask_market_q <= '0;
            bid_market_q <= '0;
            pending_q    <= '0;
            residual_q   <= '0;
            error_q      <= '0;
        end else begin
            state_q      <= state_d;
            ask_price_q  <= ask_price_d;
            bid_price_q  <= bid_price_d;
            arb_qty_q    <= arb_qty_d;
            ask_market_q <= ask_market_d;
            bid_market_q <= bid_market_d;
            pending_q    <= pending_d;
            residual_q   <= residual_d;
            error_q      <= error_d;
        end
    end

    always_comb begin
        logic arb0, arb1;
        logic [1:0] pend_next;
        logic signed[2*QTY_WIDTH-1:0] res_next;

        // FLATTEN temporaries — declared and defaulted here so no latch is inferred
        logic                 sell;
        logic [QTY_WIDTH-1:0] res_qty;
        price_t               price0, price1;
        qty_t                 quantity0, quantity1;
        logic                 liquid0, liquid1, market;

        arb0      = 1'b0;
        arb1      = 1'b0;
        sell      = 1'b0;
        res_qty   = '0;
        price0    = '0;
        price1    = '0;
        quantity0 = '0;
        quantity1 = '0;
        liquid0   = 1'b0;
        liquid1   = 1'b0;
        market    = 1'b0;

        state_d      = state_q;
        ask_price_d  = ask_price_q;
        bid_price_d  = bid_price_q;
        arb_qty_d    = arb_qty_q;
        ask_market_d = ask_market_q;
        bid_market_d = bid_market_q;
        error_d      = error_q | (order_filled_i && pending_q == 0);

        // default
        valid_o  = '0;
        market_o = '0;
        side_o   = Bid;
        price_o  = '0;
        qty_o    = '0;
        error_o  = error_d;

        // comupted here to avoid combinational loop
        pend_next = pending_q;
        res_next  = residual_q;
        if (!error_d && order_filled_i) begin
            pend_next = pending_q - 1'b1;
            if (filled_side_i == Bid)
                res_next = residual_q + signed'({1'b0, filled_qty_i});
            else
                res_next = residual_q - signed'({1'b0, filled_qty_i});
        end
        pending_d  = pend_next;
        residual_d = res_next;

        if (!error_d) begin
            ask_price_d  = ask_price_q;
            bid_price_d  = bid_price_q;
            arb_qty_d    = arb_qty_q;
            ask_market_d = ask_market_q;
            bid_market_d = bid_market_q;

            case (state_q)

                IDLE: begin
                    arb0 = bid_qtys0_i[0] > 0 && ask_qtys1_i[0] > 0 && (bid_prices0_i[0] > ask_prices1_i[0] + ARB_THRESHOLD);
                    arb1 = bid_qtys1_i[0] > 0 && ask_qtys0_i[0] > 0 && (bid_prices1_i[0] > ask_prices0_i[0] + ARB_THRESHOLD);

                    if (arb0) begin
                        ask_market_d = '0;
                        bid_market_d = '1;

                        ask_price_d  = bid_prices0_i[0];
                        bid_price_d  = ask_prices1_i[0];
                        arb_qty_d    = (bid_qtys0_i[0] < ask_qtys1_i[0]) ? bid_qtys0_i[0] : ask_qtys1_i[0];

                        state_d      = TRADE1;
                    end else if (arb1) begin
                        ask_market_d = '1;
                        bid_market_d = '0;

                        ask_price_d  = bid_prices1_i[0];
                        bid_price_d  = ask_prices0_i[0];
                        arb_qty_d    = (bid_qtys1_i[0] < ask_qtys0_i[0]) ? bid_qtys1_i[0] : ask_qtys0_i[0];

                        state_d      = TRADE1;
                    end
                end

                TRADE1: begin
                    valid_o  = '1;
                    market_o = ask_market_q;
                    side_o   = Ask;
                    price_o  = ask_price_q;
                    qty_o    = arb_qty_q;

                    pending_d = pend_next + 1'b1;
                    state_d  = TRADE2;
                end

                TRADE2: begin
                    valid_o  = '1;
                    market_o = bid_market_q;
                    side_o   = Bid;
                    price_o  = bid_price_q;
                    qty_o    = arb_qty_q;

                    pending_d = pend_next + 1'b1;
                    state_d  = FLATTEN;
                end

                FLATTEN: begin
                    if (pending_q == 0) begin
                        if (residual_q == 0) begin // changed to q (adds latency but stablizes signal)
                            state_d = IDLE;
                        end else begin
                            sell = (residual_q > 0);
                            res_qty = sell ? residual_q : (-residual_q); // value will be truncated

                            price0    = sell ? bid_prices0_i[0] : ask_prices0_i[0];
                            price1    = sell ? bid_prices1_i[0] : ask_prices1_i[0];
                            quantity0 = sell ? bid_qtys0_i[0]   : ask_qtys0_i[0];
                            quantity1 = sell ? bid_qtys1_i[0]   : ask_qtys1_i[0];

                            liquid0 = (quantity0 != 0);
                            liquid1 = (quantity1 != 0);

                            // if both liquid best price (ignores qty for now TODO)
                            if (liquid0 && liquid1)
                                market = sell ? (price1 > price0) : (price1 < price0);
                            else if (liquid1)
                                market = 1'b1;
                            else
                                market = 1'b0;

                            if (liquid0 || liquid1) begin
                                valid_o   = 1'b1;
                                market_o  = market;
                                side_o    = sell ? Ask : Bid;
                                price_o   = market ? price1 : price0;
                                qty_o     = res_qty;
                                pending_d = pend_next + 1'b1;
                            end else begin
                                valid_o = 1'b0;   // no liquidity
                            end
                        end
                    end
                end

                default: state_d = IDLE;

            endcase
        end
    end

endmodule