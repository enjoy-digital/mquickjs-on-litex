// SDCard auto-run demo for LiteX boards.
//
// Build this script into the firmware, put a FAT-formatted SDCard in
// the board with /main.js on it, and the board will load it at boot.

var path = "main.js";

function runMain() {
    console.log("[sd] loading", path);
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

console.log("[sd] auto-loading", path, "from SDCard");
console.log("[sd] edit main.js on the SDCard, then reset the board to run it again");

litex.setLeds(1);
runMain();
