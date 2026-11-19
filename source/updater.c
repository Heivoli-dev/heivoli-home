#include "updater.h"
#include "config.h"
#include "sha256.h"
#include <3ds.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MANIFEST_MAX (16 * 1024)

bool updater_is_3dsx(void)
{
    return envIsHomebrew();
}

int version_compare(const char *a, const char *b)
{
    for (int i = 0; i < 3; i++) {
        long x = strtol(a, (char **)&a, 10);
        long y = strtol(b, (char **)&b, 10);
        if (x != y) return x < y ? -1 : 1;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    return 0;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, n - i + 1);
}

static void copy_field(char *dst, size_t cap, const char *src)
{
    snprintf(dst, cap, "%s", src);
}

// Format du manifeste (une clé par ligne) :
//   version=0.2.0
//   3dsx=https://.../heivoli-home.3dsx
//   sha256_3dsx=<64 caractères hex>
//   cia=https://.../heivoli-home.cia
//   sha256_cia=<64 caractères hex>
//   notes=Texte court affiché à l'utilisateur
static int parse_manifest(char *txt, UpdateInfo *info)
{
    memset(info, 0, sizeof(*info));
    char *save = NULL;
    for (char *line = strtok_r(txt, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = '\0';
        char *key = line, *val = eq + 1;
        trim(key);
        trim(val);
        if (!strcmp(key, "version")) copy_field(info->version, sizeof(info->version), val);
        else if (!strcmp(key, "3dsx")) copy_field(info->url_3dsx, sizeof(info->url_3dsx), val);
        else if (!strcmp(key, "cia")) copy_field(info->url_cia, sizeof(info->url_cia), val);
        else if (!strcmp(key, "sha256_3dsx")) copy_field(info->sha_3dsx, sizeof(info->sha_3dsx), val);
        else if (!strcmp(key, "sha256_cia")) copy_field(info->sha_cia, sizeof(info->sha_cia), val);
        else if (!strcmp(key, "notes")) copy_field(info->notes, sizeof(info->notes), val);
    }
    return info->version[0] ? 0 : -1;
}

int updater_check(const char *manifest_url, UpdateInfo *info, char *err, size_t errlen)
{
    if (!net_wifi_connected()) {
        snprintf(err, errlen, "Pas de connexion Wi-Fi.");
        return -1;
    }
    char *txt = NULL;
    if (net_get_mem(manifest_url, &txt, NULL, MANIFEST_MAX) != 0) {
        snprintf(err, errlen, "Serveur injoignable : %s", net_last_error());
        return -2;
    }
    int rc = parse_manifest(txt, info);
    free(txt);
    if (rc != 0) {
        snprintf(err, errlen, "Fichier de mise à jour invalide.");
        return -3;
    }
    return version_compare(info->version, APP_VERSION) > 0 ? 1 : 0;
}

static bool file_exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

static int verify(const char *path, const char *expected, char *err, size_t errlen)
{
    if (!expected[0]) return 0; // pas d'empreinte fournie : on fait confiance au HTTPS
    char hex[65];
    if (sha256_file_hex(path, hex) != 0) {
        snprintf(err, errlen, "Lecture du fichier téléchargé impossible.");
        return -1;
    }
    for (char *p = (char *)expected; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strcmp(hex, expected) != 0) {
        snprintf(err, errlen, "Fichier corrompu (empreinte SHA-256 différente).");
        return -1;
    }
    return 0;
}

static int install_cia(const char *path, char *err, size_t errlen)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, errlen, "Ouverture du CIA impossible.");
        return -1;
    }
    Handle cia;
    Result rc = AM_StartCiaInstall(MEDIATYPE_SD, &cia);
    if (R_FAILED(rc)) {
        fclose(f);
        snprintf(err, errlen, "AM_StartCiaInstall : 0x%08lX", (unsigned long)rc);
        return -1;
    }
    const size_t CHUNK = 128 * 1024;
    u8 *buf = malloc(CHUNK);
    if (!buf) {
        AM_CancelCIAInstall(cia);
        fclose(f);
        snprintf(err, errlen, "Mémoire insuffisante.");
        return -1;
    }
    u64 off = 0;
    size_t n;
    while ((n = fread(buf, 1, CHUNK, f)) > 0) {
        u32 written = 0;
        rc = FSFILE_Write(cia, &written, off, buf, (u32)n, 0);
        if (R_FAILED(rc) || written != n) {
            free(buf);
            fclose(f);
            AM_CancelCIAInstall(cia);
            snprintf(err, errlen, "Écriture du CIA : 0x%08lX", (unsigned long)rc);
            return -1;
        }
        off += n;
    }
    free(buf);
    fclose(f);
    rc = AM_FinishCiaInstall(cia);
    if (R_FAILED(rc)) {
        snprintf(err, errlen, "AM_FinishCiaInstall : 0x%08lX", (unsigned long)rc);
        return -1;
    }
    return 0;
}

int updater_replace_file(const char *src_path, const char *dest_path, char *err, size_t errlen)
{
    char bak[300];
    snprintf(bak, sizeof(bak), "%s.bak", dest_path);
    remove(bak);
    if (file_exists(dest_path) && rename(dest_path, bak) != 0) {
        snprintf(err, errlen, "Sauvegarde de l'ancienne version impossible.");
        return -4;
    }
    if (rename(src_path, dest_path) != 0) {
        rename(bak, dest_path); // on remet l'ancienne version
        snprintf(err, errlen, "Remplacement du fichier impossible.");
        return -4;
    }
    return 0;
}

static int download_verified(const char *url, const char *sha, const char *tmp,
                             net_progress_cb cb, void *user, char *err, size_t errlen)
{
    if (!url[0]) {
        snprintf(err, errlen, "Aucun fichier à télécharger dans la mise à jour.");
        return -1;
    }
    mkdir(HEIVOLI_ROOT, 0777);
    mkdir(TMP_DIR, 0777);
    int rc = net_download_file(url, tmp, cb, user);
    if (rc == 1) {
        snprintf(err, errlen, "Téléchargement annulé.");
        return 1;
    }
    if (rc != 0) {
        snprintf(err, errlen, "Téléchargement : %s", net_last_error());
        return -2;
    }
    char shaCopy[65];
    snprintf(shaCopy, sizeof(shaCopy), "%s", sha);
    if (verify(tmp, shaCopy, err, errlen) != 0) {
        remove(tmp);
        return -3;
    }
    return 0;
}

int updater_install_3dsx_to(const UpdateInfo *info, const char *dest_path,
                            net_progress_cb cb, void *user, char *err, size_t errlen)
{
    const char *tmp = TMP_DIR "/update.3dsx";
    int rc = download_verified(info->url_3dsx, info->sha_3dsx, tmp, cb, user, err, errlen);
    if (rc != 0) return rc;
    return updater_replace_file(tmp, dest_path, err, errlen);
}

int updater_install(const UpdateInfo *info, const char *self_path,
                    net_progress_cb cb, void *user, char *err, size_t errlen)
{
    if (updater_is_3dsx()) return updater_install_3dsx_to(info, self_path, cb, user, err, errlen);
    const char *tmp = TMP_DIR "/update.cia";
    int rc = download_verified(info->url_cia, info->sha_cia, tmp, cb, user, err, errlen);
    if (rc != 0) return rc;
    rc = install_cia(tmp, err, errlen);
    remove(tmp);
    return rc == 0 ? 0 : -5;
}

bool updater_has_backup(const char *self_path)
{
    char bak[300];
    snprintf(bak, sizeof(bak), "%s.bak", self_path);
    return file_exists(bak);
}

int updater_restore_backup(const char *self_path, char *err, size_t errlen)
{
    char bak[300], broken[300];
    snprintf(bak, sizeof(bak), "%s.bak", self_path);
    snprintf(broken, sizeof(broken), "%s.broken", self_path);
    if (!file_exists(bak)) {
        snprintf(err, errlen, "Aucune version précédente trouvée.");
        return -1;
    }
    remove(broken);
    rename(self_path, broken);
    if (rename(bak, self_path) != 0) {
        rename(broken, self_path);
        snprintf(err, errlen, "Restauration impossible.");
        return -1;
    }
    return 0;
}
