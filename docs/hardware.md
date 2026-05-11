<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Running on Hardware

The simulator is the CI path. Hardware uses the same firmware, built
against an upstream LiteX-Boards target.

The flow is intentionally generic: pick an upstream LiteX-Boards target,
build it with enough `main_ram`, then choose SDCard boot or serial boot.
The same firmware was validated on Digilent Arty A7 and LambdaConcept
ECPIX-5 hardware.

## Options

| Option | Meaning |
|--------|---------|
| `--target` | LiteX-Boards target module, for example `litex_boards.targets.digilent_arty` |
| `--build-dir` | Board build directory used by LiteX and the firmware |
| `--serial` | Serial device used by `litex_term` |
| `--baudrate` | Serial baudrate used by `board-run` |
| `--mount` | Mounted FAT SDCard root |
| `-- <args>` | Extra arguments passed to the board target |

## Serial Boot

Serial boot is useful while iterating on a script without touching the
SDCard:

```sh
./make.py board-build --target litex_boards.targets.<target_module> --build-dir build/<board> -- <target-specific options>
./make.py firmware examples/demo.js --build-dir build/<board>
./make.py board-load --target litex_boards.targets.<target_module> --build-dir build/<board>
./make.py board-run --serial /dev/ttyUSBn
```

`board-load` delegates to the board target `--load` option, so the
target keeps ownership of the right cable/programmer details.

Start with `examples/hello.js`, then try `examples/demo.js`. LED writes
are no-ops when no LED CSR is present; switch/button reads return zero
when those CSRs are absent.

For a high-speed UART, build the SoC UART and run `litex_term` at the
same speed:

```sh
./make.py board-build --target litex_boards.targets.<target_module> --build-dir build/<board> -- --uart-baudrate=1000000 --uart-fifo-depth=512 <target-specific options>
./make.py board-run --serial /dev/ttyUSBn --baudrate 1000000
```

## ECPIX-5

The LambdaConcept ECPIX-5 uses the upstream
`litex_boards.targets.lambdaconcept_ecpix5` target. A basic LED demo:

```sh
./make.py board-build --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5 -- --uart-baudrate=1000000 --uart-fifo-depth=512
./make.py firmware examples/demo.js --build-dir build/ecpix5
./make.py board-load --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5
./make.py board-run --serial /dev/ttyUSB2 --baudrate 1000000
```

The validated setup used the FT2232 UART on `/dev/ttyUSB2`.

For HDMI output, enable the target framebuffer and use the showcase
playlist. This is the default hardware video demo and runs plasma, fire
and tunnel from one firmware:

```sh
./make.py board-build --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video -- --with-video-framebuffer --uart-baudrate=1000000 --uart-fifo-depth=512
./make.py firmware examples/showcase.js --build-dir build/ecpix5-video
./make.py board-load --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video
./make.py board-run --serial /dev/ttyUSB2 --baudrate 1000000
```

Expected output starts with:

```text
[showcase] framebuffer = 640 x 480 x 32
[showcase] clock = <target clock> Hz
[showcase] tile = 48 x 36 x 8
[showcase] plasma frames = 24
[showcase] fire frames = 24
[showcase] tunnel frames = 24
[showcase] done
[mqjs] done
```

## SDCard Boot

For a standalone demo, let LiteX BIOS boot `boot.bin` from the card,
then let mquickjs load `main.js` from the same card:

```sh
./make.py board-build --target litex_boards.targets.digilent_arty --build-dir build/arty-sd -- --with-sdcard
./make.py sdcard --build-dir build/arty-sd --mount /media/$USER/LITEX
./make.py board-load --target litex_boards.targets.digilent_arty --build-dir build/arty-sd
```

`sdcard` builds the SDCard loader firmware, removes `boot.json` when it
exists since LiteX BIOS tries it before `boot.bin`, then copies:

| SDCard file | Source |
|-------------|--------|
| `boot.bin` | `firmware/firmware.bin` |
| `main.js` | `examples/sdcard/main.js` |

Reset the board with no serial loader answering. LiteX BIOS copies
`boot.bin` to `main_ram`, runs it, and the firmware evaluates `main.js`.
Edit `main.js` on the card, reset again, and the new JavaScript runs.

Expected mquickjs output:

```text
[sd] auto-loading main.js from SDCard
[sd] edit main.js on the SDCard, then reset the board to run it again
[sd] loading main.js
[main.js] hello from SDCard
[main.js] scratch test = 0x51c0ffee OK
[main.js] LED scanner
[sd] done
```

## Other Boards

Start with the smallest script:

```sh
./make.py board-build --target litex_boards.targets.<target_module> --build-dir build/<board> -- <target-specific options>
./make.py firmware examples/hello.js --build-dir build/<board>
./make.py board-load --target litex_boards.targets.<target_module> --build-dir build/<board>
./make.py board-run --serial /dev/ttyUSBn
```

Then try `examples/demo.js`. LED writes are no-ops when no LED CSR is
present; switch/button reads return zero when those CSRs are absent. If
the target has video, pass its framebuffer option after `--` and run
`./make.py sim-video` in simulation. For hardware, start with
`examples/showcase.js`; it runs all visual demos by default. The
individual `examples/plasma.js`, `examples/fire.js`, `examples/tunnel.js`
and `examples/plasma_animated.js` scripts are useful when tuning one
effect.

`board-build` defaults to VexRiscv, but can forward another LiteX CPU
cleanly:

```sh
./make.py board-build --target litex_boards.targets.<target_module> --build-dir build/<board> --cpu-type vexiiriscv --cpu-variant standard -- <target-specific options>
```

CPU-specific options still go after `--`. For example, this builds a
FPU-capable VexiiRiscv experiment:

```sh
./make.py board-build --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video-vexii-fpu --cpu-type vexiiriscv --cpu-variant standard -- --with-video-framebuffer --sys-clk-freq=50000000 --uart-baudrate=1000000 --uart-fifo-depth=512 --with-isa f d --update-repo=no
```

On the ECPIX-5 test setup, the 75MHz VexiiRiscv F/D build routed but
missed timing, while a 50MHz build passed timing. The firmware reached
the JavaScript runtime but stalled before the animated plasma script
started printing, so the validated demo path remains VexRiscv for now.

For custom peripherals, add C bindings in `firmware/mqjs_port.c` and
register the JavaScript entry points in `firmware/mqjs_stdlib_litex.c`.
See [porting.md](porting.md) for the compact integration notes.
