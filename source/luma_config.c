#include "luma_config.h"
#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define BACKUP_DIR  HEIVOLI_ROOT "/backup"
#define BACKUP_PATH BACKUP_DIR "/luma-config.ini"

bool luma_config_present(void)
{
    struct stat st;
    return stat(LUMA_CONFIG_PATH, &st) == 0;
}

static char *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 256 * 1024) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (len) *len = got;
    return buf;
}

// Repère la ligne "clé = valeur" (hors commentaires). Renvoie le début de la valeur
// et sa longueur, ou NULL.
static char *find_value(char *txt, const char *key, size_t *vlen)
{
    size_t klen = strlen(key);
    char *line = txt;
    while (line && *line) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != ';' && *p != '#' && !strncmp(p, key, klen)) {
            char *q = p + klen;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '=') {
                q++;
                while (*q == ' ' || *q == '\t') q++;
                char *e = q;
                while (*e && *e != '\n' && *e != '\r') e++;
                while (e > q && isspace((unsigned char)e[-1])) e--;
                *vlen = (size_t)(e - q);
                return q;
            }
        }
        line = strchr(line, '\n');
        if (line) line++;
    }
    return NULL;
}

int luma_config_get(const char *key, char *out, size_t cap)
{
    char *txt = read_all(LUMA_CONFIG_PATH, NULL);
    if (!txt) return -1;
    size_t vlen = 0;
    char *v = find_value(txt, key, &vlen);
    int rc = -1;
    if (v) {
        if (vlen >= cap) vlen = cap - 1;
        memcpy(out, v, vlen);
        out[vlen] = '\0';
        rc = 0;
    }
    free(txt);
    return rc;
}

static void backup_once(const char *txt, size_t len)
{
    struct stat st;
    if (stat(BACKUP_PATH, &st) == 0) return;
    mkdir(HEIVOLI_ROOT, 0777);
    mkdir(BACKUP_DIR, 0777);
    FILE *f = fopen(BACKUP_PATH, "wb");
    if (!f) return;
    fwrite(txt, 1, len, f);
    fclose(f);
}

int luma_config_set(const char *key, const char *value)
{
    size_t len = 0;
    char *txt = read_all(LUMA_CONFIG_PATH, &len);
    if (!txt) return -2;
    size_t vlen = 0;
    char *v = find_value(txt, key, &vlen);
    if (!v) {
        free(txt);
        return -1;
    }
    backup_once(txt, len);

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s.tmp", LUMA_CONFIG_PATH);
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        free(txt);
        return -2;
    }
    size_t before = (size_t)(v - txt);
    bool ok = fwrite(txt, 1, before, f) == before;
    ok = ok && fputs(value, f) >= 0;
    size_t afterOff = before + vlen;
    ok = ok && fwrite(txt + afterOff, 1, len - afterOff, f) == len - afterOff;
    fclose(f);
    free(txt);
    if (!ok) {
        remove(tmp);
        return -2;
    }
    remove(LUMA_CONFIG_PATH);
    return rename(tmp, LUMA_CONFIG_PATH) == 0 ? 0 : -2;
}

bool luma_autoboot_enabled(void)
{
    char v[16];
    if (luma_config_get("autoboot_mode", v, sizeof(v)) != 0) return false;
    return !strcmp(v, "3ds");
}

int luma_set_autoboot(bool on)
{
    return luma_config_set("autoboot_mode", on ? "3ds" : "off");
}

void system_reboot(void)
{
    // Redémarrage complet par le contrôleur d'alimentation (comme un vrai rallumage) :
    // c'est ce qui fait relire à Luma son réglage de démarrage.
    // Le redémarrage "logiciel" d'APT ne suffit pas (testé sur 2DS : il revient au menu HOME).
    if (R_SUCCEEDED(ptmSysmInit())) {
        if (R_SUCCEEDED(PTMSYSM_RebootAsync(0))) {
            for (;;) svcSleepThread(100000000LL);
        }
        ptmSysmExit();
    }
    if (R_SUCCEEDED(APT_HardwareResetAsync())) {
        for (;;) svcSleepThread(100000000LL);
    }
}
