# Course Information

Course Information
Course Catalogue: 227-0147-00 L
Lecturer: Frank K. Gürkaynak, Luca Benini
Coordinator:  Enrico Zelioli, Philippe Sauter
Language: English
Hours: 5G
Credits: 6
Grading: graded semester performance
Course Website: vlsi.ethz.ch
Lecture:  Tue 14:00-16:00 (LFW B 1). See below for the detailed lecture schedule
Exercises: Wed 09:00-12:00 (ETZ D61.1/D61.2). See below for the exercise schedule and task descriptions
Questions and Contact: Use the Moodle Forum for general questions (anonymous). This way others can also benefit from it. For personal matters drop us a mail at vlsi2@iis.ee.ethz.ch

# Course description

This second course in our VLSI series is concerned with how to turn digital
circuit netlists into safe, testable and manufacturable mask layout, taking
into account various parasitic effects. Since 2025, the course relies on
predominantly open-source tools and puts more emphasis on hands-on design. All
students are expected to complete their own design (in groups of two).

Learning objectives:

- Understand how VLSI circuits are designed
- Gain practical experience in IC Design using open source tools
- Qualify to take part in semester/master theses that involve practical IC Design
- Develop your own System-on-Chip based on the examples in exercises

Topics:

- Basic manufacturing steps
- Standard cells, routing layers
- Floorplanning, I/O ring, packaging
- Timing in IC design and clock dsirtribution
- Parasitic effects in IC Design
- Placement and routing
- Power analysis
- Testing of IC circuits
- Assessing the performance of ICs

The most important part of the lecture is that the exercises, which will make
use of open-source tools (as much as possible) and work on a System-On-Chip
design. The exercises are essential for the lecture as the grading will be done
based on a project based on the exercises.


# Lecture & Exercise Schedule

Week	Lecture	Exercise
 	Date	Topic	Date	Topic
1	17.02	Introduction and Motivation	18.02	1: Simulation Flow for ASIC Designs
2	24.02	Refresher on System Verilog	25.02	2: Drawing Block Diagrams
3	03.03	Synthesis flow, Croc architecture	04.03	3: Understanding Croc
4	10.03	IC manufacturing	11.03	4: Synthesis
5	17.03	Metal layers and standard cells	18.03	5: Introduction to OpenROAD
6	24.03	Floorplanning, I/Os, packaging	25.03	6: Floorplanning
7	31.03	Final project discussion, Q&A	01.04	No exercise
8	07.04	Easter break	08.04	Easter break
9	14.04	Timing	15.04	7: Placement / timing
10	21.04	Parasitics, extraction	22.04	8: Understanding clock tree / timing
11	28.04	Placement and routing	29.04	9: Routing, finishing
12	05.05	Power analysis	06.05	10: Power analysis / IR drop
13	12.05	Testing	13.05	11: DRC / LVS
14	19.05	Reporting performance, power, area	20.05	12: Design for testability / ATPG
15	26.05	Final lecture	27.05	13: Preparing plots, presenting results


# Projects

Students will be asked to complete their own System-On-Chip design with an
improvement based on the example design used in exercises, and they will be
graded on the quality/timeliness of the design.

As the final project is based on the exercises, we strongly suggest that
students visit all exercises. The students will work in groups of 2 people,
they will take the exercise design, and will be given a larger area to make one
addition/improvement in the design. Submissions will be made electronically,
grade will depend on timeliness (respecting the submission deadlines),
functionality, performance and originality. You can find more information about
the final projects on the first deck of lecture slides.

At least 5 selected designs will be actually fabricated.

## Submission guidelines

This section contains all the information required to successfully complete
and submit your final projects. Submissions that do not comply to the following
instructions will be penalized with a 0.25 deduction on the final grade. Please
read it carefully, and do not hesitate to ask questions if something is unclear.

The submission deadline is 30 June 2026 10:00am CEST.

Project submissions are collected through Nextcloud. Each group should submit
as a single compressed archive, named according to the following naming
conventions:

groupUsername.tar.gz

where groupUsername is the name of the VLSI 2 account you used to work on the project (e.g. group 13 -> vlsi2_13fs26).

Example: Group 7 has been working on account vlsi2_07fs26. Their submission should be named vlsi2_07fs26.tar.gz.

The final submission should contain the following files / directories (follows the directory structure in the reference flow):

- Your final report as a single PDF file, named with the same convention of the submission archive (e.g. vlsi2_07fs25.pdf).
- The .cockpitrc used for your project.
- The openroad/out directory, containing the following files:
  - croc_lvs.v
  - croc.def
  - croc.odb
  - croc.sdc
  - croc.v
- The klayout/out directory, with a croc.gds file containing the final GDSII
  of your chip. You do NOT need to run sealring insertion and metal filling,
  you should submit the GDS file converted from the OpenROAD's DEF output with
  KLayout.
- The sw directory. You can include the original tests as provided in the
  reference flow, add your own tests, and, in case changes that prevent simulation
  output from the chip (e.g. changes to UART), a collection of tests that
  implement this communication.
- The rtl directory, including the modifications required for your design.

Note that to achieve all the submission points it is required that you submit
all the requested files in a directory structure as described above.

Example of submission directory structure for group 07 (i.e. content of the vlsi2_07fs26.tar.gz archive):

├── vlsi2_07fs26.pdf
├── .cockpitrc
└── klayout/
    └── out/
        └── croc.gds
└── openroad/
    └── out/
        ├── croc_lvs.v
        ├── croc.def
        ├── croc.odb
        ├── croc.sdc
        └── croc.v
├── rtl/
    ├── ...
├── sw/
    ├── ...


## Report

As part of your submission, you are required to provide a final report. The
report is a PDF document containing a detailed description of your project
(design, implementation, testing, results) as it will be the main reference for
us to grade you. It should include a description of your RTL modifications, with
proper documentation on the design. For example, if for your project you added
a small peripheral to Croc's user domain, you should provide block diagrams
to describe the architecture, describe the functionality of each module, and
clarify the relationships between submodules. The description should be concise
yet clear, with possible references to the RTL code if needed (assuming the code
is readable and well documented). Together with the design, you are expected
to deliver tests to functionally verify the implementation. Moreover, it is
expected that you provide a working setup to evaluate your implementation. For
example, if your project is about hardware acceleration of a specific operation,
you should provide tests that can exploit it, as well as an evaluation of the
achieved speedup. Last but not least, the report must include details of the
implementation. Rather than a chronological record of activities, this part
should describe the challenges that you faced during the backend implementation,
explaining the choices made to overcome them.

The report should be in PDF format and not exceed 16 pages. Other than that,
there are no strict requirements on the format (no template is given). There is
no minimal number of pages.

## Grading

A timely submission will be worth 3.00 points.
Every started week of delay will be -0.50. For example, if you submit your
design one minute or six days after the deadline, you will either way loose 0.5
grades. Starting from seven days of delay, the penalty will increase to -1.0
grade, and so on.
Submissions not following instructions for formatting will incur a penalty of -0.25.

Following bonusses will apply:

+0.50: 1 page outline with block diagram submitted and approved (to be submitted by May 22)
+0.50: Design passes DRC and LVS (i.e. it is good for manufacturing)
+0.25: Design runs a standard simulation of our choice successfully
+1.00: Technical content of the report (design improvement and evaluation of results)
+0.50: Quality of the report
+0.25: Design is chosen for manufacturing

## Tapeouts

As previously announced, at least 5 designs will be selected for manufacturing.
For projects willing to be taped-out, it is a requirement that designs are
open-sourced. We are currently organizing a way to collect your designs in an
open-source archive. More details on how these will be collected will follow
by the end of the semester. The selected designs will be announced after all
projects have been submitted and graded.


## Office hours

We will offer an office hour every Monday from 4pm to 6pm in ETZ H71, starting on Monday, May 4.

## Implementation guidelines

Functional requirements: It is expected that the final chip has the following Verilog ports: uart_rx_i, uart_tx_o, testmode_i, status_o, clk_i, ref_clk_i, rst_ni, jtag_trst_ni, jtag_tms_i, jtag_tdi_i, jtag_tdo_o. The setup that will be used to test your chip will assume those ports to exists, hence not having some of them will likely result in not passing our testbench. If the ports exist unmodified we will run tests against it to verify functionality of the Croc domain. Any failing test should be explainable from the report (i.e. removed or changed some peripheral, with explanation of the reasons for such choice).
Further, the chip must have a way to output the names of the students in the group. The default way that will be checked automatically is to read from the 0x2000_0000 address (i.e. the start of Croc's user domain) and expect a zero terminated string. This string will be read via UART. If this is not possible due to modifications, a testbench (along possible compiled software) must be provided as part of your submission, clearly documented in the report, and reproducible. It is required that it captures output from the chip, collects it in the testbench and prints it to the console.

Physical requirements: The final chip dimensions and pad locations must match the provided bond diagram. This allows to reuse the bonding pad and sealring already used in a previous tapeout of Croc (MLEM), giving us the opportunity to tape select multiple chips for tapeout while keeping the effort sustainable. The chip size including the sealring is 2500um (wide) x 2000um (high) . If you have special reasons that could require an exception to this rule, both a supervisor from the course and DZ must be contacted and agree upon a different finished size and pad locations.

Pins: You are allowed to change pin assignment but not the bonding pad positions, except for the provided power pins which must stay exactly in the same place (required for compatibility with tester board). Furthermore, we highly recommend to not change the positions of the following pins unless it is absolutely required to accomplish some specific goal:

JTAG signals (tdi_i, tdo_o, tms_i, trst_i)
Clock, reset, and test signals (clk_i, ref_clk_i, rst_ni, testmode_i)
UART signals (uart_rx_i, uart_tx_o)
Also in this case, if you think you cannot avoid to change the position of one of these pins, or you need more for your design, make sure to talk to an assistant first.

Pinout description

Pinout specification for the final VLSI 2 projects

The bondpad_centroids.csv files contains the exact coordinates of the center of each bonding pad, as they should be configured in OpenROAD. Note that by default, OpenROAD displays the bottom left corner coordinates of design objects, so be careful to also account for the bondpad size (70um x 70um) when comparing them to the ones of your designs.

The croc_bondpad.def is a DEF file exported from OpenROAD with only the pads placed in the correct positions, as specified by the coordinates above. You can use this as a reference to compare your designs.

The bonding_vlsi2_2026.pdf is an illustration of how the student designs sent for tapeout will be bonded inside the package.
