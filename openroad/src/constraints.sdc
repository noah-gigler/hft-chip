# Backend constraints for orderbook_chip

#############################
## Driving Cells and Loads ##
#############################
set_load [expr 2 * 5.0 + 5.0] [all_outputs]
set_driving_cell [all_inputs] -lib_cell sg13cmos5l_IOPadOut16mA -pin pad

##################
## Input Clocks ##
##################
puts "Clocks..."
# Target 100 MHz
set TCK_SYS 10.0
create_clock -name clk_sys -period $TCK_SYS [get_ports clk_i]

# No second clock domain -- clock groups and CDC constraints removed

set_clock_uncertainty 0.1 [all_clocks]
set_clock_transition  0.2 [all_clocks]

#############
## Resets  ##
#############
puts "Input/Outputs..."
set_false_path -hold  -from [get_ports rst_ni]
set_max_delay $TCK_SYS -from [get_ports rst_ni]

######################
## Orderbook Inputs ##
######################
puts "Orderbook inputs..."
set_input_delay -min -add_delay -clock clk_sys [expr $TCK_SYS * 0.10] \
    [get_ports {op_i* order_type_i* price_i* qty_i* valid_i}]
set_input_delay -max -add_delay -clock clk_sys [expr $TCK_SYS * 0.30] \
    [get_ports {op_i* order_type_i* price_i* qty_i* valid_i}]

#######################
## Orderbook Outputs ##
#######################
puts "Orderbook outputs..."
set_output_delay -min -add_delay -clock clk_sys [expr $TCK_SYS * 0.10] [get_ports arb_o]
set_output_delay -max -add_delay -clock clk_sys [expr $TCK_SYS * 0.30] [get_ports arb_o]

##########
## Spare #
##########
set_false_path -from [get_ports spare_i*]
