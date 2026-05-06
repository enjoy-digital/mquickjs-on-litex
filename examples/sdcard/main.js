// Copy this file to the root of a FAT-formatted SDCard as main.js.
// The SDCard loader auto-runs it at boot. Edit this file on the card,
// then reset the board to run the new version.

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

var original = litex.getScratch();
var magic = 0x51c0ffee;

console.log("[main.js] hello from SDCard");
console.log("[main.js] identifier =", litex.getIdentifier());
console.log("[main.js] scratch before =", hex32(original));

litex.setScratch(magic);
var readback = litex.getScratch();
console.log("[main.js] scratch test =", hex32(readback), readback === magic ? "OK" : "FAIL");

console.log("[main.js] LED scanner");

scanner(3);

litex.setLeds(0);
litex.setScratch(original);
console.log("[main.js] restored scratch =", hex32(litex.getScratch()));
