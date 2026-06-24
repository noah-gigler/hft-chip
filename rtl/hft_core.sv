module hft_core import orderbook_pkg::*; #(
    parameter int N,
    parameter bit OB_UNSORTED = 1'b1
)(
    input  logic                   clk_i,
    input  logic                   rst_ni,

    input  logic                   valid_i,
    input  logic                   message_type_i,
    input  logic [1:0]             market_i,
    input  logic                   op_i,
    input  logic                   side_i,
    input  logic [PRICE_WIDTH-1:0] price_i,
    input  logic [QTY_WIDTH-1:0]   qty_i,

    output logic                   valid_o,
    output logic [1:0]             market_o,
    output logic                   side_o,
    output logic [PRICE_WIDTH-1:0] price_o,
    output logic [QTY_WIDTH-1:0]   qty_o,
    output logic                   error_o
);

  localparam int SUM_WIDTH = $clog2(N)+QTY_WIDTH;

  price_t [3:0] bid_best_price, ask_best_price;
  qty_t   [3:0] bid_best_qty,   ask_best_qty;

  price_t [3:0] bid_price0_q, ask_price0_q;
  qty_t   [3:0] bid_qty0_q,   ask_qty0_q;

  logic [SUM_WIDTH-1:0] bid_qty_sum2, ask_qty_sum2;
  logic [SUM_WIDTH-1:0] bid_qty_sum2_q, ask_qty_sum2_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      bid_price0_q <= '{default: DEFAULT_BID};
      ask_price0_q <= '{default: DEFAULT_ASK};
      bid_qty0_q   <= '0;
      ask_qty0_q   <= '0;
      bid_qty_sum2_q <= '0;
      ask_qty_sum2_q <= '0;
    end else begin
      for (int i = 0; i < 4; i++) begin
        bid_price0_q[i] <= bid_best_price[i];
        ask_price0_q[i] <= ask_best_price[i];
        bid_qty0_q[i]   <= bid_best_qty[i];
        ask_qty0_q[i]   <= ask_best_qty[i];
      end
      bid_qty_sum2_q <= bid_qty_sum2;
      ask_qty_sum2_q <= ask_qty_sum2;
    end
  end

  logic [3:0] error_ob;
  logic [2:0] error_trader;
  logic [3:0] valid_ob;
  logic [2:0] valid_trader;

  logic   valid_arb, valid_mom, valid_ema;
  side_t  side_arb, side_mom, side_ema;
  price_t price_arb, price_mom, price_ema;
  qty_t   qty_arb, qty_mom, qty_ema;
  logic   market_arb;

  logic grant_mom, grant_ema;

  assign grant_mom = valid_mom & ~valid_arb;
  assign grant_ema = valid_ema & ~valid_arb & ~valid_mom;

  always_comb begin
    for (int i = 0; i < 4; i++)
      valid_ob[i] = valid_i & (msg_type_t'(message_type_i) == Public) & (market_i == 2'(i));
    valid_trader[0] = valid_i & (msg_type_t'(message_type_i) == Private) & (market_i < 2);
    valid_trader[1] = valid_i & (msg_type_t'(message_type_i) == Private) & (market_i == 2);
    valid_trader[2] = valid_i & (msg_type_t'(message_type_i) == Private) & (market_i == 3);
  end

  // fixed-priority output arbitration: arb > mom > ema (combinational)
  assign valid_o  = valid_arb | valid_mom | valid_ema;
  assign market_o = valid_arb ? {1'b0, market_arb} : valid_mom ? 2'd2 : 2'd3;
  assign side_o   = valid_arb ? side_arb : valid_mom ? side_mom : side_ema;
  assign price_o  = valid_arb ? price_arb : valid_mom ? price_mom : price_ema;
  assign qty_o    = valid_arb ? qty_arb   : valid_mom ? qty_mom   : qty_ema;
  assign error_o  = |error_ob | |error_trader;

  orderbook #(.N(N), .UNSORTED(OB_UNSORTED)) orderbook0_i (
    .clk_i        (clk_i),
    .rst_ni       (rst_ni),

    .valid_i      (valid_ob[0]),
    .op_i         (op_t'(op_i)),
    .side_i       (side_t'(side_i)),
    .price_i      (price_i),
    .qty_i        (qty_i),

    .best_bid_price_o (bid_best_price[0]),
    .best_bid_qty_o   (bid_best_qty[0]),
    .best_ask_price_o (ask_best_price[0]),
    .best_ask_qty_o   (ask_best_qty[0]),
    .bid_qty_sum_o    (),
    .ask_qty_sum_o    (),

    .error_o      (error_ob[0])
  );

  orderbook #(.N(N), .UNSORTED(OB_UNSORTED)) orderbook1_i (
    .clk_i        (clk_i),
    .rst_ni       (rst_ni),

    .valid_i      (valid_ob[1]),
    .op_i         (op_t'(op_i)),
    .side_i       (side_t'(side_i)),
    .price_i      (price_i),
    .qty_i        (qty_i),

    .best_bid_price_o (bid_best_price[1]),
    .best_bid_qty_o   (bid_best_qty[1]),
    .best_ask_price_o (ask_best_price[1]),
    .best_ask_qty_o   (ask_best_qty[1]),
    .bid_qty_sum_o    (),
    .ask_qty_sum_o    (),

    .error_o      (error_ob[1])
  );

  orderbook #(.N(N), .UNSORTED(OB_UNSORTED)) orderbook2_i (
    .clk_i        (clk_i),
    .rst_ni       (rst_ni),

    .valid_i      (valid_ob[2]),
    .op_i         (op_t'(op_i)),
    .side_i       (side_t'(side_i)),
    .price_i      (price_i),
    .qty_i        (qty_i),

    .best_bid_price_o (bid_best_price[2]),
    .best_bid_qty_o   (bid_best_qty[2]),
    .best_ask_price_o (ask_best_price[2]),
    .best_ask_qty_o   (ask_best_qty[2]),
    .bid_qty_sum_o    (bid_qty_sum2),
    .ask_qty_sum_o    (ask_qty_sum2),

    .error_o      (error_ob[2])
  );

  orderbook #(.N(N), .UNSORTED(OB_UNSORTED)) orderbook3_i (
    .clk_i        (clk_i),
    .rst_ni       (rst_ni),

    .valid_i      (valid_ob[3]),
    .op_i         (op_t'(op_i)),
    .side_i       (side_t'(side_i)),
    .price_i      (price_i),
    .qty_i        (qty_i),

    .best_bid_price_o (bid_best_price[3]),
    .best_bid_qty_o   (bid_best_qty[3]),
    .best_ask_price_o (ask_best_price[3]),
    .best_ask_qty_o   (ask_best_qty[3]),
    .bid_qty_sum_o    (),
    .ask_qty_sum_o    (),

    .error_o      (error_ob[3])
  );

  arb_trader arb_trader_i (
    .clk_i        (clk_i),
    .rst_ni       (rst_ni),

    .bid_price0_i (bid_price0_q[0]),
    .bid_qty0_i   (bid_qty0_q[0]),
    .ask_price0_i (ask_price0_q[0]),
    .ask_qty0_i   (ask_qty0_q[0]),

    .bid_price1_i (bid_price0_q[1]),
    .bid_qty1_i   (bid_qty0_q[1]),
    .ask_price1_i (ask_price0_q[1]),
    .ask_qty1_i   (ask_qty0_q[1]),

    .order_filled_i (valid_trader[0]),
    .filled_price_i (price_i),
    .filled_qty_i   (qty_i),
    .filled_side_i  (side_t'(side_i)),

    .valid_o  (valid_arb),
    .market_o (market_arb),
    .side_o   (side_arb),
    .price_o  (price_arb),
    .qty_o    (qty_arb),
    .error_o  (error_trader[0])
  );

  momentum_trader #(.N(N)) mom_trader_i (
    .clk_i        (clk_i),
    .rst_ni       (rst_ni),

    .bid_price_i (bid_price0_q[2]),
    .bid_qty_i   (bid_qty0_q[2]),
    .ask_price_i (ask_price0_q[2]),
    .ask_qty_i   (ask_qty0_q[2]),
    .bid_qty_sum_i (bid_qty_sum2_q),
    .ask_qty_sum_i (ask_qty_sum2_q),

    .order_filled_i (valid_trader[1]),
    .filled_price_i (price_i),
    .filled_qty_i   (qty_i),
    .filled_side_i  (side_t'(side_i)),

    .grant_i (grant_mom),

    .valid_o  (valid_mom),
    .side_o   (side_mom),
    .price_o  (price_mom),
    .qty_o    (qty_mom),
    .error_o  (error_trader[1])
  );

  ema_trader ema_trader_i (
    .clk_i        (clk_i),
    .rst_ni       (rst_ni),

    .bid_price_i (bid_price0_q[3]),
    .bid_qty_i   (bid_qty0_q[3]),
    .ask_price_i (ask_price0_q[3]),
    .ask_qty_i   (ask_qty0_q[3]),

    .order_filled_i (valid_trader[2]),
    .filled_price_i (price_i),
    .filled_qty_i   (qty_i),
    .filled_side_i  (side_t'(side_i)),

    .grant_i (grant_ema),

    .valid_o  (valid_ema),
    .side_o   (side_ema),
    .price_o  (price_ema),
    .qty_o    (qty_ema),
    .error_o  (error_trader[2])
  );

endmodule
