# Copyright (c) 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Authors:
# - Philippe Sauter <phsauter@iis.ee.ethz.ch>
# Adapted for orderbook_chip
#
# All pad instance names are flat strings matching croc_chip.sv.

set numPadsPerEdge 16
set cornerToPad [expr {$padBond + $padD}]

make_io_sites -horizontal_site sg13cmos5l_ioSite \
    -vertical_site sg13cmos5l_ioSite \
    -corner_site sg13cmos5l_ioSite \
    -offset $padBond \
    -rotation_horizontal R0 \
    -rotation_vertical R0 \
    -rotation_corner R0

##########################################################################
# Edge: WEST
##########################################################################
set westSpan  [expr {$chipH - 2*$cornerToPad - $padW}]
set westPitch [expr {floor($westSpan / double($numPadsPerEdge - 1))}]
set westStart [expr {$chipH - $cornerToPad - $padW}]

place_pad -row IO_WEST -location [expr {$westStart -  0*$westPitch}] "pad_vssio0"
place_pad -row IO_WEST -location [expr {$westStart -  1*$westPitch}] "pad_vddio0"
place_pad -row IO_WEST -location [expr {$westStart -  2*$westPitch}] "pad_valid_i"
place_pad -row IO_WEST -location [expr {$westStart -  3*$westPitch}] "pad_market_i"
place_pad -row IO_WEST -location [expr {$westStart -  4*$westPitch}] "pad_op_i"
place_pad -row IO_WEST -location [expr {$westStart -  5*$westPitch}] "pad_side_i"
place_pad -row IO_WEST -location [expr {$westStart -  6*$westPitch}] "pad_qty0_i"
place_pad -row IO_WEST -location [expr {$westStart -  7*$westPitch}] "pad_qty1_i"
place_pad -row IO_WEST -location [expr {$westStart -  8*$westPitch}] "pad_qty2_i"
place_pad -row IO_WEST -location [expr {$westStart -  9*$westPitch}] "pad_qty3_i"
place_pad -row IO_WEST -location [expr {$westStart - 10*$westPitch}] "pad_qty4_i"
place_pad -row IO_WEST -location [expr {$westStart - 11*$westPitch}] "pad_qty5_i"
place_pad -row IO_WEST -location [expr {$westStart - 12*$westPitch}] "pad_qty6_i"
place_pad -row IO_WEST -location [expr {$westStart - 13*$westPitch}] "pad_qty7_i"
place_pad -row IO_WEST -location [expr {$westStart - 14*$westPitch}] "pad_vss0"
place_pad -row IO_WEST -location [expr {$westStart - 15*$westPitch}] "pad_vdd0"

##########################################################################
# Edge: SOUTH
##########################################################################
set southSpan  [expr {$chipW - 2*$cornerToPad - $padW}]
set southPitch [expr {floor($southSpan / double($numPadsPerEdge - 1))}]
set southStart $cornerToPad

place_pad -row IO_SOUTH -location [expr {$southStart +  0*$southPitch}] "pad_vssio1"
place_pad -row IO_SOUTH -location [expr {$southStart +  1*$southPitch}] "pad_vddio1"
place_pad -row IO_SOUTH -location [expr {$southStart +  2*$southPitch}] "pad_clk_i"
place_pad -row IO_SOUTH -location [expr {$southStart +  3*$southPitch}] "pad_rst_ni"
place_pad -row IO_SOUTH -location [expr {$southStart +  4*$southPitch}] "pad_price0_i"
place_pad -row IO_SOUTH -location [expr {$southStart +  5*$southPitch}] "pad_price1_i"
place_pad -row IO_SOUTH -location [expr {$southStart +  6*$southPitch}] "pad_price2_i"
place_pad -row IO_SOUTH -location [expr {$southStart +  7*$southPitch}] "pad_price3_i"
place_pad -row IO_SOUTH -location [expr {$southStart +  8*$southPitch}] "pad_price4_i"
place_pad -row IO_SOUTH -location [expr {$southStart +  9*$southPitch}] "pad_price5_i"
place_pad -row IO_SOUTH -location [expr {$southStart + 10*$southPitch}] "pad_price6_i"
place_pad -row IO_SOUTH -location [expr {$southStart + 11*$southPitch}] "pad_price7_i"
place_pad -row IO_SOUTH -location [expr {$southStart + 12*$southPitch}] "pad_price8_i"
place_pad -row IO_SOUTH -location [expr {$southStart + 13*$southPitch}] "pad_price9_i"
place_pad -row IO_SOUTH -location [expr {$southStart + 14*$southPitch}] "pad_vss1"
place_pad -row IO_SOUTH -location [expr {$southStart + 15*$southPitch}] "pad_vdd1"

##########################################################################
# Edge: EAST
##########################################################################
set eastSpan  [expr {$chipH - 2*$cornerToPad - $padW}]
set eastPitch [expr {floor($eastSpan / double($numPadsPerEdge - 1))}]
set eastStart $cornerToPad

place_pad -row IO_EAST -location [expr {$eastStart +  0*$eastPitch}] "pad_vssio2"
place_pad -row IO_EAST -location [expr {$eastStart +  1*$eastPitch}] "pad_vddio2"
place_pad -row IO_EAST -location [expr {$eastStart +  2*$eastPitch}] "pad_valid_o"
place_pad -row IO_EAST -location [expr {$eastStart +  3*$eastPitch}] "pad_market_o"
place_pad -row IO_EAST -location [expr {$eastStart +  4*$eastPitch}] "pad_side_o"
place_pad -row IO_EAST -location [expr {$eastStart +  5*$eastPitch}] "pad_qty0_o"
place_pad -row IO_EAST -location [expr {$eastStart +  6*$eastPitch}] "pad_qty1_o"
place_pad -row IO_EAST -location [expr {$eastStart +  7*$eastPitch}] "pad_qty2_o"
place_pad -row IO_EAST -location [expr {$eastStart +  8*$eastPitch}] "pad_qty3_o"
place_pad -row IO_EAST -location [expr {$eastStart +  9*$eastPitch}] "pad_qty4_o"
place_pad -row IO_EAST -location [expr {$eastStart + 10*$eastPitch}] "pad_qty5_o"
place_pad -row IO_EAST -location [expr {$eastStart + 11*$eastPitch}] "pad_qty6_o"
place_pad -row IO_EAST -location [expr {$eastStart + 12*$eastPitch}] "pad_qty7_o"
place_pad -row IO_EAST -location [expr {$eastStart + 13*$eastPitch}] "pad_spare0_o"
place_pad -row IO_EAST -location [expr {$eastStart + 14*$eastPitch}] "pad_vss2"
place_pad -row IO_EAST -location [expr {$eastStart + 15*$eastPitch}] "pad_vdd2"

##########################################################################
# Edge: NORTH
##########################################################################
set northSpan  [expr {$chipW - 2*$cornerToPad - $padW}]
set northPitch [expr {floor($northSpan / double($numPadsPerEdge - 1))}]
set northStart [expr {$chipW - $cornerToPad - $padW}]

place_pad -row IO_NORTH -location [expr {$northStart -  0*$northPitch}] "pad_vssio3"
place_pad -row IO_NORTH -location [expr {$northStart -  1*$northPitch}] "pad_vddio3"
place_pad -row IO_NORTH -location [expr {$northStart -  2*$northPitch}] "pad_price0_o"
place_pad -row IO_NORTH -location [expr {$northStart -  3*$northPitch}] "pad_price1_o"
place_pad -row IO_NORTH -location [expr {$northStart -  4*$northPitch}] "pad_price2_o"
place_pad -row IO_NORTH -location [expr {$northStart -  5*$northPitch}] "pad_price3_o"
place_pad -row IO_NORTH -location [expr {$northStart -  6*$northPitch}] "pad_price4_o"
place_pad -row IO_NORTH -location [expr {$northStart -  7*$northPitch}] "pad_price5_o"
place_pad -row IO_NORTH -location [expr {$northStart -  8*$northPitch}] "pad_price6_o"
place_pad -row IO_NORTH -location [expr {$northStart -  9*$northPitch}] "pad_price7_o"
place_pad -row IO_NORTH -location [expr {$northStart - 10*$northPitch}] "pad_price8_o"
place_pad -row IO_NORTH -location [expr {$northStart - 11*$northPitch}] "pad_price9_o"
place_pad -row IO_NORTH -location [expr {$northStart - 12*$northPitch}] "pad_spare1_o"
place_pad -row IO_NORTH -location [expr {$northStart - 13*$northPitch}] "pad_spare2_o"
place_pad -row IO_NORTH -location [expr {$northStart - 14*$northPitch}] "pad_vss3"
place_pad -row IO_NORTH -location [expr {$northStart - 15*$northPitch}] "pad_vdd3"

place_corners $iocorner

place_io_fill -row IO_NORTH {*}$iofill
place_io_fill -row IO_SOUTH {*}$iofill
place_io_fill -row IO_WEST  {*}$iofill
place_io_fill -row IO_EAST  {*}$iofill

connect_by_abutment

place_bondpad -bond $bondPadCell -offset {5.0 -70.0} pad_*

remove_io_rows
