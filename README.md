Pastafarian: Efficient FSM Coverage
===================================
Pastafarian is a research project to automatically generate and verify FSM in SystemVerilog, especially generated
by high-level hardware generator frameworks such as Chisel and PyMTL. It does not require any RTL style and works
directly on synthesizable SystemVerilog. 

Features:
- Automatic FSM state and transition arc extraction, including counters, from any RTL.
- Tight integration with `JasperGold` to verify reachability of each FSM state and arcs.
- SVA bind file generation (WIP).

Supported RTL style/codegen:
- All synthesizable SystemVerilog (thanks to slang).
- Handwritten RTL, and no need to conform to any EDA style guide.
- Popular hardware generator frameworks:
  - Chisel
  - nMigen
  - PyMTL
  - Genesis2
  - ...


The algorithms should work on any arbitrary SystemVerilog, either behavioral or structural. As a result, it should
work with the hardware generator framework out of the box.

## Usage
Generally the command information can be obtained by invoking `detector -h`. Here are some example commands

1. Print out FSM states and transition arcs for a single SV file `input.sv`
   ```bash
   $ ./detector -i input.sv
   ```
2. Print out FSM states and transition arcs for multiple SV files
   ```bash
   $ ./detector -i input1.sv input2.sv
   ```
3. Specify top module as `TOP`
   ```bash
   $ ./detector -i input.sv --top TOP
   ```
4. Specify reset/clock pin name. Notice that tool uses some naming convention for clock and reset name detection.
   If the tool fails to detect the corresponding pins, you have to specify manually:
   ```bash
   $ ./detector -i input.sv --reset rst
   ```
   ```bash
   $ ./detector -i input.sv --clock clk
5. Specify SystemVerilog include search directories, e.g. `dir1` and `dir2`:
   ```bash
   $ ./detector -i input.sv -I dir1 dir2
   ```
6. Use `JasperGold` to verify the reachability for each state and arcs. Notice that you need to have `jaspergold`
  available in your shell. If a state or an arc is proven unreachable, the tool will indicate it in the printout.
   ```bash
   $ ./detector -i input.sv --formal
   ```
7. Control `JasperGold` proving options. Currently time limit, reset type, and double edge clock are supported.
   - reset type: `-R high` or `-R low`. If no reset or don't care, use `-R none`.
   - time limit per property: `-t 10`, which is set to 10 seconds.
   - double edge clock: `--double-edge-clock`
8. Use multi-threading. Set to `0` to use all available CPU cores. The example below shows how to use 4 CPU cores.
   ```bash
   $ ./detector -i input.sv -n 4
   ```
9. Check coupled FSM. This will print out any coupled FSM in the design. Can be used with `JasperGold` for
cross-validation.
   ```bash
   $ ./detector -i input.sv -c
   ```

## How to download/build
Currently only Linux with AVX is supported. To see if your CPU supports AVX, simply do `cat /prof/cpuinfo | grep avx`.
### Use pre-built binaries
Here is a one-line install script
```bash
curl -s https://raw.githubusercontent.com/Kuree/pastafarian/master/scripts/install.sh | sh
```
The script will download the `detector` binary and `slang` driver into the current directory, `sudo` not required.
### Build from source
This project requires C++17 compiler such as g++-8 and cmake.
```bash
mkdir build
cd build && cmake ..
make -j
```
After the build, the `detector` file will be in the `bin/` folder.

Notice that you also need to download the `slang` binary from [here](https://github.com/Kuree/binaries/raw/master/slang)
and make it executable. Then do `export SLANG=[path to slang]`.

## How it works
You can find a Latex-based documentation in `docs/` folder. To view the PDF, simply do `make` in the `docs` folder. This requires to have `pdflatex` and `dot2tex` installed.

## Why called Pastafarian
Pastafarian is a parody religion called [Flying Spaghetti Monster](https://en.wikipedia.org/wiki/Flying_Spaghetti_Monster).
Its acronym, FSM, is the same as Finite State Machine (FSM).