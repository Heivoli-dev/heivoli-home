#include "net.h"
#include "config.h"
#include <curl/curl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

static u32 *s_socBuf = NULL;
static bool s_ready = false;
static char s_err[128] = "";

const char *net_last_error(void) { return s_err; }

bool net_init(void)
{
    if (s_ready) return true;
    s_socBuf = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (!s_socBuf) {
        snprintf(s_err, sizeof(s_err), "Mémoire insuffisante pour le réseau");
        return false;
    }
    Result rc = socInit(s_socBuf, SOC_BUFFERSIZE);
    if (R_FAILED(rc)) {
        snprintf(s_err, sizeof(s_err), "socInit a échoué (0x%08lX)", (unsigned long)rc);
        free(s_socBuf);
        s_socBuf = NULL;
        return false;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    s_ready = true;
    return true;
}

void net_exit(void)
{
    if (!s_ready) return;
    curl_global_cleanup();
    socExit();
    free(s_socBuf);
    s_socBuf = NULL;
    s_ready = false;
}

bool net_wifi_connected(void)
{
    u32 status = 0;
    if (R_FAILED(acInit())) return false;
    Result rc = ACU_GetWifiStatus(&status);
    acExit();
    return R_SUCCEEDED(rc) && status != 0;
}

static void setup_common(CURL *c, const char *url)
{
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_CAINFO, CA_BUNDLE);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(c, CURLOPT_BUFFERSIZE, 128 * 1024L);
}

typedef struct {
    char *data;
    size_t len, cap, max;
} MemBuf;

static size_t mem_write(void *ptr, size_t size, size_t nmemb, void *user)
{
    MemBuf *b = (MemBuf *)user;
    size_t n = size * nmemb;
    if (b->len + n + 1 > b->max) return 0; // trop gros : on coupe
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 4096;
        while (nc < b->len + n + 1) nc *= 2;
        char *nd = realloc(b->data, nc);
        if (!nd) return 0;
        b->data = nd;
        b->cap = nc;
    }
    memcpy(b->data + b->len, ptr, n);
    b->len += n;
    b->data[b->len] = '\0';
    return n;
}

int net_get_mem(const char *url, char **out, size_t *outlen, size_t maxlen)
{
    *out = NULL;
    if (outlen) *outlen = 0;
    if (!net_init()) return -1;
    CURL *c = curl_easy_init();
    if (!c) {
        snprintf(s_err, sizeof(s_err), "curl_easy_init a échoué");
        return -1;
    }
    MemBuf b = {0};
    b.max = maxlen;
    setup_common(c, url);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, mem_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) {
        snprintf(s_err, sizeof(s_err), "%s", curl_easy_strerror(rc));
        free(b.data);
        return -2;
    }
    if (!b.data) { // réponse vide
        b.data = calloc(1, 1);
        if (!b.data) return -1;
    }
    *out = b.data;
    if (outlen) *outlen = b.len;
    return 0;
}

typedef struct {
    net_progress_cb cb;
    void *user;
} ProgCtx;

static int xfer_info(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    (void)ultotal;
    (void)ulnow;
    ProgCtx *ctx = (ProgCtx *)p;
    if (ctx->cb && !ctx->cb((u64)dlnow, (u64)dltotal, ctx->user)) return 1; // annule
    return 0;
}

int net_download_file(const char *url, const char *path, net_progress_cb cb, void *user)
{
    if (!net_init()) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(s_err, sizeof(s_err), "Impossible d'écrire %s", path);
        return -1;
    }
    CURL *c = curl_easy_init();
    if (!c) {
        fclose(f);
        snprintf(s_err, sizeof(s_err), "curl_easy_init a échoué");
        return -1;
    }
    ProgCtx ctx = {cb, user};
    setup_common(c, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, NULL); // fwrite par défaut
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xfer_info);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &ctx);
    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    fclose(f);
    if (rc == CURLE_ABORTED_BY_CALLBACK) {
        remove(path);
        snprintf(s_err, sizeof(s_err), "Téléchargement annulé");
        return 1;
    }
    if (rc != CURLE_OK) {
        remove(path);
        snprintf(s_err, sizeof(s_err), "%s", curl_easy_strerror(rc));
        return -2;
    }
    return 0;
}
