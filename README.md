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
enough `main_ram`, and has been validated on Digilent Arty A7 and
LambdaConcept ECPIX-5 hardware.

## [> Simulation

```sh
git clone --recursive https://github.com/enjoy-digital/mquickjs-on-litex
cd mquickjs-on-litex

./make.py sim
./make.py sim examples/demo.js
./make.py sim-video
./make.py sim-video examples/plasma.js
./make.py sim-video examples/fire.js
./make.py sim-video examples/tunnel.js
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
The SDCard flow is the easiest standalone demo: LiteX BIOS loads
`boot.bin`, then mquickjs loads `main.js` from the card.

```sh
./make.py board-build --target litex_boards.targets.digilent_arty --build-dir build/arty-sd -- --with-sdcard
./make.py sdcard --build-dir build/arty-sd --mount /media/$USER/LITEX
./make.py board-load --target litex_boards.targets.digilent_arty --build-dir build/arty-sd
```

Edit `main.js` on the SDCard, reset the board, and the FPGA runs the
new JavaScript.

For video-capable boards, enable the target framebuffer and run the
showcase playlist. It runs plasma, fire and tunnel from one firmware:

```sh
./make.py board-build --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video -- --with-video-framebuffer --uart-baudrate=1000000 --uart-fifo-depth=512
./make.py firmware examples/showcase.js --build-dir build/ecpix5-video
./make.py board-load --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video
./make.py board-run --serial /dev/ttyUSB2 --baudrate 1000000
```

The live-editing step serves the browser UI directly from the board:

```sh
./make.py board-build --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-live -- --with-ethernet --with-video-framebuffer --uart-baudrate=1000000 --uart-fifo-depth=512
./make.py live --build-dir build/ecpix5-live
./make.py board-load --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-live
./make.py board-run --serial /dev/ttyUSB2 --baudrate 1000000
```

Then open `http://192.168.1.50/`, edit JavaScript in the browser, and
press `Run`. For debugging, the older host bridge is still available
with `./make.py live --live-mode udp --build-dir build/ecpix5-live` and
`./tools/live_bridge.py --board 192.168.1.50`.

## [> Files

```
make.py    build/run demos
examples/  JavaScript demos
firmware/  mquickjs LiteX firmware
docs/      details
```

Useful docs:

- [docs/simulation.md](docs/simulation.md): dependencies and sim flow.
- [docs/hardware.md](docs/hardware.md): board and SDCard flow.
- [docs/porting.md](docs/porting.md): firmware integration notes.
- [docs/demoscene.md](docs/demoscene.md): framebuffer demo plan.

## [> License

BSD-2-Clause for this repository. The mquickjs and lwIP submodules keep
their upstream licenses.
