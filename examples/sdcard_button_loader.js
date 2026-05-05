// SDCard auto-run/live-reload demo for Digilent Arty.
//
// Build this script into the firmware, put a FAT-formatted SDCard in
// the board with /main.js on it, and the board will load it at boot.
// Press BTN0 to reload it after editing the file on the host.

var path = "main.js";
var last = 0;
var runs = 0;
var status = 1;
var nextPulse = 0;
var pulse = 0;

function runMain(reason) {
    runs++;
    console.log("[sd] loading", path, "run", runs, "(" + reason + ")");
    status = 2;
    litex.setLeds(2);
    try {
        litex.load(path);
        console.log("[sd] done");
        status = 4;
        litex.setLeds(4);
    } catch (e) {
        console.log("[sd] error", e);
        status = 8;
        litex.setLeds(8);
    }
}

console.log("[sd] auto-loading", path, "from SDCard");
console.log("[sd] edit main.js on the SDCard, reinsert it, then press BTN0 to reload");

litex.setLeds(1);
runMain("boot");

while (1) {
    var buttons = litex.getButtons();
    if ((buttons & 1) && !(last & 1)) {
        runMain("BTN0");
    }
    last = buttons;
    if (status === 4 && litex.millis() >= nextPulse) {
        pulse = pulse ? 0 : 1;
        litex.setLeds(pulse ? 5 : 4);
        nextPulse = litex.millis() + 500;
    }
    litex.delay(25);
}
