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

#ifndef LITEX_MQJS_LIVE_PORT
#define LITEX_MQJS_LIVE_PORT 80
#endif

#define LIVE_HTTP_MAX_CONNS     2
#define LIVE_HTTP_REQ_MAX       12288
#define LIVE_HTTP_SCRIPT_MAX    8192

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
"button{background:#238636;color:white;border:0;border-radius:6px;padding:9px 13px;font:13px sans-serif;cursor:pointer}"
"button.secondary{background:#30363d}"
"pre{white-space:pre-wrap;background:#070b10;border:1px solid #30363d;border-radius:6px;padding:12px;min-height:42px}"
"</style></head><body><main>"
"<h1>mquickjs on LiteX</h1>"
"<p>Edit JavaScript here, press Run, and the board executes it live.</p>"
"<textarea id='code'>"
"console.log('[browser] live JavaScript on LiteX');\n"
"for (let frame = 0; frame < 32; frame++) {\n"
"  const phase = frame * 13;\n"
"  framebuffer.clear(((phase & 255) << 16) | (((255 - phase) & 255) << 8));\n"
"  for (let i = 0; i < 96; i++) {\n"
"    const x = (i * 17 + frame * 7) % framebuffer.width;\n"
"    const y = (i * 11 + frame * 5) % framebuffer.height;\n"
"    framebuffer.fillRect(x, y, 10, 10, 0xffc857);\n"
"  }\n"
"}\n"
"console.log('[browser] done');\n"
"</textarea>"
"<div class='bar'>"
"<button onclick='run()'>Run</button>"
"<button class='secondary' onclick='preset(\"bars\")'>Bars</button>"
"<button class='secondary' onclick='preset(\"plasma\")'>Plasma</button>"
"<button class='secondary' onclick='preset(\"spark\")'>Spark</button>"
"</div>"
"<pre id='log'>ready</pre>"
"<script>"
"const code=document.getElementById('code'),log=document.getElementById('log');"
"const presets={"
"bars:`for (let f=0; f<48; f++) {\\n"
"  for (let y=0; y<framebuffer.height; y+=8) {\\n"
"    const c=((y*3+f*9)&255)<<16 | ((255-y+f*5)&255)<<8 | ((y+f*7)&255);\\n"
"    framebuffer.fillRect(0,y,framebuffer.width,8,c);\\n"
"  }\\n"
"}\\n`,"
"plasma:`const w=framebuffer.width, h=framebuffer.height;\\n"
"for (let f=0; f<24; f++) {\\n"
"  for (let y=0; y<h; y+=4) for (let x=0; x<w; x+=4) {\\n"
"    const v=((x*x+y*y+f*173)>>6)&255;\\n"
"    framebuffer.fillRect(x,y,4,4,(v<<16)|(((255-v)&255)<<8)|((v*3)&255));\\n"
"  }\\n"
"}\\n`,"
"spark:`for (let f=0; f<64; f++) {\\n"
"  framebuffer.clear(0);\\n"
"  for (let i=0; i<160; i++) {\\n"
"    const x=(i*23+f*9)%framebuffer.width, y=(i*i+f*13)%framebuffer.height;\\n"
"    framebuffer.fillRect(x,y,6,6,0x40d9ff);\\n"
"  }\\n"
"}\\n`};"
"function preset(n){code.value=presets[n];}"
"async function run(){log.textContent='running...';try{"
"const r=await fetch('/run',{method:'POST',headers:{'Content-Type':'text/plain'},body:code.value});"
"log.textContent=await r.text();}catch(e){log.textContent='ERR '+e;}}"
"</script></main></body></html>\n";

struct live_http_conn {
    struct tcp_pcb *pcb;
    char request[LIVE_HTTP_REQ_MAX + 1];
    size_t request_len;
    const char *response;
    size_t response_len;
    size_t response_sent;
    char response_buf[128];
    uint8_t used;
};

static struct live_http_conn live_http_conns[LIVE_HTTP_MAX_CONNS];
static JSContext **live_http_ctxp;

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

static void live_http_reply(struct live_http_conn *conn,
                            const char *status,
                            const char *type,
                            const char *body)
{
    size_t header_len;
    size_t body_len = strlen(body);

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

static void live_http_run(struct live_http_conn *conn, const char *body, size_t len)
{
    if (len > LIVE_HTTP_SCRIPT_MAX) {
        live_http_reply(conn, "413 Payload Too Large", "text/plain",
                        "ERR script too large\n");
        return;
    }

    printf("[live] HTTP script: %u bytes\n", (unsigned)len);
    JS_FreeContext(*live_http_ctxp);
    *live_http_ctxp = new_mqjs_context();
    if (*live_http_ctxp == NULL) {
        live_http_reply(conn, "500 Internal Server Error", "text/plain",
                        "ERR JS_NewContext failed\n");
        return;
    }

    if (run_source(*live_http_ctxp, body, len, "live_http.js", JS_EVAL_REPL) == 0) {
        puts("[live] script ok");
        live_http_reply(conn, "200 OK", "text/plain", "OK\n");
    } else {
        puts("[live] script failed");
        live_http_reply(conn, "500 Internal Server Error", "text/plain",
                        "ERR script failed; see UART log\n");
    }
}

static int live_http_parse(struct live_http_conn *conn)
{
    char *body;
    int body_len;
    int header_len;
    int content_len;

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

    if (strncmp(conn->request, "POST /run ", 10) != 0 &&
        strncmp(conn->request, "POST /run? ", 11) != 0) {
        live_http_reply(conn, "404 Not Found", "text/plain", "ERR not found\n");
        return 1;
    }

    content_len = live_http_content_length(conn->request);
    body_len    = conn->request_len - header_len;
    if (body_len < content_len)
        return 0;

    live_http_run(conn, body, content_len);

    return 1;
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
    }
}

#endif /* defined(CSR_ETHMAC_BASE) && LITEX_MQJS_LIVE_HTTP */
