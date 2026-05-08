// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#include <generated/mem.h>
/* csr.h includes soc.h, which also defines VIDEO_FRAMEBUFFER_BASE. Keep
 * VIDEO_FRAMEBUFFER_SIZE from mem.h, then let soc.h provide the base. */
#undef VIDEO_FRAMEBUFFER_BASE
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
#define LIVE_HTTP_REQ_MAX       32768
#define LIVE_HTTP_SCRIPT_MAX    16384
#define LIVE_HTTP_LOG_MAX       2048
#define LIVE_HTTP_SLOT_COUNT    4

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
#if defined(VIDEO_FRAMEBUFFER_SIZE) && \
    defined(CSR_VIDEO_FRAMEBUFFER_DMA_BASE_ADDR) && \
    (VIDEO_FRAMEBUFFER_SIZE >= \
     (2 * VIDEO_FRAMEBUFFER_HRES * VIDEO_FRAMEBUFFER_VRES * \
      (VIDEO_FRAMEBUFFER_DEPTH / 8)))
#define LIVE_HTTP_FB_DOUBLE_BUFFERED 1
#else
#define LIVE_HTTP_FB_DOUBLE_BUFFERED 0
#endif
#else
#define LIVE_HTTP_HAS_FRAMEBUFFER 0
#define LIVE_HTTP_FB_WIDTH  0
#define LIVE_HTTP_FB_HEIGHT 0
#define LIVE_HTTP_FB_DEPTH  0
#define LIVE_HTTP_FB_DOUBLE_BUFFERED 0
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

static const char *live_http_slots[LIVE_HTTP_SLOT_COUNT] = {
    "main.js",
    "demo1.js",
    "demo2.js",
    "demo3.js",
};

#include "live_http_index.h"

struct live_http_conn {
    struct tcp_pcb *pcb;
    char request[LIVE_HTTP_REQ_MAX + 1];
    char response_body[LIVE_HTTP_LOG_MAX + 64];
    size_t request_len;
    const char *response;
    size_t response_len;
    size_t response_sent;
    char response_buf[384];
    uint8_t used;
};

static struct live_http_conn live_http_conns[LIVE_HTTP_MAX_CONNS];
static JSContext **live_http_ctxp;
static uint8_t live_http_heaps[2][LITEX_MQJS_HEAP_SIZE]
    __attribute__((aligned(16)));
static char live_http_log[LIVE_HTTP_LOG_MAX + 1];
static size_t live_http_log_len;
static int live_http_active_heap = -1;
static uint8_t live_http_active;
static uint8_t live_http_paused;
static uint32_t live_http_frame_count;
static uint32_t live_http_last_frame_ms;
static uint32_t live_http_next_frame_ms;
static uint32_t live_http_fps_x100;
static uint32_t live_http_avg_frame_ms_x100;
static uint32_t live_http_stats_start_ms;
static uint32_t live_http_stats_frames;
static uint32_t live_http_stats_time_ms;
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
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                          "Access-Control-Allow-Headers: Content-Type\r\n"
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
                              "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n"
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

static int live_http_inactive_heap(void)
{
    return live_http_active_heap == 0 ? 1 : 0;
}

static JSContext *live_http_new_candidate_context(int *heap_index)
{
    *heap_index = live_http_inactive_heap();
    return new_mqjs_context_with_heap(live_http_heaps[*heap_index],
                                      sizeof(live_http_heaps[*heap_index]));
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
        "\"depth\":%u,"
        "\"double_buffered\":%s"
        "},"
        "\"live\":{"
        "\"active\":%s,"
        "\"paused\":%s,"
        "\"state\":\"%s\","
        "\"frames\":%u,"
        "\"last_frame_ms\":%u,"
        "\"fps_x100\":%u,"
        "\"avg_frame_ms_x100\":%u"
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
        LIVE_HTTP_JSON_BOOL(LIVE_HTTP_FB_DOUBLE_BUFFERED),
        LIVE_HTTP_JSON_BOOL(live_http_active),
        LIVE_HTTP_JSON_BOOL(live_http_paused),
        live_http_state,
        (unsigned)live_http_frame_count,
        (unsigned)live_http_last_frame_ms,
        (unsigned)live_http_fps_x100,
        (unsigned)live_http_avg_frame_ms_x100);
    if (response_len < 0)
        response_len = 0;
    if (response_len >= (int)sizeof(conn->response_body))
        response_len = sizeof(conn->response_body) - 1;

    live_http_reply_len(conn, "200 OK", "application/json",
                        conn->response_body, response_len);
}

static void live_http_run(struct live_http_conn *conn, const char *body, size_t len)
{
    JSContext *candidate;
    JSContext *old_ctx;
    const char *reload_note;
    int candidate_heap;
    int has_frame;
    int response_len;

    if (len > LIVE_HTTP_SCRIPT_MAX) {
        live_http_reply(conn, "413 Payload Too Large", "text/plain",
                        "ERR script too large\n");
        return;
    }

    printf("[live] HTTP script: %u bytes\n", (unsigned)len);
    live_http_log_reset();
    candidate = live_http_new_candidate_context(&candidate_heap);
    if (candidate == NULL) {
        live_http_reply(conn, "500 Internal Server Error", "text/plain",
                        "ERR JS_NewContext failed\n");
        return;
    }
    JS_SetLogFunc(candidate, live_http_log_func);
    mqjs_set_print_func(live_http_log_func);

    if (run_source(candidate, body, len, "live_http.js", JS_EVAL_REPL) == 0) {
        if (live_http_call_function(candidate, "setup", 0, 0) < 0) {
            mqjs_set_print_func(NULL);
            JS_FreeContext(candidate);
            puts("[live] setup failed");
            reload_note = live_http_active ? "previous script kept" :
                           "live script unchanged";
            response_len = snprintf(conn->response_body,
                                    sizeof(conn->response_body),
                                    "ERR setup failed; %s\n%s", reload_note,
                                    live_http_log);
            if (response_len < 0)
                response_len = 0;
            if (response_len >= (int)sizeof(conn->response_body))
                response_len = sizeof(conn->response_body) - 1;
            live_http_reply_len(conn, "500 Internal Server Error", "text/plain",
                                conn->response_body, response_len);
            return;
        }
        has_frame = live_http_has_function(candidate, "frame");
        old_ctx = *live_http_ctxp;
        *live_http_ctxp = candidate;
        live_http_active_heap = candidate_heap;
        if (old_ctx != NULL)
            JS_FreeContext(old_ctx);

        if (has_frame) {
            live_http_active        = 1;
            live_http_paused        = 0;
            live_http_frame_count   = 0;
            live_http_last_frame_ms = 0;
            live_http_fps_x100      = 0;
            live_http_avg_frame_ms_x100 = 0;
            live_http_stats_start_ms = live_http_millis();
            live_http_stats_frames   = 0;
            live_http_stats_time_ms  = 0;
            live_http_next_frame_ms = live_http_millis();
            live_http_set_state("running");
        } else {
            live_http_stop_script("idle");
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
        JS_FreeContext(candidate);
        puts("[live] script failed");
        reload_note = live_http_active ? "previous script kept" :
                       "live script unchanged";
        response_len = snprintf(conn->response_body, sizeof(conn->response_body),
                                "ERR script failed; %s\n%s", reload_note,
                                live_http_log);
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

static int live_http_request_is(const char *request,
                                const char *method,
                                const char *path)
{
    size_t method_len = strlen(method);
    size_t path_len   = strlen(path);

    if (strncmp(request, method, method_len) != 0 || request[method_len] != ' ')
        return 0;

    request += method_len + 1;
    if (strncmp(request, path, path_len) != 0)
        return 0;

    return request[path_len] == ' ' || request[path_len] == '?';
}

static void live_http_reset_context(void)
{
    JS_FreeContext(*live_http_ctxp);
    *live_http_ctxp = new_mqjs_context();
    live_http_active_heap = -1;
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

static const char *live_http_slot_path(const char *request)
{
    const char *name = strstr(request, "?name=");

    if (name == NULL)
        return live_http_slots[0];
    name += strlen("?name=");

    for (int i = 0; i < LIVE_HTTP_SLOT_COUNT; i++) {
        size_t len = strlen(live_http_slots[i]);

        if (strncmp(name, live_http_slots[i], len) == 0 &&
            (name[len] == ' ' || name[len] == '&')) {
            return live_http_slots[i];
        }
    }

    return NULL;
}

static void live_http_load_slot(struct live_http_conn *conn,
                                const char *request)
{
    const char *data;
    const char *path = live_http_slot_path(request);
    size_t len;
    char err[96];

    if (path == NULL) {
        live_http_reply(conn, "400 Bad Request", "text/plain",
                        "ERR unknown slot\n");
        return;
    }

    if (mqjs_fs_read_file(path, &data, &len, err, sizeof(err)) != 0) {
        snprintf(conn->response_body, sizeof(conn->response_body),
                 "ERR %s\n", err);
        live_http_reply(conn, "500 Internal Server Error", "text/plain",
                        conn->response_body);
        return;
    }
    if (len > LIVE_HTTP_SCRIPT_MAX) {
        live_http_reply(conn, "413 Payload Too Large", "text/plain",
                        "ERR script slot too large\n");
        return;
    }

    live_http_reply_len(conn, "200 OK", "text/plain", data, len);
}

static void live_http_save_slot(struct live_http_conn *conn,
                                const char *request,
                                const char *body, size_t len)
{
    const char *path = live_http_slot_path(request);
    char err[96];

    if (path == NULL) {
        live_http_reply(conn, "400 Bad Request", "text/plain",
                        "ERR unknown slot\n");
        return;
    }
    if (len > LIVE_HTTP_SCRIPT_MAX) {
        live_http_reply(conn, "413 Payload Too Large", "text/plain",
                        "ERR script too large\n");
        return;
    }
    if (mqjs_fs_write_file(path, body, len, err, sizeof(err)) != 0) {
        snprintf(conn->response_body, sizeof(conn->response_body),
                 "ERR %s\n", err);
        live_http_reply(conn, "500 Internal Server Error", "text/plain",
                        conn->response_body);
        return;
    }

    snprintf(conn->response_body, sizeof(conn->response_body),
             "OK saved %s\n", path);
    live_http_reply(conn, "200 OK", "text/plain", conn->response_body);
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
    int is_save;

    conn->request[conn->request_len] = 0;
    body = strstr(conn->request, "\r\n\r\n");
    if (body == NULL)
        return 0;

    body      += 4;
    header_len = body - conn->request;

    if (strncmp(conn->request, "OPTIONS ", 8) == 0) {
        live_http_reply(conn, "200 OK", "text/plain", "OK\n");
        return 1;
    }

    if (live_http_request_is(conn->request, "GET", "/")) {
        live_http_reply_len(conn, "200 OK", "text/html",
                            live_http_index, live_http_index_len);
        return 1;
    }

    if (live_http_request_is(conn->request, "GET", "/info")) {
        live_http_info(conn);
        return 1;
    }

    if (live_http_request_is(conn->request, "GET", "/load")) {
        live_http_load_slot(conn, conn->request);
        return 1;
    }

    is_run     = live_http_request_is(conn->request, "POST", "/run");
    is_eval    = live_http_request_is(conn->request, "POST", "/eval");
    is_control = live_http_request_is(conn->request, "POST", "/control");
    is_save    = live_http_request_is(conn->request, "POST", "/save");
    if (!is_run && !is_eval && !is_control && !is_save) {
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
    else if (is_save)
        live_http_save_slot(conn, conn->request, body, content_len);
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
    live_http_stats_frames++;
    live_http_stats_time_ms += elapsed;
    now = live_http_millis();
    if ((now - live_http_stats_start_ms) >= 1000) {
        uint32_t window_ms = now - live_http_stats_start_ms;

        if (window_ms != 0 && live_http_stats_frames != 0) {
            live_http_fps_x100 = (live_http_stats_frames * 100000) /
                                 window_ms;
            live_http_avg_frame_ms_x100 = (live_http_stats_time_ms * 100) /
                                          live_http_stats_frames;
        }
        live_http_stats_start_ms = now;
        live_http_stats_frames   = 0;
        live_http_stats_time_ms  = 0;
    }
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
