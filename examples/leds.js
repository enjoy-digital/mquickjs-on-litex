// Toggle the on-board LEDs via the `litex` hardware binding.
// Runs a handful of patterns, reports timing, then exits.
//
// In simulation the LED CSR writes are silently dropped unless
// --with-gpio is passed to litex_sim. On real hardware the pattern
// is visible.
var N = 8;

function pattern(name, fn) {
    var t0 = performance.now();
    fn();
    var t1 = performance.now();
    console.log(name, "took", (t1 - t0) | 0, "ms");
}

pattern("counter", function () {
    for (var i = 0; i < 16; i++) {
        litex.setLeds(i & ((1 << N) - 1));
        litex.delay(1);
    }
});

pattern("shift", function () {
    for (var i = 0; i < N; i++) { litex.setLeds(1 << i); litex.delay(1); }
    for (var i = N - 1; i >= 0; i--) { litex.setLeds(1 << i); litex.delay(1); }
});

litex.setLeds(0);
console.log("switches =", litex.getSwitches());
