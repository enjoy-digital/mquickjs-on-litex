<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Documentation

Start with the top-level [README](../README.md) if you want the short
demo path. These pages go deeper:

| Page | Use it for |
|------|------------|
| [building.md](building.md) | Host dependencies, simulator generation, firmware build details |
| [simulation.md](simulation.md) | How the fast `litex_sim` harness works |
| [hardware.md](hardware.md) | Hardware bring-up, LiteX-Boards targets, SDCard boot, serial boot |
| [porting.md](porting.md) | mquickjs integration internals and adding hardware bindings |

Recommended first run:

```sh
make check-env
make sim SCRIPT=examples/hello.js
```

Recommended hardware demo on the validated Digilent Arty A7:

```sh
make board-gateware BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_EXTRA="--with-sdcard --with-ethernet"
make firmware BOARD_BUILD_DIR=/tmp/mqjs_sd SCRIPT=examples/sdcard_loader.js
make board-sdcard-prepare BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_SDCARD=/media/$USER/LITEX
make board-load BOARD_BUILD_DIR=/tmp/mqjs_sd
```
