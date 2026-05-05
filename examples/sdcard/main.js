// Copy this file to the root of a FAT-formatted SDCard as main.js.
// With examples/sdcard_button_loader.js running, press BTN0 on the
// Arty to reload and execute this file live.

console.log("[main.js] hello from SDCard");
console.log("[main.js] switches =", litex.getSwitches());

for (var i = 0; i < 16; i++) {
    litex.setLeds(i);
    litex.delay(80);
}

litex.setLeds(litex.getSwitches());
