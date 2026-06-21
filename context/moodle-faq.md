FAQs
Reference flow installation
Problem:

You do not know where to find the reference flow.

Answer:

Install the reference flow by typing the following command in a terminal:

/home/vlsi2/reference_flow/install.sh

This will copy the reference flow files in a reference_flow folder in your currently working directory. Once copied, navigate to the newly created directory and install the technology files with cockpit:

cd reference_flow
icdesign ihp13 -update all -nogui

You can then start working on your project by modifying the design and implementation scripts in this directory.

Note: if you started working on the Github version of Croc: switch as soon as possible to the reference flow, as the technology setup and scripts on Github are not aligned with the PDK version used for this year's takeouts. There should be no issues if your designed was based on the Github's Croc, as the RTL is almost identical to the one of the reference flow. Any changes you made to the RTL or flow should be easily portable to the reference flow provided.

M1 pin offgrid violations
Problem:

KLayout reports hundreds of M1 pin offgrid (metal1_pin_Offgrid) violations.

Answer:

There is an issue in the GDS files of pads and SRAM macros provided by IHP. Inside these macros there are M1 pin shapes which are not aligned to the 5nm manufacturing grid. In practice, this is not an issue as these are not physical shapes, and therefore can be safely ignored. In your DRC report, you can ignore any metal1_pin_Offgrid inside the SRAM macros and in the pads.

Note that you can also disable the off-grid checks with the a dedicated option of KLayout (--no_offgrid). You can do this for convenience when iterating on DRC fixing, but remember to have a run at the end with these checks enabled to make sure the violations are only occurring inside the macros.

June 11 update:

For what concerns the pads, we were able to generate a patched version of the GDS which should solve this issue. Although this is not required, if you simply re-run the GDS generation step from now on you should not see this issue anymore inside the pads. For what concerns the memory macros, there is no easy way to patch all of the violations, and we will have to wait for IHP to release new fixed versions for this to disappear. Anyways, as stated before this is not a real issue and can just be ignored.

QuestaSim errors
Problem:

Error while running the run_vsim.sh script for RTL or post-layout simulations with QuestaSim.

Answer:

Check the following things:

You are running QuestaSim (or the run_vsim.sh script) from a bash shell, not within oseda.
You are using a compatible version of QuestaSim (available versions are questa-2019.3, questa-2022.3, questa-2023.4).
You have adapted the compile_rtl.tcl, compile_netlist.tcl, compile_tech.tcl scripts to match your designs. Any SystemVerilog file you developed for your designs should be included in the RTL and netlist compilation scripts, and the compile_tech script should be adapted to include the Verilog behavioral models of any macro cell used to implement your design (these are typically available in the technology/verilog directory).
If you use Bender to generate compilation scripts for QuestaSim (so also if you used run_vsim.sh --flist), make sure to have adapted the Bender.yml manifest to also include your additional source files. Note that you do not need to use Bender for this, and for simple changes writing a script by hand is probably the easiest option.
GDS generation with KLayout
Problem:

You run the def2gds-croc script but nothing is printed on the terminal and no file is generated in the klayout/out directory.

Answer:

Check the following things:

The input DEF file path is correct and the DEF file exported from OpenROAD is valid (e.g. not empty, human readable file).
The top module name of your design matches the one specified in the script (e.g. croc_chip).
You are including the GDSes of all used macros (defined by the in_files variable). This is for sure an issue if you added macros in the design which were not used by the reference design / flow (e.g. different SRAM configurations than 512x32).
If you still have issues, you might be missing the definition of the lef_files variable. Depending on when you installed the reference flow, you might have to add the following option to KLayout when running the def2stream.py script to generate the GDS: " -rd lef_files='' \"
Sealring and metal filling
Problem:

You are not sure if / how to run seal ring insertion and metal filling.

Answer:

You do not have to run seal ring insertion and metal filling for your projects submissions. These will be handled in a post-processing phase by us before sending the selected chips for manufacturing. We expect you to submit the GDS file generated with KLayout with the def2gds-croc script.

If you are curious, feel free to ask questions to a TA during an office hour on more details and examples on these post-processing steps. 

DRC checks
Problem:

You are having issues with unusual DRC errors reported.

Answer:

Check that the right options are enabled. You should be able to run the run_drc-croc script in the reference flow unmodified. If this reports no issues (except for known issues such as the off-gird M1 pin shapes inside the SRAM macros).

In particular, you should disable density and recommended checks (--no_density, --no_recommended), and include antenna checks (--antenna), as shown in the reference script.
