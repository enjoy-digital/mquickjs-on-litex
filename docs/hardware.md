# Running on hardware

The simulator is the primary development target and the one the CI
exercises. The same firmware also builds for physical boards — this
page documents the path using the Digilent Arty A7 as the reference.
This flow has been bring-up-tested on a Digilent Arty A7-35T with the
on-board FT2232 JTAG/UART interface.

## Build a LiteX SoC for your board

Pick any board supported by
[litex-boards](https://github.com/litex-hub/litex-boards) and generate
an SoC with the same options we use in simulation (libc-mode=full,
timer-uptime, VexRiscv CPU):

```sh
./targets/arty_mquickjs.py \
    --build \
    --cpu-type=vexriscv \
    --libc-mode=full \
    --timer-uptime \
    --output-dir=/tmp/arty_mqjs
```

On Arty, do not request a large integrated main RAM: the JavaScript
heap wants more memory than the Artix-7 BRAM budget can provide. The
command above uses the board DDR as `main_ram`, which gives the BIOS a
256 MiB RAM region at `0x40000000`. The repository target also exposes
the Arty LEDs, switches, and user buttons as LiteX CSRs so JavaScript
can access them directly.

## Build the firmware against that SoC

Point the firmware's `BUILD_DIRECTORY` at the board build dir:

```sh
make -C firmware \
     BUILD_DIRECTORY=/tmp/arty_mqjs \
     SCRIPT=$(pwd)/examples/hello.js
```

Same invocation as for simulation — the Makefile reads
`variables.mak` from whatever directory you point it at.

## Load and run

Load the gateware into SRAM. The LiteX target's `--load` path uses
OpenOCD; if your OpenOCD has FTDI support, this is enough:

```sh
./targets/arty_mquickjs.py \
    --load \
    --cpu-type=vexriscv \
    --libc-mode=full \
    --timer-uptime \
    --output-dir=/tmp/arty_mqjs
```

If OpenOCD reports `invalid command name "ftdi"`, use
`openFPGALoader` instead:

```sh
openFPGALoader -c digilent /tmp/arty_mqjs/gateware/digilent_arty.bit
```

Then upload the firmware over the serial bootloader. On the Arty
FT2232, interface 1 is normally the UART, so this is commonly
`/dev/ttyUSB2`; prefer the stable `/dev/serial/by-id/...if01...` path
when available:

```sh
litex_term /dev/ttyUSBn --kernel=firmware/firmware.bin
```

You should see the LiteX BIOS banner, the transfer, then:

```
Executing booted program at 0x40000000
--============= Liftoff! ===============--

--========= mquickjs on LiteX =========--
mquickjs heap:   1048576 bytes
CPU:             VexRiscv @ 100000000 Hz
running embedded script...
hello from mquickjs on LiteX!
[mqjs] done
```

## LED / switch demos

`examples/leds.js` uses `litex.setLeds(n)` and `litex.getSwitches()`
which compile to direct CSR accesses. The repository's Arty target
enables `leds`, `switches`, and `buttons` CSRs by default. If the CSRs
aren't present on another SoC, the bindings degrade gracefully — the
firmware still links, calls become no-ops.

For a quick smoke test, build and run `examples/leds.js` the same way:

```sh
make -C firmware \
     BUILD_DIRECTORY=/tmp/arty_mqjs \
     SCRIPT=$(pwd)/examples/leds.js

litex_term /dev/ttyUSBn --kernel=firmware/firmware.bin
```

On the tested Arty A7-35T run, the script completed with:

```
counter took 17 ms
shift took 17 ms
switches = 0
[mqjs] done
```

For a more visible demo, use `examples/arty_showcase.js`:

```sh
make firmware SCRIPT=examples/arty_showcase.js
make arty-run ARTY_SERIAL=/dev/ttyUSB2
```

It runs a binary counter, a scanner pattern, a short switch-mirror
window, and a heartbeat pattern, all from JavaScript running on the
VexRiscv CPU.

## SDCard live-reload demo

This is the more interactive Arty demo:

1. Format an SDCard as FAT.
2. Copy `examples/sdcard/main.js` to the card root as `main.js`.
3. Insert the card in the Arty SDCard PMOD.
4. Build and run the SDCard loader:

```sh
make arty-sdcard-demo ARTY_BUILD_DIR=/tmp/arty_mqjs_sd
```

The loader firmware embeds `examples/sdcard_button_loader.js`. It sits
in a JavaScript loop polling `litex.getButtons()`. Press BTN0 and the
VexRiscv-side JS engine reads `main.js` from FAT using
`litex.load("main.js")`, evaluates it, and returns to the wait loop.

LED status:

| LEDs | Meaning |
|------|---------|
| `0x1` | Waiting for BTN0 |
| `0x2` | Loading/evaluating `main.js` |
| `0x4` | Script completed |
| `0x8` | Load/eval failed; see UART |

Edit `main.js` on the SDCard, reinsert the card, and press BTN0 again
to run the new script without rebuilding gateware or firmware.

The SDCard profile enables Ethernet too:

```sh
make arty-gateware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_EXTRA="--with-sdcard --with-ethernet"
make firmware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd SCRIPT=examples/sdcard_button_loader.js
```

To test the firmware upload manually with `litex_term`, split the
steps:

```sh
make arty-gateware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_EXTRA="--with-sdcard --with-ethernet"
make arty-load ARTY_BUILD_DIR=/tmp/arty_mqjs_sd
make firmware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd SCRIPT=examples/sdcard_button_loader.js
litex_term /dev/ttyUSB2 --kernel=firmware/firmware.bin
```

If the UART number moved after reloading the bitstream, check:

```sh
ls -l /dev/ttyUSB*
```

On Arty FT2232, the LiteX UART is usually the second FT2232 interface;
on the tested setup it was `/dev/ttyUSB2`. `litex_term` should show the
BIOS serial boot request, upload `firmware.bin`, boot it, and then the
mquickjs loader should print:

```text
[sd] waiting for BTN0 to load main.js
[sd] edit main.js on the SDCard, reinsert it, then press BTN0
```

Press BTN0. With `examples/sdcard/main.js` copied to the card root as
`main.js`, the expected output is:

```text
[sd] loading main.js run 1
[main.js] hello from SDCard
[main.js] switches = 0
[sd] done
```

This keeps the LiteX BIOS SDCard boot helpers linkable with the LiteX
revision used for bring-up, while mquickjs itself uses only the SDCard,
button, LED, timer, and UART CSRs for this demo.

## Custom hardware bindings

`firmware/mqjs_port.c` and `firmware/mqjs_stdlib_litex.c` are the two
files to edit when adding your own peripherals. The pattern is:

1. Add a `JS_CFUNC_DEF("yourFunc", n, js_litex_your_func)` line under
   the `js_litex[]` table in `mqjs_stdlib_litex.c`.
2. Implement `JSValue js_litex_your_func(JSContext*, JSValue*, int, JSValue*)`
   in `mqjs_port.c`, using the CSR accessors from `generated/csr.h`.
3. Rebuild — no linker-script changes needed, the stdlib generator
   picks up the new entry automatically.
