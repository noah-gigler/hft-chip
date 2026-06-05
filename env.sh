#!/bin/bash
# Copyright (c) 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Authors:
# - Philippe Sauter <phsauter@iis.ee.ethz.ch>
# - Thomas Benz     <tbenz@iis.ee.ethz.ch>
#
# Environment setup for Croc SoC ASIC flow
# This file is sourced by all scripts to set up tool paths and PDK location

# Determine repository root
if [[ -n "${BASH_SOURCE[0]}" ]]; then
    export CROC_ROOT=$(realpath $(dirname "${BASH_SOURCE[0]}"))
else
    export CROC_ROOT=$(pwd)
fi
echo "[INFO][ENV] Croc root: $CROC_ROOT"


######################
# Project Settings
######################
export PROJ_NAME="${PROJ_NAME:-croc}"
export TOP_DESIGN="${TOP_DESIGN:-croc_chip}"
export DUT_DESIGN="${DUT_DESIGN:-croc_soc}"


###################
# PDK Discovery
###################
# priority: technology/ over ihp13/pdk/

if [[ -d "${CROC_ROOT}/technology" ]]; then

    echo "[INFO][ENV] Init tech from ETHZ DZ cockpit"
    export PDK_ROOT="$CROC_ROOT/technology"
    export KLAYOUT_PATH="$CROC_ROOT/klayout/.klayout"
    export PDK_DIR_LEF_TECH="$PDK_ROOT/lef"
    export PDK_DIR_LEF_CELLS="$PDK_ROOT/lef"
    export PDK_DIR_LEF_SRAMS="$PDK_ROOT/lef"
    export PDK_DIR_LEF_IOS="$PDK_ROOT/lef"
    export PDK_DIR_LEF_BOND="$PDK_ROOT/lef"
    export PDK_DIR_GDS_CELLS="$PDK_ROOT/gds"
    export PDK_DIR_GDS_SRAMS="$PDK_ROOT/gds"
    export PDK_DIR_GDS_IOS="$PDK_ROOT/gds"
    export PDK_DIR_GDS_BOND="$PDK_ROOT/gds"

else
    echo "[WARNING][ENV] PDK not found. Set PDK_ROOT and KLAYOUT_PATH or ensure ihp13/pdk/ exists"
    export PDK_ROOT=""  # Set to empty to avoid unbound variable error
    export KLAYOUT_PATH="" # Set to empty to avoid unbound variable error
fi

echo "[INFO][ENV] PDK root: $PDK_ROOT"
echo "[INFO][ENV] KLayout path: $KLAYOUT_PATH"

export PDK=ihp-sg13g2

