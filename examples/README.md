<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Examples

These scripts are embedded as raw JavaScript bytes. Parsing, garbage
collection, and execution all happen on the LiteX CPU.

| Script | Best first use | What to look for |
|--------|----------------|------------------|
| `hello.js` | Sanity check | UART prints and `[mqjs] done` |
| `board_showcase.js` | Board demo for people watching the LEDs | Visible LED patterns and timing |
| `sdcard_loader.js` | SDCard auto-boot firmware script | Auto-runs `main.js` from FAT |
| `sdcard/main.js` | Script to place on the SDCard root | Identifier print, scratch test, LED scanner |

Run in simulation:

```sh
make sim SCRIPT=examples/hello.js
```

Run on hardware after building/loading the board SoC:

```sh
make firmware SCRIPT=examples/board_showcase.js
make board-run BOARD_SERIAL=/dev/ttyUSB2
```

Run the SDCard auto-boot demo:

```sh
make board-gateware BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_EXTRA="--with-sdcard --with-ethernet"
make firmware BOARD_BUILD_DIR=/tmp/mqjs_sd SCRIPT=examples/sdcard_loader.js
make board-sdcard-prepare BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_SDCARD=/media/$USER/LITEX
make board-load BOARD_BUILD_DIR=/tmp/mqjs_sd
```

Use `make board-sdcard-clean-prepare ...` when reusing a card that has
old LiteX boot files on it.
