package orderbook_pkg;
    localparam int unsigned PRICE_WIDTH = 10;
    localparam int unsigned QTY_WIDTH   = 8;
    typedef logic [PRICE_WIDTH-1:0] price_t;
    typedef logic [QTY_WIDTH-1:0]   qty_t;
    typedef enum logic {Insert, Remove} op_t;
    typedef enum logic {Bid, Ask} side_t;
    localparam price_t DEFAULT_BID = '0; // min value
    localparam price_t DEFAULT_ASK = '1; // max value
endpackage

