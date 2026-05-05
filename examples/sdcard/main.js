// Copy this file to the root of a FAT-formatted SDCard as main.js.
// The SDCard loader auto-runs it at boot and BTN0 reloads it live.

function hex32(value) {
    var s = (value >>> 0).toString(16);
    while (s.length < 8) {
        s = "0" + s;
    }
    return "0x" + s;
}

function scanner(rounds) {
    for (var r = 0; r < rounds; r++) {
        for (var i = 0; i < 4; i++) {
            litex.setLeds(1 << i);
            litex.delay(70);
        }
        for (var j = 2; j > 0; j--) {
            litex.setLeds(1 << j);
            litex.delay(70);
        }
    }
}

function mirrorInputs(ms) {
    var end = litex.millis() + ms;
    while (litex.millis() < end) {
        litex.setLeds((litex.getSwitches() | (litex.getButtons() << 4)) & 0xf);
        litex.delay(20);
    }
}

var original = litex.getScratch();
var magic = 0x51c0ffee;

console.log("[main.js] hello from SDCard");
console.log("[main.js] identifier =", litex.getIdentifier());
console.log("[main.js] scratch before =", hex32(original));

litex.setScratch(magic);
var readback = litex.getScratch();
console.log("[main.js] scratch test =", hex32(readback), readback === magic ? "OK" : "FAIL");

console.log("[main.js] switches =", litex.getSwitches(), "buttons =", litex.getButtons());
console.log("[main.js] LED scanner, then live switch/button mirror");

scanner(3);
mirrorInputs(2500);

litex.setLeds(litex.getSwitches() & 0xf);
litex.setScratch(original);
console.log("[main.js] restored scratch =", hex32(litex.getScratch()));
