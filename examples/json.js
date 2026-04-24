// JSON round-trip + typed arrays. Exercises the stdlib's JSON object,
// Array prototype methods (map/filter/reduce), and console.log value
// pretty-printing.

var data = JSON.parse('{"name":"litex","cores":["vexriscv","naxriscv","neorv32"],"year":2026}');

console.log("name =", data.name);
console.log("cores:", data.cores.map(function (c) { return c.toUpperCase(); }).join(", "));

var sum = [1, 2, 3, 4, 5, 6, 7, 8].reduce(function (a, b) { return a + b; }, 0);
console.log("sum =", sum);

// Round-trip back to a string to prove stringify works.
console.log("roundtrip:", JSON.stringify({ sum: sum, year: data.year }));

// Also check typed arrays: compute cumulative sum in an Int32Array.
var n = 5;
var cum = new Int32Array(n);
cum[0] = 1;
for (var i = 1; i < n; i++) cum[i] = cum[i - 1] + (i + 1);
console.log("cum:", JSON.stringify(Array.from ? Array.from(cum) : [cum[0], cum[1], cum[2], cum[3], cum[4]]));
