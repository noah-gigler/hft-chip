# How to run DRC with KLayout

Note: you can change the tools version by specifying it to the `oseda` command (no arguments == latest release). For checking DRCs with KLayout, use `oseda -2026.04` (current default) or later for a significantly improved runtime with respect to previous versions.

1. Generate GDS file from OpenROAD's DEF
2. Run DRC with KLayout

```bash
$ cd klayout
$ oseda bash
oseda-2026.04:[klayout] ./def2gds-croc
oseda-2026.04:[klayout] ./run_drc-croc
```
