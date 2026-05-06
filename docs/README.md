# Documentation

Start with the top-level [README](../README.md) if you want the short
demo path. These pages go deeper:

| Page | Use it for |
|------|------------|
| [building.md](building.md) | Host dependencies, simulator generation, firmware build details |
| [simulation.md](simulation.md) | How the fast `litex_sim` harness works |
| [hardware.md](hardware.md) | Digilent Arty A7 bring-up, SDCard boot, serial boot |
| [porting.md](porting.md) | mquickjs integration internals and adding hardware bindings |

Recommended first run:

```sh
make check-env
make sim SCRIPT=examples/hello.js
```

Recommended hardware demo on Digilent Arty A7:

```sh
make arty-gateware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_EXTRA="--with-sdcard --with-ethernet"
make firmware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd SCRIPT=examples/sdcard_loader.js
make arty-sdcard-prepare ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_SDCARD=/media/$USER/LITEX
make arty-load ARTY_BUILD_DIR=/tmp/arty_mqjs_sd
```
