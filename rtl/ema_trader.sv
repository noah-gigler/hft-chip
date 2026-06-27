module ema_trader
    import orderbook_pkg::*;
#(
    parameter int EMA_SHIFT = 4,
    parameter int DEV_THRESHOLD = 4,   
    parameter int ORDER_QTY = 32,
    parameter int MAX_POS   = 8*ORDER_QTY
)(
    input  logic           clk_i,
    input  logic           rst_ni,

    // orderbook — top-of-book only (best bid/ask)
    input  price_t bid_price_i,
    input  qty_t   bid_qty_i,
    input  price_t ask_price_i,
    input  qty_t   ask_qty_i,

    // private order feed
    input logic            order_filled_i,
    input price_t          filled_price_i,
    input qty_t            filled_qty_i,
    input side_t           filled_side_i,

    input logic            grant_i,

    // trade output
    output logic           valid_o,
    output side_t          side_o,
    output price_t         price_o,
    output qty_t           qty_o,

    output logic           error_o
);

    typedef enum logic [1:0] {
        IDLE    = 2'd0,
        TRADE   = 2'd1,
        WAIT    = 2'd2
    } state_t;

    state_t state_q, state_d;

    logic[1:0] pending_q, pending_d;
    logic signed[$clog2(MAX_POS)+1:0] position_q, position_d;
    logic error_q, error_d;
    
    logic signed [PRICE_WIDTH+EMA_SHIFT:0] ema_q, ema_d;
    logic ema_init_q, ema_init_d;  

    side_t  order_side_q,  order_side_d;
    price_t order_price_q, order_price_d;
    qty_t   order_qty_q,   order_qty_d;

    always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni) begin
            state_q       <= IDLE;
            pending_q     <= '0;
            error_q       <= '0;
            ema_q         <= '0;
            ema_init_q    <= '0;
            order_side_q  <= Bid;
            order_price_q <= '0;
            order_qty_q   <= '0;
            position_q <= '0;
        end else begin
            state_q       <= state_d;
            pending_q     <= pending_d;
            position_q    <= position_d;
            error_q       <= error_d;
            ema_q         <= ema_d;
            ema_init_q    <= ema_init_d;
            order_side_q  <= order_side_d;
            order_price_q <= order_price_d;
            order_qty_q   <= order_qty_d;
        end
    end

    always_comb begin
        logic [PRICE_WIDTH:0] mid; 
        logic signed [PRICE_WIDTH+EMA_SHIFT:0] mid_scaled; 
        logic signed [PRICE_WIDTH+1:0] dev; 
        logic both_liquid;
        logic buy_signal, sell_signal;
        logic signed[$clog2(MAX_POS)+1:0] pos_next;
        logic [1:0] pend_next;

        state_d      = state_q;
        error_d      = error_q | (order_filled_i && pending_q == 0)
                                | (order_filled_i && filled_qty_i > order_qty_q);
        ema_d        = ema_q;
        ema_init_d   = ema_init_q;
        order_side_d  = order_side_q;
        order_price_d = order_price_q;
        order_qty_d   = order_qty_q;

        // default
        valid_o  = '0;
        side_o   = Bid;
        price_o  = '0;
        qty_o    = '0;
        error_o  = error_d;

        both_liquid = (bid_qty_i != 0) && (ask_qty_i != 0);

        mid = ({1'b0, bid_price_i} + {1'b0, ask_price_i}) >> 1;
        mid_scaled = signed'({1'b0, mid}) <<< EMA_SHIFT;

        if (both_liquid) begin
            if (!ema_init_q) begin
                ema_d = mid_scaled;
                ema_init_d = 1'b1;
            end else begin
                ema_d = ema_q + ((mid_scaled - ema_q) >>> EMA_SHIFT);
            end
        end

        // computed here to avoid combinational loop
        pos_next  = position_q;
        pend_next = pending_q;
        if (!error_d && order_filled_i) begin
            pend_next = pending_q - 1'b1;
            if (filled_side_i == Bid)
                pos_next = position_q + filled_qty_i;
            else
                pos_next = position_q - filled_qty_i;
        end
        position_d = pos_next;
        pending_d  = pend_next;

        if (!error_d) begin
            order_side_d  = order_side_q;
            order_price_d = order_price_q;
            order_qty_d   = order_qty_q;

            case (state_q)

                IDLE: begin
                    dev = signed'((mid_scaled - ema_d) >>> EMA_SHIFT);

                    buy_signal = ema_init_q && both_liquid && (dev < -DEV_THRESHOLD) && (pos_next < MAX_POS);
                    sell_signal = ema_init_q && both_liquid && (dev > DEV_THRESHOLD)  && (pos_next > -MAX_POS);

                    if (buy_signal) begin
                        order_side_d  = Bid;
                        order_price_d = ask_price_i;
                        order_qty_d   = min3(ORDER_QTY, ask_qty_i, MAX_POS - pos_next);
                        state_d       = TRADE;
                    end else if (sell_signal) begin
                        order_side_d  = Ask;
                        order_price_d = bid_price_i;
                        order_qty_d   = min3(ORDER_QTY, bid_qty_i, MAX_POS + pos_next);
                        state_d       = TRADE;
                    end
                end

                TRADE: begin
                    valid_o = '1;
                    side_o  = order_side_q;
                    price_o = order_price_q;
                    qty_o   = order_qty_q;

                    if (grant_i) begin
                        pending_d = pend_next + 1'b1;
                        state_d = WAIT;
                    end
                end

                WAIT: begin
                    if (pend_next == 0) begin
                        state_d = IDLE;
                    end
                end

                default: state_d = IDLE;

            endcase
        end
    end

endmodule