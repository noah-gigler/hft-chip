module croc_chip import orderbook_pkg::*; #() (
  input  wire clk_i,
  input  wire rst_ni,

  input  wire valid_i,
  input  wire message_type_i,
  input  wire [1:0] market_i,
  input  wire op_i,
  input  wire side_i,
  input  wire [PRICE_WIDTH-1:0] price_i,
  input  wire [QTY_WIDTH-1:0] qty_i,

  output wire valid_o,
  output wire [1:0] market_o,
  output wire side_o,
  output wire [PRICE_WIDTH-1:0] price_o,
  output wire [QTY_WIDTH-1:0] qty_o,

  output wire error_o,
  output wire spare0_o,

  inout wire VDD,
  inout wire VSS,
  inout wire VDDIO,
  inout wire VSSIO
);

  wire clk;
  wire rst_n;

  wire in_valid;
  wire in_message_type;
  wire [1:0] in_market;
  wire in_op;
  wire in_side;
  wire [PRICE_WIDTH-1:0] in_price;
  wire [QTY_WIDTH-1:0] in_qty;
  
  // changed wire to logic due to pipleine
  logic out_valid;
  logic [1:0] out_market;
  logic out_side;
  logic [PRICE_WIDTH-1:0] out_price;
  logic [QTY_WIDTH-1:0] out_qty;

  logic out_error;
  logic out_spare0;

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_clk_i  (.pad(clk_i),  .p2c(clk));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_rst_ni (.pad(rst_ni), .p2c(rst_n));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_valid_i      (.pad(valid_i), .p2c(in_valid));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_message_type_i (.pad(message_type_i), .p2c(in_message_type));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_market0_i     (.pad(market_i[0]), .p2c(in_market[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_market1_i   (.pad(market_i[1]), .p2c(in_market[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_op_i         (.pad(op_i), .p2c(in_op));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_side_i       (.pad(side_i), .p2c(in_side));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price0_i  (.pad(price_i[0]),  .p2c(in_price[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price1_i  (.pad(price_i[1]),  .p2c(in_price[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price2_i  (.pad(price_i[2]),  .p2c(in_price[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price3_i  (.pad(price_i[3]),  .p2c(in_price[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price4_i  (.pad(price_i[4]),  .p2c(in_price[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price5_i  (.pad(price_i[5]),  .p2c(in_price[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price6_i  (.pad(price_i[6]),  .p2c(in_price[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price7_i  (.pad(price_i[7]),  .p2c(in_price[7]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price8_i  (.pad(price_i[8]),  .p2c(in_price[8]));
  // (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price9_i  (.pad(price_i[9]),  .p2c(in_price[9]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty0_i (.pad(qty_i[0]), .p2c(in_qty[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty1_i (.pad(qty_i[1]), .p2c(in_qty[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty2_i (.pad(qty_i[2]), .p2c(in_qty[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty3_i (.pad(qty_i[3]), .p2c(in_qty[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty4_i (.pad(qty_i[4]), .p2c(in_qty[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty5_i (.pad(qty_i[5]), .p2c(in_qty[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty6_i (.pad(qty_i[6]), .p2c(in_qty[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty7_i (.pad(qty_i[7]), .p2c(in_qty[7]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_valid_o  (.pad(valid_o),  .c2p(out_valid));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_market0_o (.pad(market_o[0]), .c2p(out_market[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_market1_o (.pad(market_o[1]), .c2p(out_market[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_side_o   (.pad(side_o),  .c2p(out_side));
  
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price0_o  (.pad(price_o[0]),  .c2p(out_price[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price1_o  (.pad(price_o[1]),  .c2p(out_price[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price2_o  (.pad(price_o[2]),  .c2p(out_price[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price3_o  (.pad(price_o[3]),  .c2p(out_price[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price4_o  (.pad(price_o[4]),  .c2p(out_price[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price5_o  (.pad(price_o[5]),  .c2p(out_price[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price6_o  (.pad(price_o[6]),  .c2p(out_price[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price7_o  (.pad(price_o[7]),  .c2p(out_price[7]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price8_o  (.pad(price_o[8]),  .c2p(out_price[8]));
  // (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price9_o  (.pad(price_o[9]),  .c2p(out_price[9]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty0_o (.pad(qty_o[0]), .c2p(out_qty[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty1_o (.pad(qty_o[1]), .c2p(out_qty[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty2_o (.pad(qty_o[2]), .c2p(out_qty[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty3_o (.pad(qty_o[3]), .c2p(out_qty[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty4_o (.pad(qty_o[4]), .c2p(out_qty[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty5_o (.pad(qty_o[5]), .c2p(out_qty[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty6_o (.pad(qty_o[6]), .c2p(out_qty[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty7_o (.pad(qty_o[7]), .c2p(out_qty[7]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA  pad_error_o (.pad(error_o), .c2p(out_error));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA  pad_spare0_o (.pad(spare0_o), .c2p(out_spare0));

  (* dont_touch = "true" *) sg13cmos5l_IOPadVdd pad_vdd0 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadVdd pad_vdd1 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadVdd pad_vdd2 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadVdd pad_vdd3 ();

  (* dont_touch = "true" *) sg13cmos5l_IOPadVss pad_vss0 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadVss pad_vss1 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadVss pad_vss2 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadVss pad_vss3 ();

  (* dont_touch = "true" *) sg13cmos5l_IOPadIOVdd pad_vddio0 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadIOVdd pad_vddio1 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadIOVdd pad_vddio2 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadIOVdd pad_vddio3 ();

  (* dont_touch = "true" *) sg13cmos5l_IOPadIOVss pad_vssio0 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadIOVss pad_vssio1 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadIOVss pad_vssio2 ();
  (* dont_touch = "true" *) sg13cmos5l_IOPadIOVss pad_vssio3 ();

  localparam int N = 32;
  assign out_spare0 = 1'b0;

  // full-depth orderbook outputs (the books are maintained at full depth, but
  // only top-of-book + the running sums are ever consumed downstream)
  price_t [3:0][N-1:0] bid_prices, ask_prices;
  qty_t   [3:0][N-1:0] bid_qtys,  ask_qtys;

  // pipeline stage between orderbooks and traders: only top-of-book is needed,
  // so we register just level 0 (best bid/ask) per book instead of all N levels
  price_t [3:0] bid_price0_q, ask_price0_q;
  qty_t   [3:0] bid_qty0_q,   ask_qty0_q;

  // running per-side totals from the orderbooks (only market 2 is consumed,
  // by the momentum trader) plus their pipeline registers
  localparam int SUM_WIDTH = $clog2(N)+QTY_WIDTH;
  logic [SUM_WIDTH-1:0] bid_qty_sum2, ask_qty_sum2;
  logic [SUM_WIDTH-1:0] bid_qty_sum2_q, ask_qty_sum2_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bid_price0_q <= '{default: DEFAULT_BID};
      ask_price0_q <= '{default: DEFAULT_ASK};
      bid_qty0_q   <= '0;
      ask_qty0_q   <= '0;
      bid_qty_sum2_q <= '0;
      ask_qty_sum2_q <= '0;
    end else begin
      for (int i = 0; i < 4; i++) begin
        bid_price0_q[i] <= bid_prices[i][0];
        ask_price0_q[i] <= ask_prices[i][0];
        bid_qty0_q[i]   <= bid_qtys[i][0];
        ask_qty0_q[i]   <= ask_qtys[i][0];
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

  // output pad very slow so add pipeline
  always_ff @(posedge clk or negedge rst_n) begin
      if (!rst_n) begin
          out_valid  <= '0;
          out_market <= '0;
          out_side   <= Bid;
          out_price  <= '0;
          out_qty    <= '0;
          out_error  <= '0;
          // out_spare0 is driven by a continuous assign below
      end else begin
          out_valid  <= valid_arb | valid_mom | valid_ema;
          out_market <= valid_arb ? {1'b0, market_arb} : valid_mom ? 2'd2 : 2'd3;
          out_side   <= valid_arb ? side_arb : valid_mom ? side_mom : side_ema;
          out_price  <= valid_arb ? price_arb : valid_mom ? price_mom : price_ema;
          out_qty    <= valid_arb ? qty_arb   : valid_mom ? qty_mom   : qty_ema;
          out_error  <= |error_ob | |error_trader;
      end
  end

  always_comb begin
    for (int i = 0; i < 4; i++)
      valid_ob[i] = in_valid & (msg_type_t'(in_message_type) == Public) & (in_market == 2'(i));
    valid_trader[0] = in_valid & (msg_type_t'(in_message_type) == Private) & (in_market < 2);
    valid_trader[1] = in_valid & (msg_type_t'(in_message_type) == Private) & (in_market == 2);
    valid_trader[2] = in_valid & (msg_type_t'(in_message_type) == Private) & (in_market == 3);
  end



  orderbook #(.N(N)) orderbook0_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .valid_i      (valid_ob[0]),
    .op_i         (op_t'(in_op)),
    .side_i       (side_t'(in_side)),
    .price_i      (in_price),
    .qty_i        (in_qty),

    .bid_prices_o (bid_prices[0]),
    .bid_qtys_o   (bid_qtys[0]),
    .ask_prices_o (ask_prices[0]),
    .ask_qtys_o   (ask_qtys[0]),

    .error_o      (error_ob[0])
  );

  orderbook #(.N(N)) orderbook1_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .valid_i      (valid_ob[1]),
    .op_i         (op_t'(in_op)),
    .side_i       (side_t'(in_side)),
    .price_i      (in_price),
    .qty_i        (in_qty),

    .bid_prices_o (bid_prices[1]),
    .bid_qtys_o   (bid_qtys[1]),
    .ask_prices_o (ask_prices[1]),
    .ask_qtys_o   (ask_qtys[1]),

    .error_o      (error_ob[1])
  );

  orderbook #(.N(N)) orderbook2_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .valid_i      (valid_ob[2]),
    .op_i         (op_t'(in_op)),
    .side_i       (side_t'(in_side)),
    .price_i      (in_price),
    .qty_i        (in_qty),

    .bid_prices_o (bid_prices[2]),
    .bid_qtys_o   (bid_qtys[2]),
    .ask_prices_o (ask_prices[2]),
    .ask_qtys_o   (ask_qtys[2]),
    .bid_qty_sum_o (bid_qty_sum2),
    .ask_qty_sum_o (ask_qty_sum2),

    .error_o      (error_ob[2])
  );

  orderbook #(.N(N)) orderbook3_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .valid_i      (valid_ob[3]),
    .op_i         (op_t'(in_op)),
    .side_i       (side_t'(in_side)),
    .price_i      (in_price),
    .qty_i        (in_qty),

    .bid_prices_o (bid_prices[3]),
    .bid_qtys_o   (bid_qtys[3]),
    .ask_prices_o (ask_prices[3]),
    .ask_qtys_o   (ask_qtys[3]),

    .error_o      (error_ob[3])
  );

  arb_trader arb_trader_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .bid_price0_i (bid_price0_q[0]),
    .bid_qty0_i   (bid_qty0_q[0]),
    .ask_price0_i (ask_price0_q[0]),
    .ask_qty0_i   (ask_qty0_q[0]),

    .bid_price1_i (bid_price0_q[1]),
    .bid_qty1_i   (bid_qty0_q[1]),
    .ask_price1_i (ask_price0_q[1]),
    .ask_qty1_i   (ask_qty0_q[1]),

    .order_filled_i (valid_trader[0]),
    .filled_price_i (in_price),
    .filled_qty_i   (in_qty),
    .filled_side_i  (side_t'(in_side)),

    .valid_o  (valid_arb),
    .market_o (market_arb),
    .side_o   (side_arb),
    .price_o  (price_arb),
    .qty_o    (qty_arb),
    .error_o  (error_trader[0])
  );

  momentum_trader #(.N(N)) mom_trader_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .bid_price_i (bid_price0_q[2]),
    .bid_qty_i   (bid_qty0_q[2]),
    .ask_price_i (ask_price0_q[2]),
    .ask_qty_i   (ask_qty0_q[2]),
    .bid_qty_sum_i (bid_qty_sum2_q),
    .ask_qty_sum_i (ask_qty_sum2_q),

    .order_filled_i (valid_trader[1]),
    .filled_price_i (in_price),
    .filled_qty_i   (in_qty),
    .filled_side_i  (side_t'(in_side)),

    .grant_i (grant_mom),

    .valid_o  (valid_mom),
    .side_o   (side_mom),
    .price_o  (price_mom),
    .qty_o    (qty_mom),
    .error_o  (error_trader[1])
  );

  ema_trader ema_trader_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .bid_price_i (bid_price0_q[3]),
    .bid_qty_i   (bid_qty0_q[3]),
    .ask_price_i (ask_price0_q[3]),
    .ask_qty_i   (ask_qty0_q[3]),

    .order_filled_i (valid_trader[2]),
    .filled_price_i (in_price),
    .filled_qty_i   (in_qty),
    .filled_side_i  (side_t'(in_side)),

    .grant_i (grant_ema),

    .valid_o  (valid_ema),
    .side_o   (side_ema),
    .price_o  (price_ema),
    .qty_o    (qty_ema),
    .error_o  (error_trader[2])
  );

endmodule
