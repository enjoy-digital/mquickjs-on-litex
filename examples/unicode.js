// Regression test — the NUL sentinel fix in tools/embed_script.py
// must keep this file parseable despite the UTF-8 bytes in the
// comment below.
//
// Comments with em-dashes — and other punctuation “like this” — used
// to blow up the parser before the fix: mquickjs's one-byte lookahead
// past src_len was hitting a random .rodata byte and reporting a
// phantom "unexpected character" error on the line past EOF.
// The NUL sentinel pinned the end-of-input condition so the parser
// stops cleanly.
console.log("unicode ok");
