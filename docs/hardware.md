# Running on hardware

The simulator is the primary development target and the one the CI
exercises. The same firmware also builds for physical boards — this
page documents the path using the Digilent Arty A7 as the reference.
It has been prepared but not yet bring-up-tested.

## Build a LiteX SoC for your board

Pick any board supported by
[litex-boards](https://github.com/litex-hub/litex-boards) and generate
an SoC with the same options we use in simulation (libc-mode=full,
timer-uptime, VexRiscv CPU):

```sh
python3 -m litex_boards.targets.digilent_arty \
    --build \
    --cpu-type=vexriscv \
    --libc-mode=full \
    --timer-uptime \
    --integrated-main-ram-size=0x01000000 \
    --output-dir=/tmp/arty_mqjs
```

For targets with external DRAM you can skip the integrated main RAM and
let the build system wire up the DRAM controller instead.

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

Flash the gateware, then upload the firmware over the serial
bootloader:

```sh
litex_term /dev/ttyUSBn --kernel=firmware/firmware.bin
```

You should see the LiteX BIOS banner, the transfer, then:

```
Executing booted program at 0x40000000
--============= Liftoff! ===============--

--========= litex_mquickjs =========--
mquickjs heap:   1048576 bytes
CPU:             VexRiscv @ 100000000 Hz
running embedded script...
hello from mquickjs on litex!
[mqjs] done
```

## LEDs / switches binding

`examples/leds.js` uses `litex.setLeds(n)` and `litex.getSwitches()`
which compile to direct CSR accesses. If your board exposes those CSRs
(they're standard on the Arty A7 target), the script will drive the
LEDs visibly. If the CSRs aren't present, the bindings degrade
gracefully — the firmware still links, calls become no-ops.

## Custom hardware bindings

`firmware/mqjs_port.c` and `firmware/mqjs_stdlib_litex.c` are the two
files to edit when adding your own peripherals. The pattern is:

1. Add a `JS_CFUNC_DEF("yourFunc", n, js_litex_your_func)` line under
   the `js_litex[]` table in `mqjs_stdlib_litex.c`.
2. Implement `JSValue js_litex_your_func(JSContext*, JSValue*, int, JSValue*)`
   in `mqjs_port.c`, using the CSR accessors from `generated/csr.h`.
3. Rebuild — no linker-script changes needed, the stdlib generator
   picks up the new entry automatically.
