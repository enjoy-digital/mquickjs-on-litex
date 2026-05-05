// SDCard live-reload demo for Digilent Arty.
//
// Build this script into the firmware, put a FAT-formatted SDCard in
// the board with /main.js on it, then press BTN0 to load and run that
// file without rebuilding or reloading the firmware.

var path = "main.js";
var last = 0;
var runs = 0;

console.log("[sd] waiting for BTN0 to load", path);
console.log("[sd] edit main.js on the SDCard, reinsert it, then press BTN0");

litex.setLeds(1);

while (1) {
    var buttons = litex.getButtons();
    if ((buttons & 1) && !(last & 1)) {
        runs++;
        console.log("[sd] loading", path, "run", runs);
        litex.setLeds(2);
        try {
            litex.load(path);
            console.log("[sd] done");
            litex.setLeds(4);
        } catch (e) {
            console.log("[sd] error", e);
            litex.setLeds(8);
        }
    }
    last = buttons;
    litex.delay(25);
}
