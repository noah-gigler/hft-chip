#############################
## Driving Cells and Loads ##
#############################
set_load [expr 2 * 5.0 + 5.0] [all_outputs]
set_driving_cell [all_inputs] -lib_cell sg13cmos5l_IOPadOut16mA -pin pad

##################
## Input Clocks ##
##################
puts "Clocks..."
set TCK_SYS 10.0
create_clock -name clk_sys -period $TCK_SYS [get_ports clk_i]

set_clock_uncertainty 0.1 [all_clocks]
set_clock_transition  0.2 [all_clocks]

#############
## Resets  ##
#############
puts "Input/Outputs..."
set_input_delay -max [expr $TCK_SYS * 0.10] [get_ports rst_ni]
set_false_path -hold  -from [get_ports rst_ni]
set_max_delay $TCK_SYS -from [get_ports rst_ni]

######################
## HFT Inputs ##
######################
puts "HFT inputs..."
set_input_delay -min -add_delay -clock clk_sys [expr $TCK_SYS * 0.10] [get_ports {valid_i message_type_i market_i_* op_i side_i price_i_* qty_i_*}]
set_input_delay -max -add_delay -clock clk_sys [expr $TCK_SYS * 0.30] [get_ports {valid_i message_type_i market_i_* op_i side_i price_i_* qty_i_*}]

#######################
## HFT Outputs ##
#######################
puts "HFT outputs..."
set_output_delay -min -add_delay -clock clk_sys [expr $TCK_SYS * 0.10] [get_ports {valid_o market_o_* side_o price_o_* qty_o_* error_o spare0_o}]
set_output_delay -max -add_delay -clock clk_sys [expr $TCK_SYS * 0.30] [get_ports {valid_o market_o_* side_o price_o_* qty_o_* error_o spare0_o}]
