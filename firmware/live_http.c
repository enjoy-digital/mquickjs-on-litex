// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#include <generated/csr.h>
#include <generated/soc.h>

#if defined(CSR_ETHMAC_BASE) && LITEX_MQJS_LIVE_HTTP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"

#include "liteeth_lwip.h"
#include "live_http.h"
#include "live_runtime.h"
#include "mqjs_config.h"
#include "mqjs_port.h"

#ifndef LITEX_MQJS_LIVE_PORT
#define LITEX_MQJS_LIVE_PORT 80
#endif

#define LIVE_HTTP_MAX_CONNS     2
#define LIVE_HTTP_REQ_MAX       24576
#define LIVE_HTTP_SCRIPT_MAX    16384
#define LIVE_HTTP_LOG_MAX       2048

#if defined(CSR_LEDS_BASE)
#define LIVE_HTTP_HAS_LEDS 1
#else
#define LIVE_HTTP_HAS_LEDS 0
#endif

#if defined(CSR_SWITCHES_BASE)
#define LIVE_HTTP_HAS_SWITCHES 1
#else
#define LIVE_HTTP_HAS_SWITCHES 0
#endif

#if defined(CSR_BUTTONS_BASE)
#define LIVE_HTTP_HAS_BUTTONS 1
#else
#define LIVE_HTTP_HAS_BUTTONS 0
#endif

#if defined(CSR_CTRL_SCRATCH_ADDR)
#define LIVE_HTTP_HAS_SCRATCH 1
#else
#define LIVE_HTTP_HAS_SCRATCH 0
#endif

#if defined(CSR_SDCARD_BASE) || defined(CSR_SPISDCARD_BASE)
#define LIVE_HTTP_HAS_SDCARD 1
#else
#define LIVE_HTTP_HAS_SDCARD 0
#endif

#if defined(VIDEO_FRAMEBUFFER_BASE) && defined(VIDEO_FRAMEBUFFER_HRES) && \
    defined(VIDEO_FRAMEBUFFER_VRES) && defined(VIDEO_FRAMEBUFFER_DEPTH)
#define LIVE_HTTP_HAS_FRAMEBUFFER 1
#define LIVE_HTTP_FB_WIDTH  VIDEO_FRAMEBUFFER_HRES
#define LIVE_HTTP_FB_HEIGHT VIDEO_FRAMEBUFFER_VRES
#define LIVE_HTTP_FB_DEPTH  VIDEO_FRAMEBUFFER_DEPTH
#else
#define LIVE_HTTP_HAS_FRAMEBUFFER 0
#define LIVE_HTTP_FB_WIDTH  0
#define LIVE_HTTP_FB_HEIGHT 0
#define LIVE_HTTP_FB_DEPTH  0
#endif

#define LIVE_HTTP_JSON_BOOL(v) ((v) ? "true" : "false")

#ifdef MACADDR1
static const uint8_t live_mac[6] = {MACADDR1, MACADDR2, MACADDR3,
                                    MACADDR4, MACADDR5, MACADDR6};
#else
static const uint8_t live_mac[6] = {0x10, 0xe2, 0xd5, 0x00, 0x00, 0x00};
#endif

#ifdef LOCALIP1
static const unsigned int live_ip[4] = {LOCALIP1, LOCALIP2, LOCALIP3, LOCALIP4};
#else
static const unsigned int live_ip[4] = {192, 168, 1, 50};
#endif

static const char live_http_index[] =
"<!doctype html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>mquickjs on LiteX</title>"
"<style>"
"body{margin:0;background:#0d1117;color:#d6deeb;font:14px monospace;display:flex;min-height:100vh}"
"main{margin:auto;width:min(980px,calc(100vw - 32px));padding:24px 0}"
"h1{font-size:22px;margin:0 0 4px;color:#fff}p{margin:0 0 16px;color:#8b949e}"
"textarea{box-sizing:border-box;width:100%;height:380px;background:#111820;color:#d6deeb;border:1px solid #30363d;border-radius:6px;padding:14px;font:14px monospace;line-height:1.45}"
".bar{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0}"
".bar label{display:flex;align-items:center;gap:6px;color:#8b949e}"
"input[type=range]{width:118px}"
"button{background:#238636;color:white;border:0;border-radius:6px;padding:9px 13px;font:13px sans-serif;cursor:pointer}"
"button:disabled{opacity:.35;cursor:not-allowed}button.secondary{background:#30363d}button.tool{background:#1f6feb}"
"pre{white-space:pre-wrap;background:#070b10;border:1px solid #30363d;border-radius:6px;padding:12px;min-height:42px}"
"</style></head><body><main>"
"<h1>mquickjs on LiteX</h1>"
"<p>Edit JavaScript here, press Run, and the board executes it live.</p>"
"<pre id='info'>detecting board...</pre>"
"<textarea id='code'>"
"var t0 = 0;\n"
"\n"
"function setup() {\n"
"  t0 = litex.millis();\n"
"  demo.clear(0x05070a);\n"
"  console.log('[live] setup done');\n"
"}\n"
"\n"
"function frame(t) {\n"
"  var phase = (((t - t0) * params.speed) / 256) | 0;\n"
"  var maxX = framebuffer.width  > 18 ? framebuffer.width  - 18 : 0;\n"
"  var maxY = framebuffer.height > 18 ? framebuffer.height - 18 : 0;\n"
"  demo.clear(demo.hue(phase + params.hue));\n"
"  for (var i = 0; i < params.count; i++) {\n"
"    var x = maxX ? (i * 17 + phase * 5) % maxX : 0;\n"
"    var y = maxY ? (i * 11 + phase * 3) % maxY : 0;\n"
"    framebuffer.fillRect(x, y, 12, 12, demo.hue(phase + i * 3));\n"
"  }\n"
"  demo.led(phase >> 3);\n"
"}\n"
"</textarea>"
"<div class='bar'>"
"<button onclick='run()'>Run</button>"
"<button class='secondary' onclick='control(\"stop\")'>Stop</button>"
"<button class='secondary' onclick='control(\"pause\")'>Pause</button>"
"<button class='secondary' onclick='control(\"resume\")'>Resume</button>"
"<button class='secondary' onclick='control(\"reset\")'>Reset</button>"
"<button class='secondary' onclick='preset(\"bars\")'>Bars</button>"
"<button class='secondary' onclick='preset(\"plasma\")'>Plasma</button>"
"<button class='secondary' onclick='preset(\"spark\")'>Spark</button>"
"<button class='secondary' onclick='preset(\"tunnel\")'>Tunnel</button>"
"</div>"
"<div class='bar'>"
"<label>speed <input id='speed' type='range' min='1' max='96' value='32' onchange='syncParams()'></label>"
"<label>scale <input id='scale' type='range' min='2' max='8' value='6' onchange='syncParams()'></label>"
"<label>count <input id='count' type='range' min='16' max='256' value='96' onchange='syncParams()'></label>"
"<label>hue <input id='hue' type='range' min='0' max='255' value='0' onchange='syncParams()'></label>"
"</div>"
"<div class='bar'>"
"<button class='tool' onclick='action(\"info\")'>Identify</button>"
"<button class='tool' data-feature='io' onclick='action(\"io\")'>Read I/O</button>"
"<button class='tool' data-feature='scratch' onclick='action(\"scratch\")'>Scratch</button>"
"<button class='tool' data-feature='leds' onclick='action(\"led1\")'>LED 1</button>"
"<button class='tool' data-feature='leds' onclick='action(\"led2\")'>LED 2</button>"
"<button class='tool' data-feature='leds' onclick='action(\"led4\")'>LED 4</button>"
"<button class='tool' data-feature='leds' onclick='action(\"led8\")'>LED 8</button>"
"<button class='tool' data-feature='leds' onclick='action(\"chase\")'>Chase</button>"
"<button class='tool' data-feature='framebuffer' onclick='action(\"clear\")'>Clear</button>"
"</div>"
"<pre id='log'>ready</pre>"
"<script>"
"const code=document.getElementById('code'),log=document.getElementById('log'),infoEl=document.getElementById('info');"
"const speed=document.getElementById('speed'),scale=document.getElementById('scale'),count=document.getElementById('count'),hue=document.getElementById('hue');"
"let boardInfo=null;"
"const helper=`var params={speed:32,scale:6,count:96,hue:0};\\n"
"var demo={\\n"
"  rgb:function(r,g,b){return ((r&255)<<16)|((g&255)<<8)|(b&255);},\\n"
"  hue:function(h){h&=255;return demo.rgb((h*5)&255,(255-h)&255,(h*3)&255);},\\n"
"  clear:function(c){if(framebuffer.width)framebuffer.clear(c);},\\n"
"  led:function(n){litex.setLeds(1<<(n&3));},\\n"
"  centerX:function(w){return ((framebuffer.width-w)/2)|0;},\\n"
"  centerY:function(h){return ((framebuffer.height-h)/2)|0;},\\n"
"  blitIndexed:function(idx,pal,w,h,s){framebuffer.blitIndexedScale(idx,pal,w,h,demo.centerX(w*s),demo.centerY(h*s),s);}\\n"
"};\\n`;"
"const presets={"
"bars:`var t0=0;\\n"
"function setup(){t0=litex.millis();console.log('[bars] running');}\\n"
"function frame(t){\\n"
"  var p=(((t-t0)*params.speed)/256)|0;\\n"
"  for(var y=0;y<=framebuffer.height-8;y+=8){\\n"
"    var c=demo.hue(y+p+params.hue);\\n"
"    framebuffer.fillRect(0,y,framebuffer.width,8,c);\\n"
"  }\\n"
"  demo.led(p>>4);\\n"
"}\\n`,"
"plasma:`var w=80,h=60,idx,pal,t0=0;\\n"
"function setup(){\\n"
"  t0=litex.millis();idx=new Uint8Array(w*h);pal=new Uint32Array(256);\\n"
"  for(var i=0;i<256;i++)pal[i]=demo.hue(i);\\n"
"  console.log('[plasma] low-res indexed plasma');\\n"
"}\\n"
"function frame(t){\\n"
"  var p=(((t-t0)*params.speed)/128)|0;\\n"
"  for(var y=0;y<h;y++)for(var x=0;x<w;x++)idx[y*w+x]=((x*x+y*y+(x*p)+(y*(p>>1)))>>5)&255;\\n"
"  demo.blitIndexed(idx,pal,w,h,params.scale);\\n"
"  demo.led(p>>4);\\n"
"}\\n`,"
"spark:`var t0=0;\\n"
"function setup(){t0=litex.millis();demo.clear(0);console.log('[spark] particles');}\\n"
"function frame(t){\\n"
"  var p=(((t-t0)*params.speed)/192)|0;demo.clear(0);\\n"
"  var mx=framebuffer.width>6?framebuffer.width-6:0,my=framebuffer.height>6?framebuffer.height-6:0;\\n"
"  for(var i=0;i<params.count;i++){\\n"
"    var x=mx?(i*23+p*9)%mx:0,y=my?(i*i+p*13)%my:0;\\n"
"    framebuffer.fillRect(x,y,6,6,demo.hue(params.hue+p+i));\\n"
"  }\\n"
"  demo.led(p>>3);\\n"
"}\\n`,"
"tunnel:`var w=80,h=60,idx,pal,t0=0;\\n"
"function setup(){\\n"
"  t0=litex.millis();idx=new Uint8Array(w*h);pal=new Uint32Array(256);\\n"
"  for(var i=0;i<256;i++)pal[i]=demo.hue(i+params.hue);\\n"
"  console.log('[tunnel] integer tunnel');\\n"
"}\\n"
"function frame(t){\\n"
"  var p=(((t-t0)*params.speed)/160)|0,cx=w>>1,cy=h>>1;\\n"
"  for(var y=0;y<h;y++)for(var x=0;x<w;x++){\\n"
"    var dx=x-cx,dy=y-cy,d=(dx*dx+dy*dy+1);\\n"
"    idx[y*w+x]=((4096/d)+(x*3)+(y*5)+p)&255;\\n"
"  }\\n"
"  demo.blitIndexed(idx,pal,w,h,params.scale);\\n"
"  demo.led(p>>4);\\n"
"}\\n`};"
"const actions={"
"info:`console.log('[board] identifier =', litex.getIdentifier());\\n"
"console.log('[board] clock =', litex.clockFrequency(), 'Hz');\\n"
"console.log('[board] framebuffer =', framebuffer.width, 'x', framebuffer.height, 'x', framebuffer.depth);\\n`,"
"io:`console.log('[board] switches =', litex.getSwitches());\\n"
"console.log('[board] buttons =', litex.getButtons());\\n`,"
"scratch:`var before=litex.getScratch();\\n"
"litex.setScratch(0x51c0ffee);\\n"
"var after=litex.getScratch();\\n"
"litex.setScratch(before);\\n"
"console.log('[board] scratch before = 0x'+before.toString(16));\\n"
"console.log('[board] scratch test = 0x'+after.toString(16));\\n`,"
"led1:`litex.setLeds(1); console.log('[board] LEDs = 0x1');\\n`,"
"led2:`litex.setLeds(2); console.log('[board] LEDs = 0x2');\\n`,"
"led4:`litex.setLeds(4); console.log('[board] LEDs = 0x4');\\n`,"
"led8:`litex.setLeds(8); console.log('[board] LEDs = 0x8');\\n`,"
"chase:`for (var i=0; i<16; i++) { litex.setLeds(1 << (i & 3)); litex.delay(80); }\\n"
"litex.setLeds(0); console.log('[board] LED chase done');\\n`,"
"clear:`framebuffer.clear(0); litex.setLeds(0); console.log('[board] cleared');\\n`};"
"function preset(n){code.value=presets[n];}"
"function action(n){send(actions[n]);}"
"async function send(src){log.textContent='running...';try{"
"const r=await fetch('/run',{method:'POST',headers:{'Content-Type':'text/plain'},body:src});"
"log.textContent=await r.text();await refreshInfo();}catch(e){log.textContent='ERR '+e;}}"
"async function evalJS(src){try{"
"const r=await fetch('/eval',{method:'POST',headers:{'Content-Type':'text/plain'},body:src});"
"log.textContent=await r.text();await refreshInfo();}catch(e){log.textContent='ERR '+e;}}"
"async function control(cmd){log.textContent=cmd+'...';try{"
"const r=await fetch('/control',{method:'POST',headers:{'Content-Type':'text/plain'},body:cmd});"
"log.textContent=await r.text();await refreshInfo();}catch(e){log.textContent='ERR '+e;}}"
"function run(){send(helper+code.value);}"
"function syncParams(){evalJS('params.speed='+speed.value+';params.scale='+scale.value+';params.count='+count.value+';params.hue='+hue.value+';console.log(\"[params] speed='+speed.value+' scale='+scale.value+' count='+count.value+' hue='+hue.value+'\");');}"
"function featureOK(n){if(!boardInfo)return true;if(n==='io')return boardInfo.features.switches||boardInfo.features.buttons;return !!boardInfo.features[n];}"
"function applyInfo(){"
"var f=boardInfo.features,fb=boardInfo.framebuffer;"
"var live=boardInfo.live;"
"infoEl.textContent=boardInfo.identifier+'\\nCPU '+boardInfo.cpu+' @ '+boardInfo.clock_hz+' Hz, heap '+boardInfo.heap_bytes+' bytes\\nframebuffer '+(fb.present?(fb.width+' x '+fb.height+' x '+fb.depth):'not present')+'\\nfeatures: leds='+f.leds+' switches='+f.switches+' buttons='+f.buttons+' sdcard='+f.sdcard+'\\nlive: '+(live.active?(live.paused?'paused':'running'):'idle')+', frames='+live.frames+', last='+live.last_frame_ms+' ms';"
"document.querySelectorAll('[data-feature]').forEach(function(e){e.disabled=!featureOK(e.dataset.feature);});"
"}"
"async function refreshInfo(){try{boardInfo=await (await fetch('/info')).json();applyInfo();}catch(e){infoEl.textContent='board info unavailable: '+e;}}"
"refreshInfo();"
"setInterval(refreshInfo,1000);"
"</script></main></body></html>\n";

struct live_http_conn {
    struct tcp_pcb *pcb;
    char request[LIVE_HTTP_REQ_MAX + 1];
    char response_body[LIVE_HTTP_LOG_MAX + 64];
    size_t request_len;
    const char *response;
    size_t response_len;
    size_t response_sent;
    char response_buf[128];
    uint8_t used;
};

static struct live_http_conn live_http_conns[LIVE_HTTP_MAX_CONNS];
static JSContext **live_http_ctxp;
static char live_http_log[LIVE_HTTP_LOG_MAX + 1];
static size_t live_http_log_len;
static uint8_t live_http_active;
static uint8_t live_http_paused;
static uint32_t live_http_frame_count;
static uint32_t live_http_last_frame_ms;
static uint32_t live_http_next_frame_ms;
static char live_http_state[32] = "idle";

static uint32_t live_http_millis(void)
{
#if defined(CSR_TIMER0_UPTIME_CYCLES_ADDR)
    timer0_uptime_latch_write(1);
    return (uint32_t)(timer0_uptime_cycles_read() /
        (CONFIG_CLOCK_FREQUENCY / 1000));
#else
    static uint32_t fallback_ms;

    fallback_ms += 16;
    return fallback_ms;
#endif
}

static void live_http_log_reset(void)
{
    live_http_log_len = 0;
    live_http_log[0]  = 0;
}

static void live_http_log_append(const void *buf, size_t len)
{
    size_t avail;

    if (live_http_log_len >= LIVE_HTTP_LOG_MAX)
        return;

    avail = LIVE_HTTP_LOG_MAX - live_http_log_len;
    if (len > avail)
        len = avail;

    memcpy(&live_http_log[live_http_log_len], buf, len);
    live_http_log_len += len;
    live_http_log[live_http_log_len] = 0;
}

static void live_http_log_func(void *opaque, const void *buf, size_t buf_len)
{
    const char *p = buf;

    (void)opaque;

    for (size_t i = 0; i < buf_len; i++)
        putchar(p[i]);
    live_http_log_append(buf, buf_len);
}

static uint32_t ip_from_octets(const unsigned int ip[4])
{
    return ((ip[0] & 0xff) << 24) |
           ((ip[1] & 0xff) << 16) |
           ((ip[2] & 0xff) <<  8) |
           ((ip[3] & 0xff) <<  0);
}

static struct live_http_conn *live_http_alloc_conn(void)
{
    for (int i = 0; i < LIVE_HTTP_MAX_CONNS; i++) {
        if (!live_http_conns[i].used) {
            memset(&live_http_conns[i], 0, sizeof(live_http_conns[i]));
            live_http_conns[i].used = 1;
            return &live_http_conns[i];
        }
    }
    return NULL;
}

static void live_http_free_conn(struct live_http_conn *conn)
{
    if (conn != NULL)
        memset(conn, 0, sizeof(*conn));
}

static int live_http_content_length(const char *request)
{
    const char *p = strstr(request, "Content-Length:");

    if (p == NULL)
        return 0;
    p += strlen("Content-Length:");
    while (*p == ' ')
        p++;

    return atoi(p);
}

static err_t live_http_send_more(struct live_http_conn *conn)
{
    while (conn->response_sent < conn->response_len) {
        u16_t chunk;
        u16_t avail = tcp_sndbuf(conn->pcb);
        err_t err;

        if (avail == 0)
            break;

        chunk = conn->response_len - conn->response_sent;
        if (chunk > avail)
            chunk = avail;
        if (chunk > 1024)
            chunk = 1024;

        err = tcp_write(conn->pcb,
                        &conn->response[conn->response_sent],
                        chunk,
                        TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK)
            return err;
        conn->response_sent += chunk;
    }

    tcp_output(conn->pcb);

    if (conn->response_sent == conn->response_len) {
        tcp_arg(conn->pcb, NULL);
        tcp_sent(conn->pcb, NULL);
        tcp_recv(conn->pcb, NULL);
        tcp_poll(conn->pcb, NULL, 0);
        tcp_close(conn->pcb);
        live_http_free_conn(conn);
    }

    return ERR_OK;
}

static void live_http_reply_len(struct live_http_conn *conn,
                                const char *status,
                                const char *type,
                                const char *body,
                                size_t body_len)
{
    size_t header_len;

    header_len = snprintf(conn->response_buf, sizeof(conn->response_buf),
                          "HTTP/1.1 %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %u\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          status, type, (unsigned)body_len);

    if ((header_len + body_len) >= sizeof(conn->request)) {
        body     = "ERR response too large\n";
        body_len = strlen(body);
        header_len = snprintf(conn->response_buf, sizeof(conn->response_buf),
                              "HTTP/1.1 500 Internal Server Error\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: %u\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              (unsigned)body_len);
    }

    memcpy(conn->request, conn->response_buf, header_len);
    memcpy(&conn->request[header_len], body, body_len);
    conn->response      = conn->request;
    conn->response_len  = header_len + body_len;
    conn->response_sent = 0;
    live_http_send_more(conn);
}

static void live_http_reply(struct live_http_conn *conn,
                            const char *status,
                            const char *type,
                            const char *body)
{
    live_http_reply_len(conn, status, type, body, strlen(body));
}

static void live_http_set_state(const char *state)
{
    snprintf(live_http_state, sizeof(live_http_state), "%s", state);
}

static void live_http_stop_script(const char *state)
{
    live_http_active = 0;
    live_http_paused = 0;
    live_http_set_state(state);
}

static void live_http_dump_exception(JSContext *ctx)
{
    JSValue e = JS_GetException(ctx);

    fputs("error: ", stdout);
    JS_PrintValueF(ctx, e, JS_DUMP_LONG);
    putchar('\n');
}

static int live_http_has_function(JSContext *ctx, const char *name)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue func   = JS_GetPropertyStr(ctx, global, name);

    return JS_IsFunction(ctx, func);
}

static int live_http_call_function(JSContext *ctx, const char *name,
                                   uint32_t arg, int use_arg)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue func   = JS_GetPropertyStr(ctx, global, name);
    JSValue ret;

    if (!JS_IsFunction(ctx, func))
        return 0;
    if (JS_StackCheck(ctx, use_arg ? 3 : 2)) {
        JS_ThrowOutOfMemory(ctx);
        live_http_dump_exception(ctx);
        return -1;
    }

    if (use_arg)
        JS_PushArg(ctx, JS_NewInt32(ctx, arg));
    JS_PushArg(ctx, func);
    JS_PushArg(ctx, global);
    ret = JS_Call(ctx, use_arg ? 1 : 0);
    if (JS_IsException(ret)) {
        live_http_dump_exception(ctx);
        return -1;
    }

    return 1;
}

static const char *live_http_identifier(void)
{
#if defined(CONFIG_IDENTIFIER)
    return config_identifier_read();
#else
    return "LiteX SoC";
#endif
}

static void live_http_info(struct live_http_conn *conn)
{
    int response_len;

    response_len = snprintf(conn->response_body, sizeof(conn->response_body),
        "{"
        "\"identifier\":\"%s\","
        "\"cpu\":\"%s\","
        "\"clock_hz\":%u,"
        "\"heap_bytes\":%u,"
        "\"features\":{"
        "\"leds\":%s,"
        "\"switches\":%s,"
        "\"buttons\":%s,"
        "\"scratch\":%s,"
        "\"sdcard\":%s,"
        "\"framebuffer\":%s"
        "},"
        "\"framebuffer\":{"
        "\"present\":%s,"
        "\"width\":%u,"
        "\"height\":%u,"
        "\"depth\":%u"
        "},"
        "\"live\":{"
        "\"active\":%s,"
        "\"paused\":%s,"
        "\"state\":\"%s\","
        "\"frames\":%u,"
        "\"last_frame_ms\":%u"
        "}"
        "}\n",
        live_http_identifier(),
        CONFIG_CPU_HUMAN_NAME,
        (unsigned)CONFIG_CLOCK_FREQUENCY,
        (unsigned)LITEX_MQJS_HEAP_SIZE,
        LIVE_HTTP_JSON_BOOL(LIVE_HTTP_HAS_LEDS),
        LIVE_HTTP_JSON_BOOL(LIVE_HTTP_HAS_SWITCHES),
        LIVE_HTTP_JSON_BOOL(LIVE_HTTP_HAS_BUTTONS),
        LIVE_HTTP_JSON_BOOL(LIVE_HTTP_HAS_SCRATCH),
        LIVE_HTTP_JSON_BOOL(LIVE_HTTP_HAS_SDCARD),
        LIVE_HTTP_JSON_BOOL(LIVE_HTTP_HAS_FRAMEBUFFER),
        LIVE_HTTP_JSON_BOOL(LIVE_HTTP_HAS_FRAMEBUFFER),
        (unsigned)LIVE_HTTP_FB_WIDTH,
        (unsigned)LIVE_HTTP_FB_HEIGHT,
        (unsigned)LIVE_HTTP_FB_DEPTH,
        LIVE_HTTP_JSON_BOOL(live_http_active),
        LIVE_HTTP_JSON_BOOL(live_http_paused),
        live_http_state,
        (unsigned)live_http_frame_count,
        (unsigned)live_http_last_frame_ms);
    if (response_len < 0)
        response_len = 0;
    if (response_len >= (int)sizeof(conn->response_body))
        response_len = sizeof(conn->response_body) - 1;

    live_http_reply_len(conn, "200 OK", "application/json",
                        conn->response_body, response_len);
}

static void live_http_run(struct live_http_conn *conn, const char *body, size_t len)
{
    int response_len;

    if (len > LIVE_HTTP_SCRIPT_MAX) {
        live_http_reply(conn, "413 Payload Too Large", "text/plain",
                        "ERR script too large\n");
        return;
    }

    printf("[live] HTTP script: %u bytes\n", (unsigned)len);
    JS_FreeContext(*live_http_ctxp);
    *live_http_ctxp = new_mqjs_context();
    live_http_log_reset();
    if (*live_http_ctxp == NULL) {
        live_http_reply(conn, "500 Internal Server Error", "text/plain",
                        "ERR JS_NewContext failed\n");
        return;
    }
    JS_SetLogFunc(*live_http_ctxp, live_http_log_func);
    mqjs_set_print_func(live_http_log_func);
    live_http_stop_script("idle");

    if (run_source(*live_http_ctxp, body, len, "live_http.js", JS_EVAL_REPL) == 0) {
        if (live_http_call_function(*live_http_ctxp, "setup", 0, 0) < 0) {
            mqjs_set_print_func(NULL);
            live_http_stop_script("setup failed");
            puts("[live] setup failed");
            response_len = snprintf(conn->response_body,
                                    sizeof(conn->response_body),
                                    "ERR setup failed\n%s", live_http_log);
            if (response_len < 0)
                response_len = 0;
            if (response_len >= (int)sizeof(conn->response_body))
                response_len = sizeof(conn->response_body) - 1;
            live_http_reply_len(conn, "500 Internal Server Error", "text/plain",
                                conn->response_body, response_len);
            return;
        }
        if (live_http_has_function(*live_http_ctxp, "frame")) {
            live_http_active        = 1;
            live_http_paused        = 0;
            live_http_frame_count   = 0;
            live_http_last_frame_ms = 0;
            live_http_next_frame_ms = live_http_millis();
            live_http_set_state("running");
        }
        mqjs_set_print_func(NULL);
        puts("[live] script ok");
        response_len = snprintf(conn->response_body, sizeof(conn->response_body),
                                live_http_active ?
                                "OK live loop started\n%s" : "OK\n%s",
                                live_http_log);
        if (response_len < 0)
            response_len = 0;
        if (response_len >= (int)sizeof(conn->response_body))
            response_len = sizeof(conn->response_body) - 1;
        live_http_reply_len(conn, "200 OK", "text/plain",
                            conn->response_body, response_len);
    } else {
        mqjs_set_print_func(NULL);
        puts("[live] script failed");
        response_len = snprintf(conn->response_body, sizeof(conn->response_body),
                                "ERR script failed\n%s", live_http_log);
        if (response_len < 0)
            response_len = 0;
        if (response_len >= (int)sizeof(conn->response_body))
            response_len = sizeof(conn->response_body) - 1;
        live_http_reply_len(conn, "500 Internal Server Error", "text/plain",
                            conn->response_body, response_len);
    }
}

static void live_http_eval(struct live_http_conn *conn, const char *body, size_t len)
{
    int response_len;

    if (len > LIVE_HTTP_SCRIPT_MAX) {
        live_http_reply(conn, "413 Payload Too Large", "text/plain",
                        "ERR script too large\n");
        return;
    }
    if (*live_http_ctxp == NULL) {
        live_http_reply(conn, "500 Internal Server Error", "text/plain",
                        "ERR JS context is not ready\n");
        return;
    }

    printf("[live] HTTP eval: %u bytes\n", (unsigned)len);
    live_http_log_reset();
    JS_SetLogFunc(*live_http_ctxp, live_http_log_func);
    mqjs_set_print_func(live_http_log_func);

    if (run_source(*live_http_ctxp, body, len, "live_eval.js", JS_EVAL_REPL) == 0) {
        mqjs_set_print_func(NULL);
        puts("[live] eval ok");
        response_len = snprintf(conn->response_body, sizeof(conn->response_body),
                                "OK\n%s", live_http_log);
        if (response_len < 0)
            response_len = 0;
        if (response_len >= (int)sizeof(conn->response_body))
            response_len = sizeof(conn->response_body) - 1;
        live_http_reply_len(conn, "200 OK", "text/plain",
                            conn->response_body, response_len);
    } else {
        mqjs_set_print_func(NULL);
        puts("[live] eval failed");
        response_len = snprintf(conn->response_body, sizeof(conn->response_body),
                                "ERR eval failed\n%s", live_http_log);
        if (response_len < 0)
            response_len = 0;
        if (response_len >= (int)sizeof(conn->response_body))
            response_len = sizeof(conn->response_body) - 1;
        live_http_reply_len(conn, "500 Internal Server Error", "text/plain",
                            conn->response_body, response_len);
    }
}

static int live_http_body_is(const char *body, size_t len, const char *cmd)
{
    size_t cmd_len = strlen(cmd);

    while (len > 0 && (body[len - 1] == '\r' || body[len - 1] == '\n' ||
                       body[len - 1] == ' '))
        len--;
    return len == cmd_len && memcmp(body, cmd, cmd_len) == 0;
}

static void live_http_reset_context(void)
{
    JS_FreeContext(*live_http_ctxp);
    *live_http_ctxp = new_mqjs_context();
    live_http_stop_script(*live_http_ctxp == NULL ? "reset failed" : "idle");
}

static void live_http_control(struct live_http_conn *conn,
                              const char *body, size_t len)
{
    if (live_http_body_is(body, len, "stop")) {
        live_http_stop_script("stopped");
        live_http_reply(conn, "200 OK", "text/plain", "OK stopped\n");
    } else if (live_http_body_is(body, len, "pause")) {
        if (live_http_active) {
            live_http_paused = 1;
            live_http_set_state("paused");
        }
        live_http_reply(conn, "200 OK", "text/plain", "OK paused\n");
    } else if (live_http_body_is(body, len, "resume")) {
        if (live_http_active) {
            live_http_paused = 0;
            live_http_set_state("running");
        }
        live_http_reply(conn, "200 OK", "text/plain", "OK resumed\n");
    } else if (live_http_body_is(body, len, "reset")) {
        live_http_reset_context();
        live_http_reply(conn, "200 OK", "text/plain", "OK reset\n");
    } else {
        live_http_reply(conn, "400 Bad Request", "text/plain",
                        "ERR unknown control\n");
    }
}

static int live_http_parse(struct live_http_conn *conn)
{
    char *body;
    int body_len;
    int header_len;
    int content_len;
    int is_control;
    int is_eval;
    int is_run;

    conn->request[conn->request_len] = 0;
    body = strstr(conn->request, "\r\n\r\n");
    if (body == NULL)
        return 0;

    body      += 4;
    header_len = body - conn->request;

    if (strncmp(conn->request, "GET / ", 6) == 0 ||
        strncmp(conn->request, "GET /HTTP", 9) == 0) {
        live_http_reply(conn, "200 OK", "text/html", live_http_index);
        return 1;
    }

    if (strncmp(conn->request, "GET /info ", 10) == 0 ||
        strncmp(conn->request, "GET /info?", 10) == 0) {
        live_http_info(conn);
        return 1;
    }

    is_run = strncmp(conn->request, "POST /run ", 10) == 0 ||
             strncmp(conn->request, "POST /run? ", 11) == 0;
    is_eval = strncmp(conn->request, "POST /eval ", 11) == 0 ||
              strncmp(conn->request, "POST /eval? ", 12) == 0;
    is_control = strncmp(conn->request, "POST /control ", 14) == 0 ||
                 strncmp(conn->request, "POST /control? ", 15) == 0;
    if (!is_run && !is_eval && !is_control) {
        live_http_reply(conn, "404 Not Found", "text/plain", "ERR not found\n");
        return 1;
    }

    content_len = live_http_content_length(conn->request);
    body_len    = conn->request_len - header_len;
    if (body_len < content_len)
        return 0;

    if (is_run)
        live_http_run(conn, body, content_len);
    else if (is_eval)
        live_http_eval(conn, body, content_len);
    else
        live_http_control(conn, body, content_len);

    return 1;
}

static void live_http_service_frame(void)
{
    uint32_t elapsed;
    uint32_t now;
    uint32_t start;

    if (!live_http_active || live_http_paused || *live_http_ctxp == NULL)
        return;

    now = live_http_millis();
    if ((int32_t)(now - live_http_next_frame_ms) < 0)
        return;

    start = live_http_millis();
    mqjs_set_print_func(NULL);
    if (live_http_call_function(*live_http_ctxp, "frame", now, 1) < 0) {
        live_http_stop_script("frame failed");
        puts("[live] frame failed");
        return;
    }
    elapsed = live_http_millis() - start;
    live_http_frame_count++;
    live_http_last_frame_ms = elapsed;
    live_http_next_frame_ms = live_http_millis() + 33;
}

static err_t live_http_recv(void *arg, struct tcp_pcb *pcb,
                            struct pbuf *p, err_t err)
{
    struct live_http_conn *conn = arg;

    (void)pcb;

    if (p == NULL || err != ERR_OK) {
        live_http_free_conn(conn);
        return ERR_OK;
    }

    if ((conn->request_len + p->tot_len) > LIVE_HTTP_REQ_MAX) {
        pbuf_free(p);
        live_http_reply(conn, "413 Payload Too Large", "text/plain",
                        "ERR request too large\n");
        return ERR_OK;
    }

    pbuf_copy_partial(p, &conn->request[conn->request_len], p->tot_len, 0);
    conn->request_len += p->tot_len;
    tcp_recved(conn->pcb, p->tot_len);
    pbuf_free(p);

    live_http_parse(conn);

    return ERR_OK;
}

static err_t live_http_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    struct live_http_conn *conn = arg;

    (void)pcb;
    (void)len;

    if (conn != NULL)
        return live_http_send_more(conn);
    return ERR_OK;
}

static void live_http_err(void *arg, err_t err)
{
    (void)err;
    live_http_free_conn(arg);
}

static err_t live_http_poll(void *arg, struct tcp_pcb *pcb)
{
    (void)pcb;
    (void)arg;

    return ERR_OK;
}

static err_t live_http_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    struct live_http_conn *conn;

    (void)arg;

    if (err != ERR_OK)
        return err;

    conn = live_http_alloc_conn();
    if (conn == NULL) {
        tcp_abort(newpcb);
        return ERR_ABRT;
    }

    conn->pcb = newpcb;
    tcp_arg(newpcb, conn);
    tcp_recv(newpcb, live_http_recv);
    tcp_sent(newpcb, live_http_sent);
    tcp_err(newpcb, live_http_err);
    tcp_poll(newpcb, live_http_poll, 4);

    return ERR_OK;
}

void live_http_loop(JSContext **ctxp)
{
    struct tcp_pcb *pcb;
    struct tcp_pcb *listen_pcb;
    struct netif netif;
    uint32_t ip = ip_from_octets(live_ip);

    live_http_ctxp = ctxp;

    puts("[live] starting browser JavaScript receiver");
    printf("[live] local IP: %u.%u.%u.%u\n",
           live_ip[0], live_ip[1], live_ip[2], live_ip[3]);
    printf("[live] open: http://%u.%u.%u.%u/\n",
           live_ip[0], live_ip[1], live_ip[2], live_ip[3]);

    if (liteeth_lwip_init(&netif, live_mac, ip, 0xffffff00,
                          (ip & 0xffffff00) | 1) != 0) {
        puts("[live] lwIP/LiteEth init failed");
        return;
    }

    pcb = tcp_new();
    if (pcb == NULL) {
        puts("[live] tcp_new failed");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, LITEX_MQJS_LIVE_PORT) != ERR_OK) {
        puts("[live] tcp_bind failed");
        return;
    }

    listen_pcb = tcp_listen(pcb);
    if (listen_pcb == NULL) {
        puts("[live] tcp_listen failed");
        return;
    }
    tcp_accept(listen_pcb, live_http_accept);

    for (;;) {
        liteeth_lwip_poll(&netif);
        sys_check_timeouts();
        live_http_service_frame();
    }
}

#endif /* defined(CSR_ETHMAC_BASE) && LITEX_MQJS_LIVE_HTTP */
