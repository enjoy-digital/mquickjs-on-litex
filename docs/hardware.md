<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Running on hardware

The simulator is the primary development target and the one CI
exercises. The same firmware also builds for physical LiteX boards.
This page documents a generic LiteX-Boards flow and uses the Digilent
Arty A7 as the validated reference board.

The tested setup is a Digilent Arty A7-35T with the on-board FT2232
JTAG/UART interface.

## Board variables

The top-level Makefile defaults to the Arty A7 target but can point at
another LiteX-Boards target:

```sh
make board-gateware \
    BOARD_TARGET=litex_boards.targets.digilent_arty \
    BOARD_BUILD_DIR=/tmp/mqjs_board \
    BOARD_EXTRA="--with-sdcard --with-ethernet"
```

Useful variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `BOARD_TARGET` | `litex_boards.targets.digilent_arty` | Python module for the LiteX-Boards target |
| `BOARD_BUILD_DIR` | `/tmp/mquickjs_on_litex` | LiteX build output directory |
| `BOARD_BITSTREAM` | `$(BOARD_BUILD_DIR)/gateware/digilent_arty.bit` | Bitstream loaded by `make board-load` |
| `BOARD_SERIAL` | `/dev/ttyUSB2` | Serial port used by `litex_term` |
| `BOARD_CABLE` | `digilent` | `openFPGALoader -c` cable name |
| `BOARD_EXTRA` | empty | Extra target arguments |
| `BOARD_SDCARD` | `/media/$USER/LITEX` | Mounted FAT SDCard root |

For a non-Arty board, set at least `BOARD_TARGET`,
`BOARD_BITSTREAM`, `BOARD_SERIAL`, and usually `BOARD_EXTRA`.
The legacy `arty-*` Makefile targets remain as aliases for the default
Arty setup, but new docs use the board-generic names.

## Build the LiteX SoC

Use the upstream LiteX-Boards target directly:

```sh
python3 -m $BOARD_TARGET \
    --build \
    --cpu-type=vexriscv \
    --libc-mode=full \
    --timer-uptime \
    $BOARD_EXTRA \
    --output-dir=/tmp/mqjs_board
```

The board needs enough `main_ram` for the firmware, stack and JavaScript
heap. On Arty, leave `integrated-main-ram-size` unset so the target uses
the board DDR as LiteX `main_ram`.

The top-level Makefile wraps this:

```sh
make board-gateware
```

## Build and run firmware

Point the firmware build at the LiteX output directory:

```sh
make firmware BOARD_BUILD_DIR=/tmp/mqjs_board SCRIPT=examples/board_showcase.js
```

Load the bitstream:

```sh
make board-load BOARD_BUILD_DIR=/tmp/mqjs_board
```

Then upload the firmware over the LiteX serial bootloader. On the Arty
FT2232, interface 1 is normally the UART; on the tested setup it was
`/dev/ttyUSB2`.

```sh
make board-run BOARD_SERIAL=/dev/ttyUSB2
```

Equivalent manual command:

```sh
litex_term /dev/ttyUSB2 --kernel=firmware/firmware.bin
```

You should see the LiteX BIOS banner, the firmware transfer, then:

```text
Executing booted program at 0x40000000
--============= Liftoff! ===============--

--========= mquickjs on LiteX =========--
mquickjs heap:   1048576 bytes
CPU:             VexRiscv @ 100000000 Hz
running embedded script...
[demo] binary counter
...
[demo] done
[mqjs] done
```

## SDCard boot demo

This is the most convenient standalone demo:

1. Build a SDCard-capable LiteX SoC.
2. Build the loader firmware.
3. Copy `firmware.bin` to the FAT SDCard root as `boot.bin`.
4. Copy `examples/sdcard/main.js` to the FAT SDCard root as `main.js`.
5. Load the bitstream and reset the board.

The Makefile can do the build/copy steps:

```sh
make board-gateware BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_EXTRA="--with-sdcard --with-ethernet"
make firmware BOARD_BUILD_DIR=/tmp/mqjs_sd SCRIPT=examples/sdcard_loader.js
make board-sdcard-prepare BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_SDCARD=/media/$USER/LITEX
make board-load BOARD_BUILD_DIR=/tmp/mqjs_sd
```

If the card was previously used for Linux-on-LiteX or another demo, use
the clean prepare target. It removes known stale root files such as
`boot.json`, Linux images, OpenSBI, rootfs archives, and DTBs before
copying the two mquickjs demo files:

```sh
make board-sdcard-clean-prepare BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_SDCARD=/media/$USER/LITEX
```

To inspect an already prepared card:

```sh
make board-sdcard-check BOARD_SDCARD=/media/$USER/LITEX
```

On reset, LiteX BIOS tries serial boot first, then SDCard boot. With no
host serial loader answering, it reads `boot.bin`, copies it to
`main_ram`, and jumps to it. The firmware then mounts the same card and
runs `main.js`.

Edit `main.js` on the SDCard and reset the board to run the new version.
No gateware rebuild or firmware upload is needed.

Expected mquickjs output:

```text
[sd] auto-loading main.js from SDCard
[sd] edit main.js on the SDCard, then reset the board to run it again
[sd] loading main.js
[main.js] hello from SDCard
[main.js] identifier = LiteX SoC on Arty A7 2026-05-05 15:06:23
[main.js] scratch before = 0x12345678
[main.js] scratch test = 0x51c0ffee OK
[main.js] LED scanner
[main.js] restored scratch = 0x12345678
[sd] done
```

## Testing another LiteX board target

Pick a target from `litex_boards.targets`, build it with the same core
options, then build firmware against that output directory. For example:

```sh
make board-gateware \
    BOARD_TARGET=litex_boards.targets.<target_module> \
    BOARD_BUILD_DIR=/tmp/mqjs_other \
    BOARD_BITSTREAM=/tmp/mqjs_other/gateware/<bitstream>.bit \
    BOARD_EXTRA="<target-specific options>"

make firmware BOARD_BUILD_DIR=/tmp/mqjs_other SCRIPT=examples/hello.js
make board-load BOARD_BUILD_DIR=/tmp/mqjs_other BOARD_BITSTREAM=/tmp/mqjs_other/gateware/<bitstream>.bit
make board-run BOARD_SERIAL=/dev/ttyUSBn
```

Start with `examples/hello.js`. Then try `examples/board_showcase.js`
if the SoC exposes LED CSRs. The JavaScript bindings degrade gracefully:
LED writes become no-ops if no LED CSR exists, and switch/button reads
return zero if those CSRs are absent.

For custom peripherals, add bindings in `firmware/mqjs_port.c` and
declare them in `firmware/mqjs_stdlib_litex.c`. See
[porting.md](porting.md) for the integration details.
