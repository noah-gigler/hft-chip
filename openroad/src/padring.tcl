# Copyright (c) 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Authors:
# - Philippe Sauter <phsauter@iis.ee.ethz.ch>
# Adapted for hft_chip
#
# Pad positions are taken verbatim from the VLSI2 2026 bond diagram
# (context/bondpad_centroids.csv). They are NOT distributed by a formula:
# the QFN-64 footprint is rectangular (2416 x 1916 OR die) with 12 signal
# slots per edge and the power pads pinned at the corner clusters.
#
# place_pad -location is the lower edge of the 80 um pad cell along the row,
# so it equals (bondpad centroid along the edge) - 40. Power pad instances
# are placed at the exact net positions of the matching croc_bondpad.def
# instance, so every power-net bond lands where the tester board expects it.
#
# All pad instance names are flat strings matching hft_chip.sv.

# half the 80 um pad-cell beachfront: centroid = location + padCenter
set padCenter 40

make_io_sites -horizontal_site sg13cmos5l_ioSite \
    -vertical_site sg13cmos5l_ioSite \
    -corner_site sg13cmos5l_ioSite \
    -offset $padBond \
    -rotation_horizontal R0 \
    -rotation_vertical R0 \
    -rotation_corner R0

# place_pad helper: location derived from the bond-diagram centroid
proc place_pad_at {row centroid inst} {
    global padCenter
    place_pad -row $row -location [expr {$centroid - $padCenter}] $inst
}

##########################################################################
# Edge: WEST (x=35), top -> bottom        [PWR] vddio0, vdd0
##########################################################################
place_pad_at IO_WEST 1626 "pad_vddio0"
place_pad_at IO_WEST 1524 "pad_valid_i"
place_pad_at IO_WEST 1422 "pad_market0_i"
place_pad_at IO_WEST 1320 "pad_market1_i"
place_pad_at IO_WEST 1218 "pad_side_i"
place_pad_at IO_WEST 1116 "pad_qty0_i"
place_pad_at IO_WEST 1014 "pad_qty1_i"
place_pad_at IO_WEST  912 "pad_qty2_i"
place_pad_at IO_WEST  810 "pad_qty3_i"
place_pad_at IO_WEST  708 "pad_qty4_i"
place_pad_at IO_WEST  606 "pad_qty5_i"
place_pad_at IO_WEST  504 "pad_qty6_i"
place_pad_at IO_WEST  402 "pad_qty7_i"
place_pad_at IO_WEST  300 "pad_vdd0"

##########################################################################
# Edge: SOUTH (y=35), left -> right       [PWR] vss0,vssio1,vddio1 | vdd1,vss1,vssio2
##########################################################################
place_pad_at IO_SOUTH  290 "pad_vss0"
place_pad_at IO_SOUTH  398 "pad_vssio1"
place_pad_at IO_SOUTH  506 "pad_vddio1"
place_pad_at IO_SOUTH  614 "pad_clk_i"
place_pad_at IO_SOUTH  722 "pad_rst_ni"
place_pad_at IO_SOUTH  830 "pad_price0_i"
place_pad_at IO_SOUTH  938 "pad_price1_i"
place_pad_at IO_SOUTH 1046 "pad_price2_i"
place_pad_at IO_SOUTH 1154 "pad_price3_i"
place_pad_at IO_SOUTH 1262 "pad_price4_i"
place_pad_at IO_SOUTH 1370 "pad_price5_i"
place_pad_at IO_SOUTH 1478 "pad_price6_i"
place_pad_at IO_SOUTH 1586 "pad_price7_i"
place_pad_at IO_SOUTH 1694 "pad_price8_i"
place_pad_at IO_SOUTH 1802 "pad_op_i"
place_pad_at IO_SOUTH 1910 "pad_vdd1"
place_pad_at IO_SOUTH 2018 "pad_vss1"
place_pad_at IO_SOUTH 2126 "pad_vssio2"

##########################################################################
# Edge: EAST (x=2381), bottom -> top      [PWR] vddio2, vdd2
##########################################################################
place_pad_at IO_EAST  290 "pad_vddio2"
place_pad_at IO_EAST  392 "pad_valid_o"
place_pad_at IO_EAST  494 "pad_market0_o"
place_pad_at IO_EAST  596 "pad_market1_o"
place_pad_at IO_EAST  698 "pad_side_o"
place_pad_at IO_EAST  800 "pad_qty0_o"
place_pad_at IO_EAST  902 "pad_qty1_o"
place_pad_at IO_EAST 1004 "pad_qty2_o"
place_pad_at IO_EAST 1106 "pad_qty3_o"
place_pad_at IO_EAST 1208 "pad_qty4_o"
place_pad_at IO_EAST 1310 "pad_qty5_o"
place_pad_at IO_EAST 1412 "pad_qty6_o"
place_pad_at IO_EAST 1514 "pad_qty7_o"
place_pad_at IO_EAST 1616 "pad_vdd2"

##########################################################################
# Edge: NORTH (y=1881), right -> left     [PWR] vss2,vssio3,vddio3 | vdd3,vss3,vssio0
##########################################################################
place_pad_at IO_NORTH 2126 "pad_vss2"
place_pad_at IO_NORTH 2018 "pad_vssio3"
place_pad_at IO_NORTH 1910 "pad_vddio3"
place_pad_at IO_NORTH 1802 "pad_price0_o"
place_pad_at IO_NORTH 1694 "pad_price1_o"
place_pad_at IO_NORTH 1586 "pad_price2_o"
place_pad_at IO_NORTH 1478 "pad_price3_o"
place_pad_at IO_NORTH 1370 "pad_price4_o"
place_pad_at IO_NORTH 1262 "pad_price5_o"
place_pad_at IO_NORTH 1154 "pad_price6_o"
place_pad_at IO_NORTH 1046 "pad_price7_o"
place_pad_at IO_NORTH  938 "pad_price8_o"
place_pad_at IO_NORTH  830 "pad_error_o"
place_pad_at IO_NORTH  722 "pad_message_type_i"
place_pad_at IO_NORTH  614 "pad_spare0_o"
place_pad_at IO_NORTH  506 "pad_vdd3"
place_pad_at IO_NORTH  398 "pad_vss3"
place_pad_at IO_NORTH  290 "pad_vssio0"

place_corners $iocorner

place_io_fill -row IO_NORTH {*}$iofill
place_io_fill -row IO_SOUTH {*}$iofill
place_io_fill -row IO_WEST  {*}$iofill
place_io_fill -row IO_EAST  {*}$iofill

connect_by_abutment

place_bondpad -bond $bondPadCell -offset {5.0 -70.0} pad_*

remove_io_rows
