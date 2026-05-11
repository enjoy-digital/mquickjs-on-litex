// Digilent Arty A7 showcase: JavaScript visibly drives board LEDs,
// samples switches, and reports timings over the LiteX UART.

var LEDS = 4;
var MASK = (1 << LEDS) - 1;

function show(name, fn) {
    console.log("[demo]", name);
    var t0 = performance.now();
    fn();
    var t1 = performance.now();
    console.log("[demo]", name, "took", (t1 - t0) | 0, "ms");
}

function leds(value) {
    litex.setLeds(value & MASK);
}

function pause(ms) {
    litex.delay(ms);
}

show("binary counter", function () {
    for (var i = 0; i < 16; i++) {
        leds(i);
        pause(80);
    }
});

show("knight rider", function () {
    for (var round = 0; round < 4; round++) {
        for (var i = 0; i < LEDS; i++) {
            leds(1 << i);
            pause(80);
        }
        for (var i = LEDS - 2; i > 0; i--) {
            leds(1 << i);
            pause(80);
        }
    }
});

show("switch mirror", function () {
    for (var i = 0; i < 60; i++) {
        leds(litex.getSwitches());
        pause(50);
    }
});

show("heartbeat", function () {
    for (var i = 0; i < 6; i++) {
        leds(MASK);
        pause(70);
        leds(0);
        pause(140);
    }
});

leds(0);
console.log("[demo] final switches =", litex.getSwitches());
