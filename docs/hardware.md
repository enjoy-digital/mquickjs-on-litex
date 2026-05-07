<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Running on Hardware

The simulator is the CI path. Hardware uses the same firmware, built
against an upstream LiteX-Boards target.

The examples below use a Digilent Arty A7, but the flow is intentionally
generic: pick a LiteX-Boards target, build it with enough `main_ram`,
then choose SDCard boot or serial boot.

## Options

| Option | Meaning |
|--------|---------|
| `--target` | LiteX-Boards target module, for example `litex_boards.targets.digilent_arty` |
| `--build-dir` | Board build directory used by LiteX and the firmware |
| `--serial` | Serial device used by `litex_term` |
| `--mount` | Mounted FAT SDCard root |
| `-- <args>` | Extra arguments passed to the board target |

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

## Serial Boot

Serial boot is useful while iterating on a script without touching the
SDCard:

```sh
./make.py board-build --target litex_boards.targets.digilent_arty --build-dir build/arty
./make.py firmware examples/demo.js --build-dir build/arty
./make.py board-load --target litex_boards.targets.digilent_arty --build-dir build/arty
./make.py board-run --serial /dev/ttyUSB2
```

`board-load` delegates to the board target `--load` option, so the
target keeps ownership of the right cable/programmer details.

On the Arty FT2232, interface 1 is normally the UART; on the validated
setup it appeared as `/dev/ttyUSB2`.

## Other Boards

Start with the smallest script:

```sh
./make.py board-build --target litex_boards.targets.<target_module> --build-dir build/<board> -- <target-specific options>
./make.py firmware examples/hello.js --build-dir build/<board>
./make.py board-load --target litex_boards.targets.<target_module> --build-dir build/<board>
./make.py board-run --serial /dev/ttyUSBn
```

Then try `examples/demo.js`. LED writes are no-ops when no LED CSR is
present; switch/button reads return zero when those CSRs are absent. The
same JavaScript can therefore run on small and large LiteX SoCs.

For custom peripherals, add C bindings in `firmware/mqjs_port.c` and
register the JavaScript entry points in `firmware/mqjs_stdlib_litex.c`.
See [porting.md](porting.md) for the compact integration notes.
