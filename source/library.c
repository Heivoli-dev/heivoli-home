#include "library.h"
#include "config.h"
#include "ui.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

// --- Format SMDH (icône + titre des jeux 3DS et des homebrews) -------------
typedef struct {
    u16 shortDescription[0x40];
    u16 longDescription[0x80];
    u16 publisher[0x40];
} SmdhTitle;

typedef struct {
    u32 magic;
    u16 version;
    u16 reserved;
    SmdhTitle titles[16];
    u8 settings[0x30];
    u8 reserved2[0x8];
    u16 smallIcon[0x240];
    u16 bigIcon[0x900];
} Smdh;

static Section s_sections[SECTION_COUNT] = {
    {"3DS",  "Jeux 3DS",      NULL,   "",          KIND_3DS_TITLE, 0, 0},
    {"HB",   "Homebrews",     NULL,   ".3dsx",     KIND_HOMEBREW,  0, 0},
    {"DS",   "Nintendo DS",   "nds",  ".nds",      KIND_ROM,       0, 0},
    {"GBA",  "Game Boy Advance", "gba", ".gba|.zip", KIND_ROM,     0, 0},
    {"GBC",  "Game Boy Color", "gbc", ".gbc|.zip", KIND_ROM,       0, 0},
    {"GB",   "Game Boy",      "gb",   ".gb|.zip",  KIND_ROM,       0, 0},
    {"SNES", "Super Nintendo", "snes", ".sfc|.smc|.zip", KIND_ROM, 0, 0},
    {"NES",  "NES",           "nes",  ".nes|.zip", KIND_ROM,       0, 0},
};

static void init_colors(void)
{
    static bool done = false;
    if (done) return;
    const u32 c[SECTION_COUNT][2] = {
        {C_SKY, COL(0x3F, 0x7F, 0xC4, 0xFF)},
        {C_MINT, COL(0x2E, 0x8F, 0x6A, 0xFF)},
        {C_PEACH, COL(0xC8, 0x6A, 0x3A, 0xFF)},
        {C_LAV, COL(0x6A, 0x5A, 0xC0, 0xFF)},
        {COL(0xFF, 0xF2, 0xCC, 0xFF), COL(0xA8, 0x80, 0x20, 0xFF)},
        {COL(0xE2, 0xF2, 0xD6, 0xFF), COL(0x55, 0x8A, 0x3A, 0xFF)},
        {COL(0xFC, 0xE1, 0xEC, 0xFF), COL(0xB0, 0x4A, 0x78, 0xFF)},
        {COL(0xFD, 0xE0, 0xE0, 0xFF), COL(0xB0, 0x40, 0x48, 0xFF)},
    };
    for (int i = 0; i < SECTION_COUNT; i++) {
        s_sections[i].c1 = c[i][0];
        s_sections[i].c2 = c[i][1];
    }
    done = true;
}

Section *library_sections(void)
{
    init_colors();
    return s_sections;
}

void library_prepare_folders(void)
{
    mkdir(HEIVOLI_ROOT, 0777);
    mkdir(ROMS_ROOT, 0777);
    mkdir(TMP_DIR, 0777);
    char p[128];
    for (int i = 0; i < SECTION_COUNT; i++) {
        if (!s_sections[i].folder) continue;
        snprintf(p, sizeof(p), "%s/%s", ROMS_ROOT, s_sections[i].folder);
        mkdir(p, 0777);
    }
}

static Entry *push(Section *s)
{
    if (s->count == s->cap) {
        int nc = s->cap ? s->cap * 2 : 32;
        Entry *n = realloc(s->items, sizeof(Entry) * nc);
        if (!n) return NULL;
        s->items = n;
        s->cap = nc;
    }
    Entry *e = &s->items[s->count++];
    memset(e, 0, sizeof(*e));
    return e;
}

static bool has_ext(const char *name, const char *exts)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", exts);
    char *save = NULL;
    for (char *t = strtok_r(buf, "|", &save); t; t = strtok_r(NULL, "|", &save))
        if (!strcasecmp(dot, t)) return true;
    return false;
}

static void name_from_file(char *dst, size_t cap, const char *file)
{
    snprintf(dst, cap, "%s", file);
    char *dot = strrchr(dst, '.');
    if (dot) *dot = '\0';
}

static int cmp_entries(const void *a, const void *b)
{
    return strcasecmp(((const Entry *)a)->name, ((const Entry *)b)->name);
}

// Convertit la grande icône SMDH (48x48, RGB565, déjà en tuiles 8x8) en texture 64x64.
static void load_icon(Entry *e, const Smdh *smdh)
{
    if (!C3D_TexInit(&e->iconTex, 64, 64, GPU_RGB565)) return;
    memset(e->iconTex.data, 0, e->iconTex.size);
    const u16 *src = smdh->bigIcon;
    u16 *dst = (u16 *)e->iconTex.data;
    for (int row = 0; row < 48; row += 8) {
        memcpy(dst, src, 48 * 8 * sizeof(u16));
        src += 48 * 8;
        dst += 64 * 8;
    }
    C3D_TexFlush(&e->iconTex);
    C3D_TexSetFilter(&e->iconTex, GPU_LINEAR, GPU_LINEAR);
    e->iconSub.width = 48;
    e->iconSub.height = 48;
    e->iconSub.left = 0.0f;
    e->iconSub.top = 1.0f;
    e->iconSub.right = 48 / 64.0f;
    e->iconSub.bottom = 1.0f - 48 / 64.0f;
    e->icon.tex = &e->iconTex;
    e->icon.subtex = &e->iconSub;
    e->hasIcon = true;
}

static void title_from_smdh(char *dst, size_t cap, const Smdh *smdh)
{
    u8 lang = CFG_LANGUAGE_FR;
    CFGU_GetSystemLanguage(&lang);
    const u16 *name = smdh->titles[lang < 16 ? lang : 1].shortDescription;
    if (!name[0]) name = smdh->titles[1].shortDescription; // anglais
    u8 tmp[0x40 * 3 + 1] = {0};
    ssize_t n = utf16_to_utf8(tmp, name, sizeof(tmp) - 1);
    if (n < 0) n = 0;
    tmp[n] = 0;
    snprintf(dst, cap, "%s", (char *)tmp);
}

static bool read_title_smdh(u64 tid, u8 media, Smdh *out)
{
    static const u32 filePath[5] = {0x0, 0x0, 0x2, 0x6E6F6369, 0x0}; // "icon"
    u32 archPath[4] = {(u32)(tid & 0xFFFFFFFF), (u32)(tid >> 32), media, 0x0};
    FS_Path a = {PATH_BINARY, sizeof(archPath), archPath};
    FS_Path f = {PATH_BINARY, sizeof(filePath), filePath};
    Handle h;
    if (R_FAILED(FSUSER_OpenFileDirectly(&h, ARCHIVE_SAVEDATA_AND_CONTENT, a, f, FS_OPEN_READ, 0))) return false;
    u32 read = 0;
    Result rc = FSFILE_Read(h, &read, 0, out, sizeof(Smdh));
    FSFILE_Close(h);
    return R_SUCCEEDED(rc) && read == sizeof(Smdh) && out->magic == 0x48444D53; // "SMDH"
}

#define OVERRIDES_PATH HEIVOLI_ROOT "/sections.ini"

// Corrections manuelles (touche X dans la bibliothèque) : "<titre en hex>=hb" ou "=game".
static int override_for(u64 tid)
{
    FILE *f = fopen(OVERRIDES_PATH, "r");
    if (!f) return 0;
    char line[64], want[24];
    snprintf(want, sizeof(want), "%016llX=", (unsigned long long)tid);
    int r = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strncasecmp(line, want, 17)) {
            r = !strncmp(line + 17, "hb", 2) ? 1 : (!strncmp(line + 17, "game", 4) ? -1 : 0);
        }
    }
    fclose(f);
    return r; // 1 = homebrew, -1 = jeu, 0 = automatique
}

// Devine si un titre installé est un homebrew (CIA) plutôt qu'un jeu officiel.
static bool looks_homebrew(u64 tid, u8 media)
{
    int o = override_for(tid);
    if (o) return o > 0;
    if (media == MEDIATYPE_GAME_CARD) return false;   // une cartouche est un vrai jeu
    u32 uid = (u32)(tid >> 8) & 0xFFFFF;
    if (uid >= 0xF0000) return true;                   // plage d'identifiants des homebrews
    char pc[16] = {0};
    if (R_FAILED(AM_GetTitleProductCode((FS_MediaType)media, tid, pc))) return false;
    if (!strncmp(pc, "CTR-P-CTAP", 10)) return true;  // code par défaut des outils homebrew
    if (!strncmp(pc, "CTR-H", 5)) return true;
    // Jeux officiels : CTR-P-XXXX, CTR-N-XXXX, KTR-... (4 lettres/chiffres majuscules)
    bool official = strlen(pc) == 10 && (!strncmp(pc, "CTR-", 4) || !strncmp(pc, "KTR-", 4)) &&
                    strchr("PNUM", pc[4]) && pc[5] == '-';
    for (int i = 6; official && i < 10; i++)
        if (!(isupper((unsigned char)pc[i]) || isdigit((unsigned char)pc[i]))) official = false;
    return !official;
}

bool library_toggle_title_section(u64 tid, bool toHomebrew)
{
    mkdir(HEIVOLI_ROOT, 0777);
    FILE *f = fopen(OVERRIDES_PATH, "a");
    if (!f) return false;
    fprintf(f, "%016llX=%s\n", (unsigned long long)tid, toHomebrew ? "hb" : "game");
    fclose(f);
    return true;
}

// Range les titres installés : jeux officiels dans `games`, homebrews CIA dans `hb`.
static void scan_titles(Section *games, Section *hb)
{
    static const FS_MediaType medias[2] = {MEDIATYPE_GAME_CARD, MEDIATYPE_SD};
    Smdh *smdh = malloc(sizeof(Smdh));
    if (!smdh) return;
    for (int m = 0; m < 2; m++) {
        u32 count = 0;
        if (R_FAILED(AM_GetTitleCount(medias[m], &count)) || count == 0) continue;
        u64 *ids = malloc(sizeof(u64) * count);
        if (!ids) continue;
        u32 got = 0;
        if (R_SUCCEEDED(AM_GetTitleList(&got, medias[m], count, ids))) {
            for (u32 i = 0; i < got; i++) {
                if ((ids[i] >> 32) != 0x00040000) continue; // applications uniquement
                Section *s = looks_homebrew(ids[i], (u8)medias[m]) ? hb : games;
                Entry *e = push(s);
                if (!e) break;
                e->titleId = ids[i];
                e->mediatype = (u8)medias[m];
                if (read_title_smdh(ids[i], (u8)medias[m], smdh)) {
                    title_from_smdh(e->name, sizeof(e->name), smdh);
                    load_icon(e, smdh);
                }
                if (!e->name[0]) snprintf(e->name, sizeof(e->name), "%016llX", (unsigned long long)ids[i]);
            }
        }
        free(ids);
    }
    free(smdh);
}

// Lit le SMDH intégré à un .3dsx (en-tête étendu).
static bool read_3dsx_smdh(const char *path, Smdh *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    u8 hdr[0x2C];
    bool ok = false;
    if (fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr) && !memcmp(hdr, "3DSX", 4)) {
        u16 hsize = hdr[4] | (hdr[5] << 8);
        if (hsize > 0x20) {
            u32 off = hdr[0x20] | (hdr[0x21] << 8) | (hdr[0x22] << 16) | ((u32)hdr[0x23] << 24);
            u32 size = hdr[0x24] | (hdr[0x25] << 8) | (hdr[0x26] << 16) | ((u32)hdr[0x27] << 24);
            if (size >= sizeof(Smdh) && fseek(f, off, SEEK_SET) == 0 &&
                fread(out, 1, sizeof(Smdh), f) == sizeof(Smdh) && out->magic == 0x48444D53)
                ok = true;
        }
    }
    fclose(f);
    return ok;
}

static void add_homebrew(Section *s, const char *path, const char *file, Smdh *smdh)
{
    if (strstr(file, "heivoli-home")) return; // on ne se liste pas soi-même
    Entry *e = push(s);
    if (!e) return;
    snprintf(e->path, sizeof(e->path), "%s", path);
    if (read_3dsx_smdh(path, smdh)) {
        title_from_smdh(e->name, sizeof(e->name), smdh);
        load_icon(e, smdh);
    }
    if (!e->name[0]) name_from_file(e->name, sizeof(e->name), file);
}

static void scan_homebrew(Section *s)
{
    Smdh *smdh = malloc(sizeof(Smdh));
    if (!smdh) return;
    DIR *d = opendir("sdmc:/3ds");
    if (d) {
        struct dirent *de;
        char p[300];
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            snprintf(p, sizeof(p), "sdmc:/3ds/%s", de->d_name);
            if (de->d_type == DT_DIR) {
                // Convention : sdmc:/3ds/<nom>/<nom>.3dsx
                char inner[300];
                snprintf(inner, sizeof(inner), "%s/%s.3dsx", p, de->d_name);
                struct stat st;
                if (stat(inner, &st) == 0) add_homebrew(s, inner, de->d_name, smdh);
            } else if (has_ext(de->d_name, ".3dsx")) {
                add_homebrew(s, p, de->d_name, smdh);
            }
        }
        closedir(d);
    }
    free(smdh);
}

static void scan_roms(Section *s)
{
    char dir[128];
    snprintf(dir, sizeof(dir), "%s/%s", ROMS_ROOT, s->folder);
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.' || de->d_type == DT_DIR) continue;
        if (!has_ext(de->d_name, s->exts)) continue;
        Entry *e = push(s);
        if (!e) break;
        snprintf(e->path, sizeof(e->path), "%s/%s", dir, de->d_name);
        name_from_file(e->name, sizeof(e->name), de->d_name);
    }
    closedir(d);
}

static void free_section(Section *s)
{
    for (int i = 0; i < s->count; i++)
        if (s->items[i].hasIcon) C3D_TexDelete(&s->items[i].iconTex);
    free(s->items);
    s->items = NULL;
    s->count = s->cap = 0;
    s->sel = s->scroll = 0;
    s->scanned = false;
}

static void finish_section(Section *s)
{
    // Le tri déplace les entrées : on recâble les pointeurs d'icônes ensuite.
    if (s->count > 1) qsort(s->items, s->count, sizeof(Entry), cmp_entries);
    for (int i = 0; i < s->count; i++) {
        Entry *e = &s->items[i];
        if (e->hasIcon) {
            e->icon.tex = &e->iconTex;
            e->icon.subtex = &e->iconSub;
        }
    }
    s->scanned = true;
}

void library_scan(Section *s)
{
    if (s->kind == KIND_ROM) {
        free_section(s);
        scan_roms(s);
        finish_section(s);
        return;
    }
    // Jeux 3DS et Homebrews se partagent la liste des titres installés : on les fait ensemble.
    Section *games = &s_sections[0], *hb = &s_sections[1];
    free_section(games);
    free_section(hb);
    scan_titles(games, hb);
    scan_homebrew(hb);
    finish_section(games);
    finish_section(hb);
}

void library_free(void)
{
    for (int i = 0; i < SECTION_COUNT; i++) free_section(&s_sections[i]);
}

const Entry *library_find_title(const char *needle)
{
    if (!s_sections[0].scanned) library_scan(&s_sections[0]);
    size_t nl = strlen(needle);
    for (int k = 0; k < 2; k++) {
        Section *s = &s_sections[k];
        for (int i = 0; i < s->count; i++) {
            if (!s->items[i].titleId) continue;
            for (const char *p = s->items[i].name; *p; p++)
                if (!strncasecmp(p, needle, nl)) return &s->items[i];
        }
    }
    return NULL;
}

const Entry *library_find_tid(u64 tid)
{
    if (!s_sections[0].scanned) library_scan(&s_sections[0]);
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < s_sections[k].count; i++)
            if (s_sections[k].items[i].titleId == tid) return &s_sections[k].items[i];
    return NULL;
}
