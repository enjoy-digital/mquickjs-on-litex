<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

```
              __  _______       _     __      ______              __   _ __      _  __
             /  |/  / __ \__ __(_)___/ /____ / / __/ ___  ___    / /  (_) /____ | |/_/
            / /|_/ / /_/ / // / / __/  '_/ // /\ \  / _ \/ _ \  / /__/ / __/ -_)>  <
           /_/  /_/\___\_\_,_/_/\__/_/\_\\___/___/  \___/_//_/ /____/_/\__/\__/_/|_|
                                      Copyright (c) 2026, EnjoyDigital
                                              Powered by LiteX
```

[![](https://github.com/enjoy-digital/litex_mquickjs/workflows/sim/badge.svg)](https://github.com/enjoy-digital/litex_mquickjs/actions)
![License](https://img.shields.io/badge/License-BSD%202--Clause-orange.svg)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/enjoy-digital/litex_mquickjs)

## [> Intro

mquickjs on LiteX runs Fabrice Bellard's
[mquickjs](https://github.com/bellard/mquickjs) JavaScript engine on a
bare-metal [LiteX](https://github.com/enjoy-digital/litex) SoC.

<p align="center">
  <img src="docs/mquickjs-on-litex-intro.png" alt="mquickjs on LiteX intro" width="620">
</p>

The JavaScript is not translated to C and is not running on the host.
LiteX boots a VexRiscv firmware, creates a mquickjs heap, parses the
script, runs the VM, and exposes LiteX peripherals through a small
`litex` object.

```text
JavaScript file -> firmware.bin -> VexRiscv -> mquickjs -> LiteX CSRs
```

It runs in `litex_sim`, is meant to be portable to LiteX boards with
enough `main_ram`, and has been validated on a Digilent Arty A7.

## [> Simulation

```sh
git clone --recursive https://github.com/enjoy-digital/mquickjs-on-litex
cd mquickjs-on-litex

./make.py sim
./make.py sim examples/demo.js
```

The first run builds the LiteX simulator. Later runs reuse it and only
rebuild the firmware/script.

Expected short output:

```text
--========= mquickjs on LiteX =========--
running embedded script...
hello from mquickjs on LiteX!
[mqjs] done
```

## [> Hardware

Use any upstream LiteX-Boards target that provides enough `main_ram`.
The Arty A7 commands below are just an example:

```sh
./make.py board-build --target litex_boards.targets.digilent_arty --build-dir build/arty
./make.py firmware examples/demo.js --build-dir build/arty
./make.py board-load --target litex_boards.targets.digilent_arty --build-dir build/arty
./make.py board-run --serial /dev/ttyUSB2
```

For the SDCard flow, LiteX BIOS loads `boot.bin`, then mquickjs loads
`main.js` from the card:

```sh
./make.py board-build --target litex_boards.targets.digilent_arty --build-dir build/arty-sd -- --with-sdcard
./make.py sdcard --build-dir build/arty-sd --mount /media/$USER/LITEX
./make.py board-load --target litex_boards.targets.digilent_arty --build-dir build/arty-sd
```

Edit `main.js` on the SDCard, reset the board, and the FPGA runs the
new JavaScript.

## [> Files

```
make.py       friendly build/run entry point
firmware/     mquickjs port and LiteX bindings
examples/     hello, demo, SDCard main.js
sim/          litex_sim runner
tools/        firmware build helpers
docs/         simulation, hardware and porting notes
test/         end-to-end simulation smoke tests
```

Useful docs:

- [docs/simulation.md](docs/simulation.md): dependencies and sim flow.
- [docs/hardware.md](docs/hardware.md): board and SDCard flow.
- [docs/porting.md](docs/porting.md): firmware integration notes.

## [> License

BSD-2-Clause for this repository. The mquickjs submodule keeps its
upstream MIT license.
