<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Quickstart

The shortest path to the full demo is a LiteX board with Ethernet and a
video framebuffer. The board serves the editor, receives JavaScript from
the browser and runs it inside mquickjs on the LiteX CPU.

## ECPIX-5 Live Demo

Build the SoC:

```sh
./make.py board-build \
    --target litex_boards.targets.lambdaconcept_ecpix5 \
    --build-dir build/ecpix5-live \
    -- \
    --with-ethernet \
    --with-video-framebuffer \
    --uart-baudrate=1000000 \
    --uart-fifo-depth=512
```

Build the live firmware:

```sh
./make.py live --build-dir build/ecpix5-live
```

Load the bitstream and upload the firmware:

```sh
SERIAL=/dev/ttyUSBn

./make.py board-load \
    --target litex_boards.targets.lambdaconcept_ecpix5 \
    --build-dir build/ecpix5-live

./make.py board-run --serial $SERIAL --baudrate 1000000
```

At the LiteX BIOS prompt, run:

```text
serialboot
```

Then open:

```text
http://192.168.1.50/
```

Click `Logo`, `Plasma` or `Stars` in the quick show section, or edit
`script.js` and press `Run`.

## Other LiteX Boards

Use the upstream LiteX-Boards target directly. The project does not need
a custom target when the board already exposes the features you want:

```sh
./make.py board-build \
    --target litex_boards.targets.<target_module> \
    --build-dir build/<board>-live \
    -- \
    --with-ethernet \
    --with-video-framebuffer \
    <target-specific-options>

./make.py live --build-dir build/<board>-live

./make.py board-load \
    --target litex_boards.targets.<target_module> \
    --build-dir build/<board>-live

./make.py board-run --serial <serial-port> --baudrate <baudrate>
```

If the target has no video framebuffer, the live editor still works for
LEDs, CSRs and console output. If the target has no Ethernet, use the
normal embedded-script or SDCard flows from [hardware.md](hardware.md).

## Browser Over SSH

When the board is connected to a remote development machine, forward the
board web page through SSH:

```sh
ssh -L 8050:192.168.1.50:80 user@dev-machine
```

Then open `http://localhost:8050/` on your laptop.
