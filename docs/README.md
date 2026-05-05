# Documentation

Start with the top-level [README](../README.md) if you want the short
demo path. These pages go deeper:

| Page | Use it for |
|------|------------|
| [building.md](building.md) | Host dependencies, simulator generation, firmware build details |
| [simulation.md](simulation.md) | How the fast `litex_sim` harness works |
| [hardware.md](hardware.md) | Digilent Arty A7 bring-up, bitstream loading, serial boot |
| [porting.md](porting.md) | mquickjs integration internals and adding hardware bindings |

Recommended first run:

```sh
make check-env
make sim SCRIPT=examples/hello.js
```

Recommended hardware demo on Digilent Arty A7:

```sh
make arty-gateware
make firmware SCRIPT=examples/leds.js
make arty-load
make arty-run ARTY_SERIAL=/dev/ttyUSB2
```
