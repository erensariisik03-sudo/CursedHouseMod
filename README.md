# Farming Simulator 14 - Vehicle / Wheel Analysis

This repository analyzes `input/libfs14.so` with Ghidra Headless.

## Run
1. Push this repository to GitHub (include `.github`, `scripts`, and `input`).
2. Open **Actions** → **Ghidra Vehicle Decompile** → **Run workflow**.
3. Download artifact **fs14-vehicle-analysis**.

## Output
- `vehicle_decompiled.c`: decompiled vehicle/wheel/attachment functions and one-hop callers/callees.
- `vehicle_disassembly.txt`: instruction listing for those functions.
- `vehicle_callgraph.txt`: calls between selected functions.
- `vehicle_data_refs.txt`: data references from relevant code.
- `vehicle_strings.txt`: model/vehicle-related strings and referencing functions.
- `all_function_names.txt`: full function list.
- `headless_log.txt`: Ghidra analysis log.

The library in `input/` is the exact `libfs14.so` supplied for this analysis.
