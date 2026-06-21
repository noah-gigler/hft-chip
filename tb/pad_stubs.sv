// Behavioral stubs for the IHP SG13G2 IO pad cells, for chip-level simulation.
// The real cells are analog/hard macros; functionally each signal pad is a wire
// buffer between the package pad and the core. Power pads are no-ops in sim.

module sg13cmos5l_IOPadIn (input wire pad, output wire p2c);
  assign p2c = pad;
endmodule

module sg13cmos5l_IOPadOut16mA (output wire pad, input wire c2p);
  assign pad = c2p;
endmodule

module sg13cmos5l_IOPadVdd   (); endmodule
module sg13cmos5l_IOPadVss   (); endmodule
module sg13cmos5l_IOPadIOVdd (); endmodule
module sg13cmos5l_IOPadIOVss (); endmodule
