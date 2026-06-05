#! /bin/csh -f

source ../.setPDK.csh

set topcell = croc_chip
set gdspath = ./out/croc.gds
set outdir  = ./drc/out

if ( -e ${outdir} ) then
    echo "Removing existing directory: ${outdir}"
    rm -rf ${outdir}
endif

mkdir -p ${outdir}

#usage:
#    run_drc.py (--help | -h)
#    run_drc.py --path=<file_path>
#            [--table=<table_name>]... [--mp=<num_cores>] [--run_dir=<run_dir_path>]
#            [--topcell=<topcell_name>] [--run_mode=<mode>] [--drc_json=<json_path>]
#            [--precheck_drc] [--disable_extra_rules] [--no_feol] [--no_beol] [--no_density]
#            [--density_thr=<density_threads>] [--density_only] [--antenna]
#            [--antenna_only] [--no_offgrid] [--no_recommended]

python3 ${IHP_TECH}/klayout/tech/drc/run_drc.py \
 --path=${gdspath} \
 --topcell=${topcell} \
 --run_dir ${outdir} \
 --no_recommended \
 --no_density \
 --antenna \
 --mp 32
