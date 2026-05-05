# Examples

These scripts are embedded as raw JavaScript bytes by default. Parsing,
bytecode generation, garbage collection, and execution all happen on
the LiteX CPU.

| Script | Best first use | What to look for |
|--------|----------------|------------------|
| `hello.js` | Sanity check | UART prints and `[mqjs] done` |
| `leds.js` | Fast hardware CSR test | Short LED writes plus switch read |
| `arty_showcase.js` | Arty demo for people watching the board | Visible LED patterns, switch sampling, timing |
| `sdcard_button_loader.js` | Arty SDCard auto-boot/live-reload firmware script | Auto-runs `main.js` from FAT, then BTN0 reloads it |
| `sdcard/main.js` | Script to place on the SDCard root | Identifier print, scratch test, LED scanner, switch/button mirror |
| `fib.js` | CPU timing | Recursive JavaScript plus `performance.now()` |
| `json.js` | Language/library coverage | JSON, arrays, typed arrays |
| `mandelbrot.js` | Compute demo | ASCII art from soft-float-heavy JavaScript |
| `unicode.js` | Regression coverage | UTF-8 source handling |

Run in simulation:

```sh
make sim SCRIPT=examples/hello.js
```

Run on Digilent Arty A7 after building/loading the board SoC:

```sh
make firmware SCRIPT=examples/arty_showcase.js
make arty-run ARTY_SERIAL=/dev/ttyUSB2
```

Run the SDCard auto-boot/live-reload demo on Arty:

```sh
make arty-gateware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_EXTRA="--with-sdcard --with-ethernet"
make firmware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd SCRIPT=examples/sdcard_button_loader.js
make arty-sdcard-prepare ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_SDCARD=/media/$USER/LITEX
make arty-load ARTY_BUILD_DIR=/tmp/arty_mqjs_sd
```
