#!/usr/bin/env python3

#
# This file is part of mquickjs on LiteX.
#
# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

import json
import socket
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


DEFAULT_BOARD = "192.168.1.50"
DEFAULT_UDP_PORT = 12345
DEFAULT_HTTP_PORT = 8000


PRESETS = {
    "plasma": r"""var w = framebuffer.width;
var h = framebuffer.height;
var scale = 8;
var tw = 48;
var th = 36;
var x0 = (w - tw * scale) >> 1;
var y0 = (h - th * scale) >> 1;
var indexes = new Uint8Array(tw * th);
var palette = new Uint32Array(256);

function tri(n) {
    n &= 255;
    return n < 128 ? n * 2 : 510 - n * 2;
}

function rgb(r, g, b) {
    return ((r & 255) << 16) | ((g & 255) << 8) | (b & 255);
}

for (var i = 0; i < 256; i++)
    palette[i] = rgb(tri(i), tri(i * 2), tri(i + 160));

for (var frame = 0; frame < 24; frame++) {
    for (var y = 0; y < th; y++) {
        for (var x = 0; x < tw; x++) {
            var dx = x - (tw >> 1);
            var dy = y - (th >> 1);
            indexes[y * tw + x] =
                (tri(x * 8 + frame * 5) +
                 tri(y * 7 + frame * 3) +
                 tri(((dx * dx + dy * dy) >> 3) + frame * 5)) & 255;
        }
    }
    framebuffer.blitIndexedScale(indexes, palette, tw, th, x0, y0, scale, frame * 2);
    litex.delay(20);
}
console.log("[live] plasma done");""",
    "fire": r"""var w = framebuffer.width;
var h = framebuffer.height;
var scale = 8;
var tw = 48;
var th = 36;
var x0 = (w - tw * scale) >> 1;
var y0 = (h - th * scale) >> 1;
var heat = new Uint8Array(tw * th);
var palette = new Uint32Array(256);

function rgb(r, g, b) {
    return ((r & 255) << 16) | ((g & 255) << 8) | (b & 255);
}

for (var i = 0; i < 256; i++) {
    var r = Math.min(255, i * 3);
    var g = Math.max(0, Math.min(255, (i - 48) * 3));
    var b = Math.max(0, Math.min(255, (i - 160) * 3));
    palette[i] = rgb(r, g, b);
}

for (var frame = 0; frame < 24; frame++) {
    for (var y = 0; y < th; y++) {
        for (var x = 0; x < tw; x++) {
            var center = x - (tw >> 1);
            var wave = ((x * 37 + frame * 11) ^ (y * 29) ^ ((x + y) * 13)) & 63;
            var taper = (center * center) >> 1;
            var v = (((th - 1 - y) * 255 / th) >> 0) + wave - taper;
            heat[y * tw + x] = Math.max(0, Math.min(255, v));
        }
    }
    framebuffer.blitIndexedScale(heat, palette, tw, th, x0, y0, scale, frame * 2);
    litex.delay(20);
}
console.log("[live] fire done");""",
    "bars": r"""var colors = [0xff0033, 0xffcc00, 0x00dd88, 0x0088ff, 0xcc44ff];
var w = framebuffer.width;
var h = framebuffer.height;

for (var frame = 0; frame < 48; frame++) {
    for (var i = 0; i < colors.length; i++) {
        var y = (((i * 37 + frame * 5) % 80) * h / 80) >> 0;
        framebuffer.fillRect(0, y, w, 6, colors[(i + frame) % colors.length]);
    }
    framebuffer.copyRect(0, 6, w, h - 6, 0, 0);
    litex.delay(15);
}
console.log("[live] bars done");""",
}


def page() -> bytes:
    presets = json.dumps(PRESETS)
    html = f"""<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>mquickjs on LiteX Live</title>
<style>
body {{
    margin: 0;
    background: #080b10;
    color: #e8eef7;
    font: 15px/1.4 system-ui, sans-serif;
}}
header {{
    padding: 16px 20px;
    background: #111821;
    border-bottom: 1px solid #243244;
}}
main {{
    display: grid;
    grid-template-columns: 180px 1fr;
    min-height: calc(100vh - 58px);
}}
nav {{
    padding: 16px;
    border-right: 1px solid #243244;
}}
button {{
    display: block;
    width: 100%;
    margin: 0 0 10px;
    padding: 10px;
    color: #e8eef7;
    background: #192231;
    border: 1px solid #34455e;
    border-radius: 6px;
    cursor: pointer;
}}
button.primary {{
    background: #1d5f4a;
    border-color: #2f9d78;
}}
section {{
    display: grid;
    grid-template-rows: 1fr auto;
}}
textarea {{
    width: 100%;
    height: 100%;
    box-sizing: border-box;
    padding: 16px;
    resize: none;
    color: #f5f7fb;
    background: #080b10;
    border: 0;
    outline: 0;
    font: 13px/1.45 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
}}
pre {{
    min-height: 72px;
    margin: 0;
    padding: 12px 16px;
    color: #b9c7d9;
    background: #0d131b;
    border-top: 1px solid #243244;
    white-space: pre-wrap;
}}
</style>
</head>
<body>
<header>mquickjs on LiteX Live</header>
<main>
<nav>
<button class="primary" onclick="run()">Run</button>
<button onclick="loadPreset('plasma')">Plasma</button>
<button onclick="loadPreset('fire')">Fire</button>
<button onclick="loadPreset('bars')">Bars</button>
</nav>
<section>
<textarea id="code" spellcheck="false"></textarea>
<pre id="log">Ready.</pre>
</section>
</main>
<script>
const presets = {presets};
const code = document.getElementById("code");
const log = document.getElementById("log");

function loadPreset(name) {{
    code.value = presets[name];
}}

async function run() {{
    log.textContent = "Sending...";
    const response = await fetch("/run", {{
        method: "POST",
        headers: {{"Content-Type": "text/plain"}},
        body: code.value
    }});
    log.textContent = await response.text();
}}

loadPreset("plasma");
</script>
</body>
</html>"""
    return html.encode("utf-8")


def send_udp(board, udp_port, source):
    data = source.encode("utf-8")
    if len(data) > 1400:
        return "ERR script is too large for one UDP packet\n"

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(2.0)
        sock.sendto(data, (board, udp_port))
        try:
            reply, _ = sock.recvfrom(4096)
        except socket.timeout:
            return "No reply from board. Check Ethernet link, IP and live firmware.\n"
    return reply.decode("utf-8", errors="replace")


def make_handler(board, udp_port):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            body = page()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            if self.path != "/run":
                self.send_error(404)
                return
            length = int(self.headers.get("Content-Length", "0"))
            source = self.rfile.read(length).decode("utf-8", errors="replace")
            body = send_udp(board, udp_port, source).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt, *args):
            print("[live_bridge] " + fmt % args)

    return Handler


def main():
    parser = argparse.ArgumentParser(description="Browser bridge for the mquickjs live demo.")
    parser.add_argument("--board",     default=DEFAULT_BOARD,
                        help="Board IPv4 address.")
    parser.add_argument("--udp-port",  default=DEFAULT_UDP_PORT, type=int,
                        help="Board UDP live-loader port.")
    parser.add_argument("--http-port", default=DEFAULT_HTTP_PORT, type=int,
                        help="Local browser UI port.")
    args = parser.parse_args()

    server = ThreadingHTTPServer(
        ("127.0.0.1", args.http_port),
        make_handler(args.board, args.udp_port),
    )
    print(f"[live_bridge] open http://127.0.0.1:{args.http_port}")
    print(f"[live_bridge] forwarding to {args.board}:{args.udp_port}/udp")
    server.serve_forever()


if __name__ == "__main__":
    main()
