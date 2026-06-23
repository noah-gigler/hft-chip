module hft_chip import orderbook_pkg::*; #() (
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

  // raw combinational nets from input pads
  wire valid_i_pad;
  wire message_type_i_pad;
  wire [1:0] market_i_pad;
  wire op_i_pad;
  wire side_i_pad;
  wire [PRICE_WIDTH-1:0] price_i_pad;
  wire [QTY_WIDTH-1:0] qty_i_pad;

  // registered inputs for core
  logic valid_i_q;
  logic message_type_i_q;
  logic [1:0] market_i_q;
  logic op_i_q;
  logic side_i_q;
  logic [PRICE_WIDTH-1:0] price_i_q;
  logic [QTY_WIDTH-1:0] qty_i_q;

  // combinational outputs from core
  logic valid_o_d;
  logic [1:0] market_o_d;
  logic side_o_d;
  logic [PRICE_WIDTH-1:0] price_o_d;
  logic [QTY_WIDTH-1:0] qty_o_d;
  logic error_o_d;

  // registered nets for output pads
  logic valid_o_q;
  logic [1:0] market_o_q;
  logic side_o_q;
  logic [PRICE_WIDTH-1:0] price_o_q;
  logic [QTY_WIDTH-1:0] qty_o_q;
  logic error_o_q;
  logic spare0_o_q;

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_clk_i  (.pad(clk_i),  .p2c(clk));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_rst_ni (.pad(rst_ni), .p2c(rst_n));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_valid_i      (.pad(valid_i), .p2c(valid_i_pad));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_message_type_i (.pad(message_type_i), .p2c(message_type_i_pad));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_market0_i     (.pad(market_i[0]), .p2c(market_i_pad[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_market1_i   (.pad(market_i[1]), .p2c(market_i_pad[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_op_i         (.pad(op_i), .p2c(op_i_pad));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_side_i       (.pad(side_i), .p2c(side_i_pad));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price0_i  (.pad(price_i[0]),  .p2c(price_i_pad[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price1_i  (.pad(price_i[1]),  .p2c(price_i_pad[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price2_i  (.pad(price_i[2]),  .p2c(price_i_pad[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price3_i  (.pad(price_i[3]),  .p2c(price_i_pad[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price4_i  (.pad(price_i[4]),  .p2c(price_i_pad[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price5_i  (.pad(price_i[5]),  .p2c(price_i_pad[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price6_i  (.pad(price_i[6]),  .p2c(price_i_pad[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price7_i  (.pad(price_i[7]),  .p2c(price_i_pad[7]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_price8_i  (.pad(price_i[8]),  .p2c(price_i_pad[8]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty0_i (.pad(qty_i[0]), .p2c(qty_i_pad[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty1_i (.pad(qty_i[1]), .p2c(qty_i_pad[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty2_i (.pad(qty_i[2]), .p2c(qty_i_pad[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty3_i (.pad(qty_i[3]), .p2c(qty_i_pad[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty4_i (.pad(qty_i[4]), .p2c(qty_i_pad[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty5_i (.pad(qty_i[5]), .p2c(qty_i_pad[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty6_i (.pad(qty_i[6]), .p2c(qty_i_pad[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadIn pad_qty7_i (.pad(qty_i[7]), .p2c(qty_i_pad[7]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_valid_o  (.pad(valid_o),  .c2p(valid_o_q));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_market0_o (.pad(market_o[0]), .c2p(market_o_q[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_market1_o (.pad(market_o[1]), .c2p(market_o_q[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_side_o   (.pad(side_o),  .c2p(side_o_q));

  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price0_o  (.pad(price_o[0]),  .c2p(price_o_q[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price1_o  (.pad(price_o[1]),  .c2p(price_o_q[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price2_o  (.pad(price_o[2]),  .c2p(price_o_q[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price3_o  (.pad(price_o[3]),  .c2p(price_o_q[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price4_o  (.pad(price_o[4]),  .c2p(price_o_q[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price5_o  (.pad(price_o[5]),  .c2p(price_o_q[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price6_o  (.pad(price_o[6]),  .c2p(price_o_q[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price7_o  (.pad(price_o[7]),  .c2p(price_o_q[7]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_price8_o  (.pad(price_o[8]),  .c2p(price_o_q[8]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty0_o (.pad(qty_o[0]), .c2p(qty_o_q[0]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty1_o (.pad(qty_o[1]), .c2p(qty_o_q[1]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty2_o (.pad(qty_o[2]), .c2p(qty_o_q[2]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty3_o (.pad(qty_o[3]), .c2p(qty_o_q[3]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty4_o (.pad(qty_o[4]), .c2p(qty_o_q[4]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty5_o (.pad(qty_o[5]), .c2p(qty_o_q[5]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty6_o (.pad(qty_o[6]), .c2p(qty_o_q[6]));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA pad_qty7_o (.pad(qty_o[7]), .c2p(qty_o_q[7]));

  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA  pad_error_o (.pad(error_o), .c2p(error_o_q));
  (* dont_touch = "true" *) sg13cmos5l_IOPadOut16mA  pad_spare0_o (.pad(spare0_o), .c2p(spare0_o_q));

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

  localparam int N = 64;

  // input boundary register: pad -> core
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      valid_i_q        <= '0;
      message_type_i_q <= '0;
      market_i_q       <= '0;
      op_i_q           <= '0;
      side_i_q         <= '0;
      price_i_q        <= '0;
      qty_i_q          <= '0;
    end else begin
      valid_i_q        <= valid_i_pad;
      message_type_i_q <= message_type_i_pad;
      market_i_q       <= market_i_pad;
      op_i_q           <= op_i_pad;
      side_i_q         <= side_i_pad;
      price_i_q        <= price_i_pad;
      qty_i_q          <= qty_i_pad;
    end
  end

  hft_core #(.N(N)) core_i (
    .clk_i   (clk),
    .rst_ni  (rst_n),

    .valid_i        (valid_i_q),
    .message_type_i (message_type_i_q),
    .market_i       (market_i_q),
    .op_i           (op_i_q),
    .side_i         (side_i_q),
    .price_i        (price_i_q),
    .qty_i          (qty_i_q),

    .valid_o  (valid_o_d),
    .market_o (market_o_d),
    .side_o   (side_o_d),
    .price_o  (price_o_d),
    .qty_o    (qty_o_d),
    .error_o  (error_o_d)
  );

  assign spare0_o_q = 1'b0;

  // output boundary register: core -> pad
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      valid_o_q  <= '0;
      market_o_q <= '0;
      side_o_q   <= Bid;
      price_o_q  <= '0;
      qty_o_q    <= '0;
      error_o_q  <= '0;
    end else begin
      valid_o_q  <= valid_o_d;
      market_o_q <= market_o_d;
      side_o_q   <= side_o_d;
      price_o_q  <= price_o_d;
      qty_o_q    <= qty_o_d;
      error_o_q  <= error_o_d;
    end
  end

endmodule
