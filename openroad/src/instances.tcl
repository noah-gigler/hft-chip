# Copyright 2024 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Authors:
# - Philippe Sauter <phsauter@iis.ee.ethz.ch>

# Automatic collection of SRAMs and delay-line macros
# Used for automatic macro placement
# set macros [list]

# set srams [get_cells *RM_IHP*]
# foreach inst $srams {
#     lappend macros $inst
# }


# Macro names as produced by the yosys synthesis
# Used for manual macro placement

set CORE            i_core
