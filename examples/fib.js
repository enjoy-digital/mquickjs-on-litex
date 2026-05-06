// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Recursive Fibonacci: a CPU-bound workout that also exercises the GC
// through the call stack.
function fib(n) {
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

var t0 = performance.now();
var r  = fib(20);
var t1 = performance.now();

console.log("fib(20) =", r);
console.log("elapsed_ms =", t1 - t0);
