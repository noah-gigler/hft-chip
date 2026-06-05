# Copyright 2023 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Authors:
# - Tobias Senti <tsenti@ethz.ch>
# - Jannis Schönleber <janniss@iis.ee.ethz.ch>
# - Philippe Sauter   <phsauter@iis.ee.ethz.ch>

# Power planning

utl::report "Power Grid"
# ToDo: Check connectivity on left and right power pad cells
source scripts/floorplan_util.tcl

##########################################################################
# Reset
##########################################################################

if {[info exists power_grid_defined]} {
    pdngen -ripup
    pdngen -reset
} else {
    set power_grid_defined 1
}

##########################################################################
# Power settings
##########################################################################
# Core Power Ring
## Space between pads and core -> used for power ring
set PowRingSpace  35
## Spacing must meet TM1 rules
set pgcrSpacing 4
## Width must meet TM1 rules
set pgcrWidth 8
## Offset from core to power ring
set pgcrOffset [expr ($PowRingSpace - $pgcrSpacing - 2 * $pgcrWidth) / 2]

# TopMetal1 Core Power Grid
set tpg1Width     3; # arbitrary number
set tpg1Pitch    90; # multiple of pad-pitch
set tpg1Spacing  10; # big enough to skip over a pad
set tpg1Offset   70; # offset from leftX of core

set pg4Width      1; # two tracks on Metal4
set pg4Pitch     90; # multiple of pad-pitch

# Macro Power Rings -> M3 and M2
## Spacing must be larger than pitch of M2/M3
set mprSpacing 0.6
## Width
set mprWidth  1.05
## Offset from Macro to power ring
set mprOffsetX 2.4
set mprOffsetY 1.0

##########################################################################
# SRAM power rings
##########################################################################
proc sram_power { name macro } {
    global mprWidth mprSpacing mprOffsetX mprOffsetY mpgWidth mpgSpacing
    # Macro Grid and Rings
    define_pdn_grid -macro -cells $macro -name ${name}_grid -orient "R0 R180 MY MX" \
        -grid_over_boundary -voltage_domains {CORE} \
        -halo {1 1}

    add_pdn_ring -grid ${name}_grid \
        -layer        {Metal3 Metal4} \
        -widths       "$mprWidth $mprWidth" \
        -spacings     "$mprSpacing $mprSpacing" \
        -core_offsets "$mprOffsetX $mprOffsetY" \
        -add_connect

    # Connection of Macro Power Ring to standard-cell rails
    add_pdn_connect -grid ${name}_grid -layers {Metal3 Metal2}
    add_pdn_connect -grid ${name}_grid -layers {Metal4 Metal1}
    # Connection of Stripes on Macro to Core Power Stripes
    add_pdn_connect -grid ${name}_grid -layers {TopMetal1 Metal4}
}


##########################################################################
# Core Power
##########################################################################
# Top 1 - Top 2
add_pdn_ring -grid {core_grid} \
   -layer        {TopMetal1 Metal4} \
   -widths       "$pgcrWidth $pgcrWidth" \
   -spacings     "$pgcrSpacing $pgcrSpacing" \
   -core_offsets "$pgcrOffset $pgcrOffset" \
   -add_connect                        \
   -connect_to_pads

# M1 Standardcell Rows (tracks)
add_pdn_stripe -grid {core_grid} -layer {Metal1} -width {0.32} -offset {0} \
               -followpins -extend_to_core_ring

sram_power "sram_512x32"  "RM_IHPSG13_1P_512x32_c2_bm_bist"

# Top power grid
# Top 1 Stripe
add_pdn_stripe  -grid {core_grid} -layer {TopMetal1} -width $tpg1Width \
                -pitch $tpg1Pitch -spacing $tpg1Spacing -offset $tpg1Offset \
                -extend_to_core_ring -snap_to_grid

add_pdn_stripe  -grid {core_grid} -layer {Metal4} -width $pg4Width \
                -pitch $pg4Pitch -extend_to_core_ring -snap_to_grid

# "The add_pdn_connect command is used to define which layers in the power grid are to be connected together.
#  During power grid generation, vias will be added for overlapping power nets and overlapping ground nets."
# vertical TopMetal1 to below horizonals (M1 has horizontal power tracks)
add_pdn_connect -grid {core_grid} -layers {Metal4 Metal1}

##########################################################################
# Generate
##########################################################################
pdngen -failed_via_report ${report_dir}/01_${proj_name}_pdngen.rpt
