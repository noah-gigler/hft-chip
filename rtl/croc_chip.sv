module croc_chip import orderbook_pkg::*; #() (
  input  wire clk_i,
  input  wire rst_ni,

  input  wire valid_i,
  input  wire market_i,
  input  wire op_i,
  input  wire side_i,
  input  wire [PRICE_WIDTH-1:0] price_i,
  input  wire [QTY_WIDTH-1:0] qty_i,

  output wire valid_o,
  output wire market_o,
  output wire side_o,
  output wire [PRICE_WIDTH-1:0] price_o,
  output wire [QTY_WIDTH-1:0] qty_o,

  output wire error_o,
  output wire spare0_o,
  output wire spare1_o,

  inout wire VDD,
  inout wire VSS,
  inout wire VDDIO,
  inout wire VSSIO
);

  wire clk;
  wire rst_n;

  wire in_valid;
  wire in_market;
  wire in_op;
  wire in_side;
  wire [PRICE_WIDTH-1:0] in_price;
  wire [QTY_WIDTH-1:0] in_qty;
  
  wire out_valid;
  wire out_market;
  wire out_side;
  wire [PRICE_WIDTH-1:0] out_price;
  wire [QTY_WIDTH-1:0] out_qty;

  wire out_error, out_spare0, out_spare1;

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_clk_i  (.pad(clk_i),  .p2c(clk));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_rst_ni (.pad(rst_ni), .p2c(rst_n));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_valid_i (.pad(valid_i), .p2c(in_valid));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_market_i (.pad(market_i), .p2c(in_market));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_op_i (.pad(op_i), .p2c(in_op));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_side_i (.pad(side_i), .p2c(in_side));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price0_i  (.pad(price_i[0]),  .p2c(in_price[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price1_i  (.pad(price_i[1]),  .p2c(in_price[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price2_i  (.pad(price_i[2]),  .p2c(in_price[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price3_i  (.pad(price_i[3]),  .p2c(in_price[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price4_i  (.pad(price_i[4]),  .p2c(in_price[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price5_i  (.pad(price_i[5]),  .p2c(in_price[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price6_i  (.pad(price_i[6]),  .p2c(in_price[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price7_i  (.pad(price_i[7]),  .p2c(in_price[7]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price8_i  (.pad(price_i[8]),  .p2c(in_price[8]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price9_i  (.pad(price_i[9]),  .p2c(in_price[9]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty0_i (.pad(qty_i[0]), .p2c(in_qty[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty1_i (.pad(qty_i[1]), .p2c(in_qty[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty2_i (.pad(qty_i[2]), .p2c(in_qty[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty3_i (.pad(qty_i[3]), .p2c(in_qty[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty4_i (.pad(qty_i[4]), .p2c(in_qty[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty5_i (.pad(qty_i[5]), .p2c(in_qty[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty6_i (.pad(qty_i[6]), .p2c(in_qty[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty7_i (.pad(qty_i[7]), .p2c(in_qty[7]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_valid_o (.pad(valid_o),  .c2p(out_valid));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_market_o (.pad(market_o), .c2p(out_market));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_side_o (.pad(side_o),  .c2p(out_side));
  
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price0_o  (.pad(price_o[0]),  .c2p(out_price[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price1_o  (.pad(price_o[1]),  .c2p(out_price[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price2_o  (.pad(price_o[2]),  .c2p(out_price[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price3_o  (.pad(price_o[3]),  .c2p(out_price[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price4_o  (.pad(price_o[4]),  .c2p(out_price[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price5_o  (.pad(price_o[5]),  .c2p(out_price[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price6_o  (.pad(price_o[6]),  .c2p(out_price[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price7_o  (.pad(price_o[7]),  .c2p(out_price[7]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price8_o  (.pad(price_o[8]),  .c2p(out_price[8]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price9_o  (.pad(price_o[9]),  .c2p(out_price[9]));

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
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA  pad_spare1_o (.pad(spare1_o), .c2p(out_spare1));

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

  assign out_error = error0 | error1;
  assign out_spare0 = 1'b0;
  assign out_spare1 = 1'b0;

  localparam int N = 8;

  price_t [N-1:0] bid_prices0;
  qty_t   [N-1:0] bid_qtys0;
  price_t [N-1:0] ask_prices0;
  qty_t   [N-1:0] ask_qtys0;

  price_t [N-1:0] bid_prices1;
  qty_t   [N-1:0] bid_qtys1;
  price_t [N-1:0] ask_prices1;
  qty_t   [N-1:0] ask_qtys1;

  logic error0;
  logic error1;

  logic valid0;
  logic valid1;

  assign valid0 = in_valid & (in_market == 1'b0);
  assign valid1 = in_valid & (in_market == 1'b1);

  orderbook #(.N(N)) orderbook0_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .valid_i      (valid0),
    .op_i         (op_t'(in_op)),
    .side_i       (side_t'(in_side)),
    .price_i      (in_price),
    .qty_i        (in_qty),

    .bid_prices_o (bid_prices0),
    .bid_qtys_o   (bid_qtys0),
    .ask_prices_o (ask_prices0),
    .ask_qtys_o   (ask_qtys0),

    .error_o      (error0)
  );

  orderbook #(.N(N)) orderbook1_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .valid_i      (valid1),
    .op_i         (op_t'(in_op)),
    .side_i       (side_t'(in_side)),
    .price_i      (in_price),
    .qty_i        (in_qty),

    .bid_prices_o (bid_prices1),
    .bid_qtys_o   (bid_qtys1),
    .ask_prices_o (ask_prices1),
    .ask_qtys_o   (ask_qtys1),

    .error_o      (error1)
  );

  trader #(.N(N)) trader_i (
    .clk_i        (clk),
    .rst_ni       (rst_n),

    .bid_prices0_i (bid_prices0),
    .bid_qtys0_i   (bid_qtys0),
    .ask_prices0_i (ask_prices0),
    .ask_qtys0_i   (ask_qtys0),

    .bid_prices1_i (bid_prices1),
    .bid_qtys1_i   (bid_qtys1),
    .ask_prices1_i (ask_prices1),
    .ask_qtys1_i   (ask_qtys1),

    .valid_o      (out_valid),
    .market_o     (out_market),
    .side_o       (out_side),
    .price_o      (out_price),
    .qty_o        (out_qty)

  );

endmodule
