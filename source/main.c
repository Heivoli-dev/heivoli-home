// Heivoli Home — v0.2
// Menu 3DS de Heivoli Network : accueil en cartes + dock, bibliothèque par console,
// réglages, Communauté (Miiverse/Juxt), mise à jour Wi-Fi et mode secours.
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "launcher.h"
#include "library.h"
#include "luma_config.h"
#include "net.h"
#include "settings.h"
#include "sprites.h"
#include "ui.h"
#include "updater.h"

// ---------------------------------------------------------------------------
typedef enum { SCR_HOME, SCR_LIBRARY, SCR_SETTINGS, SCR_UPDATE, SCR_RECOVERY } Screen;

typedef enum {
    UPD_IDLE, UPD_CHECKING, UPD_UPTODATE, UPD_AVAILABLE,
    UPD_DOWNLOADING, UPD_DONE, UPD_ERROR
} UpdState;

typedef struct {
    const char *name, *shortName, *kind, *desc, *cta;
    size_t icon;
    u32 tint;
} Item;

// Cartes (0-2) puis dock (3-8)
enum { H_RESUME, H_LIBRARY, H_COMMUNITY, H_GALLERY, H_FRIENDS, H_ONLINE, H_SHOP, H_MUSIC, H_SETTINGS, H_COUNT };

// Boutons de l'en-tête de l'écran du bas
enum { HB_PROFILE, HB_LUMA, HB_CAPTURE, HB_BELL, HB_COUNT };
#define CARD_COUNT 3

static Item s_items[H_COUNT];

static C3D_RenderTarget *s_top, *s_bot;
static C2D_SpriteSheet s_sheet;
static bool s_hasSheet = false;

static Screen s_screen = SCR_HOME;
static int s_homeSel = 0;
static int s_tab = 0;
static int s_setSel = 0, s_setScroll = 0;
static bool s_quit = false;

static char s_selfPath[256] = DEFAULT_3DSX_PATH;
static const Entry *s_resume = NULL; // dernier jeu lancé, s'il est encore installé

static UpdState s_upd = UPD_IDLE;
static UpdateInfo s_updInfo;
static bool s_updAvailable = false;
static bool s_updForce = false;
static char s_updMsg[160] = "";
static float s_updFrac = 0;
static u64 s_lastProgressDraw = 0;

static int s_recSel = 0;
static u64 s_lumaConfirmUntil = 0; // double appui sur "Luma"

static char s_toast[160] = "";
static u64 s_toastUntil = 0;

static void toast(const char *msg)
{
    snprintf(s_toast, sizeof(s_toast), "%s", msg);
    s_toastUntil = osGetTime() + 2800;
}

static const char *manifest_url(void)
{
    return g_settings.beta ? UPDATE_MANIFEST_BETA : UPDATE_MANIFEST_STABLE;
}

static void save_settings(void)
{
    if (settings_save(SETTINGS_PATH, &g_settings) != 0) toast("Impossible d'enregistrer les réglages.");
}

static void init_items(void)
{
    const Item it[H_COUNT] = {
        {"Reprendre", "Reprendre", "REPRENDRE", "Relance ta dernière partie en un geste.", "Jouer", sprites_ic_pad_idx, C_SKY},
        {"Bibliothèque", "Jeux", "JEUX", "Jeux 3DS, homebrews et jeux rétro, rangés par console.", "Ouvrir", sprites_ic_pad_idx, C_LAV},
        {"Communauté", "Juxt", "JUXT", "Ouvre Miiverse, redirigé vers Juxt par Pretendo.", "Ouvrir", sprites_ic_chat_idx, C_MINT},
        {"Galerie", "Galerie", "APPLI", "Tes captures d'écran Luma, prêtes à partager.", "Ouvrir", sprites_ic_photo_idx, C_PEACH},
        {"Amis", "Amis", "SOCIAL", "Codes ami Heivoli et amis en ligne.", "Ouvrir", sprites_ic_users_idx, C_PEACH},
        {"En ligne", "En ligne", "PRETENDO", "Ouvre Nimbus pour choisir entre Pretendo et Nintendo.", "Ouvrir", sprites_ic_globe_idx, C_SKY},
        {"Boutique Heivoli", "Boutique", "SERVICE HEIVOLI", "Thèmes et contenus du réseau Heivoli.", "Ouvrir", sprites_ic_shop_idx, C_LAV},
        {"Musique", "Musique", "APPLI", "Écoute tes musiques pendant que tu navigues.", "Ouvrir", sprites_ic_music_idx, C_MINT},
        {"Réglages", "Réglages", "SYSTÈME", "Thème, démarrage, mises à jour et compte Heivoli.", "Ouvrir", sprites_ic_gear_idx, C_SKY},
    };
    memcpy(s_items, it, sizeof(it));
}

// ---------------------------------------------------------------------------
// Éléments communs

static void draw_loading(const char *msg);

static void draw_status_bar(const char *where)
{
    ui_bar(0, 212, TOP_W, 28);
    float x = 10;
    if (s_hasSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(s_sheet, sprites_logo_idx), x, 214, 0.8f, NULL, 24.0f / 64.0f, 24.0f / 64.0f);
        x += 28;
    }
    ui_text(x, 217, 0.52f, C_INK, 0, "heivoli");
    if (where) ui_text(x + ui_text_width(0.52f, "heivoli") + 6, 218, 0.45f, C_INK_MUTED, 0, where);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char clock[8];
    snprintf(clock, sizeof(clock), "%02d:%02d", tm->tm_hour, tm->tm_min);
    static const char *days[7] = {"dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam."};
    static const char *months[12] = {"janv.", "févr.", "mars", "avr.", "mai", "juin", "juil.", "août", "sept.", "oct.", "nov.", "déc."};
    char date[32];
    snprintf(date, sizeof(date), "%s %d %s", days[tm->tm_wday % 7], tm->tm_mday, months[tm->tm_mon % 12]);
    ui_text(312 - ui_text_width(0.45f, date) - 10, 218, 0.45f, C_INK_SOFT, 0, date);
    ui_text(318, 216, 0.56f, C_INK, 0, clock);

    u8 lvl = 5;
    PTMU_GetBatteryLevel(&lvl);
    if (lvl > 5) lvl = 5;
    C2D_DrawRectSolid(358, 219, 0.8f, 24, 12, C_INK_SOFT);
    C2D_DrawRectSolid(359.5f, 220.5f, 0.85f, 21, 9, C_WHITE);
    C2D_DrawRectSolid(382, 222, 0.8f, 2, 6, C_INK_SOFT);
    C2D_DrawRectSolid(361, 222, 0.9f, 18.0f * lvl / 5.0f, 6, lvl <= 1 ? COL(0xE5, 0x48, 0x4D, 0xFF) : C_INK_SOFT);
}

static void draw_toast(void)
{
    if (!s_toast[0] || osGetTime() > s_toastUntil) return;
    ui_card(10, 190, 300, 42, 12, false);
    ui_text_wrap(20, 196, 0.46f, C_INK, 280, s_toast);
}

static void draw_hint(const char *txt)
{
    ui_text(10, 224, 0.4f, C_INK_MUTED, 0, txt);
}

// Grande illustration de l'écran du haut : icône du jeu ou icône au trait dans un rond blanc.
static void draw_hero(float x, float y, float size, u32 tint, size_t icon, const Entry *game)
{
    ui_rrect(x - 1, y - 1, size + 2, size + 2, 19, C_WHITE);
    ui_rrect(x, y, size, size, 18, tint);
    if (game && game->hasIcon) {
        float s = size * 0.62f;
        ui_rrect(x + (size - s) / 2 - 4, y + (size - s) / 2 - 4, s + 8, s + 8, 12, C_WHITE);
        C2D_DrawImageAt(game->icon, x + (size - s) / 2, y + (size - s) / 2, 0.7f, NULL, s / 48.0f, s / 48.0f);
    } else {
        float d = size * 0.56f;
        C2D_DrawCircleSolid(x + size / 2, y + size / 2, 0.6f, d / 2, COL(0xFF, 0xFF, 0xFF, 0xD0));
        ui_icon(icon, x + (size - d * 0.62f) / 2, y + (size - d * 0.62f) / 2, d * 0.62f);
    }
}

static void draw_hero_text(float x, float y, float w, const char *kind, const char *name, const char *desc, const char *cta)
{
    ui_text(x, y, 0.45f, C_INK_MUTED, 0, kind);
    ui_text_wrap(x, y + 16, 0.8f, C_INK, w, name);
    ui_text_wrap(x, y + 56, 0.5f, C_INK_SOFT, w, desc);
    if (cta) ui_pill_dark(x, y + 120, 96, 28, cta);
}

// ---------------------------------------------------------------------------
// Accueil

typedef struct { float x, y, w, h; } Rect;

static Rect home_rect(int i)
{
    static const float cardX[CARD_COUNT] = {8, 149, 234};
    static const float cardW[CARD_COUNT] = {134, 78, 78};
    if (i < CARD_COUNT) return (Rect){cardX[i], 34, cardW[i], 120};
    int d = i - CARD_COUNT;
    return (Rect){16 + d * 50, 166, 34, 34};
}

static Rect header_rect(int b)
{
    switch (b) {
    case HB_PROFILE: return (Rect){8, 5, 112, 24};
    case HB_LUMA: return (Rect){196, 5, 62, 24};
    case HB_CAPTURE: return (Rect){264, 5, 24, 24};
    default: return (Rect){292, 5, 24, 24};
    }
}

static void open_update(void)
{
    s_screen = SCR_UPDATE;
    s_upd = s_updAvailable ? UPD_AVAILABLE : UPD_IDLE;
}

static void header_action(int b)
{
    switch (b) {
    case HB_PROFILE: toast("Le compte Heivoli arrive avec le site."); break;
    case HB_LUMA:
        // Repasser sur Luma : on désactive le démarrage sur Heivoli puis on redémarre
        // sur le menu d'origine. Heivoli Installer permet de le réactiver.
        if (!luma_config_present() || !luma_autoboot_enabled()) {
            s_quit = true; // Heivoli lancé à la main : on quitte simplement
        } else if (osGetTime() > s_lumaConfirmUntil) {
            s_lumaConfirmUntil = osGetTime() + 3000;
            toast("Appuie encore sur Luma pour redémarrer sur le menu d'origine.");
        } else if (luma_set_autoboot(false) == 0) {
            draw_loading("Redémarrage...");
            system_reboot();
        } else {
            toast("Impossible de modifier la configuration de Luma.");
        }
        break;
    case HB_CAPTURE: toast("Capture : utilise le menu Rosalina (L + Bas + SELECT) en attendant."); break;
    case HB_BELL:
        if (s_updAvailable) open_update();
        else toast("Aucune notification.");
        break;
    }
}

static void launch_resume(void)
{
    if (!s_resume) {
        s_screen = SCR_LIBRARY;
        return;
    }
    if (R_SUCCEEDED(launcher_prepare_title(s_resume->titleId, s_resume->mediatype))) s_quit = true;
    else toast("Impossible de relancer ce jeu.");
}

static void open_item(int i)
{
    switch (i) {
    case H_RESUME: launch_resume(); break;
    case H_LIBRARY: s_screen = SCR_LIBRARY; break;
    case H_COMMUNITY:
        // Ouvrir l'applet Miiverse depuis le Homebrew Launcher fait planter la console :
        // on ne le tente que depuis la version CIA.
        if (updater_is_3dsx()) toast("La Communauté (Juxt) s'ouvre depuis la version CIA de Heivoli.");
        else launcher_open_miiverse();
        break;
    case H_ONLINE: {
        const Entry *e = library_find_title("Nimbus");
        if (!e) toast("Nimbus n'est pas installé. Installe-le (en CIA) pour utiliser Pretendo.");
        else if (R_SUCCEEDED(launcher_prepare_title(e->titleId, e->mediatype))) s_quit = true;
        else toast("Impossible d'ouvrir Nimbus.");
        break;
    }
    case H_FRIENDS: toast("Les amis arrivent avec le compte Heivoli."); break;
    case H_GALLERY: toast("L'import des captures Luma arrive bientôt."); break;
    case H_SHOP: toast("La boutique Heivoli arrive avec le site."); break;
    case H_MUSIC: toast("Le lecteur de musique arrive dans une prochaine version."); break;
    case H_SETTINGS: s_screen = SCR_SETTINGS; break;
    }
}

static void update_home(u32 down, touchPosition *tp)
{
    int i = s_homeSel;
    if (down & KEY_RIGHT) {
        if (i < CARD_COUNT - 1 || (i >= CARD_COUNT && i < H_COUNT - 1)) i++;
    }
    if (down & KEY_LEFT) {
        if ((i > 0 && i < CARD_COUNT) || i > CARD_COUNT) i--;
    }
    if (down & KEY_DOWN && i < CARD_COUNT) i = (i == 0) ? H_FRIENDS : (i == 1 ? H_SHOP : H_SETTINGS);
    if (down & KEY_UP && i >= CARD_COUNT) i = (i <= H_FRIENDS) ? 0 : (i <= H_SHOP ? 1 : 2);
    s_homeSel = i;
    if (down & KEY_A) open_item(s_homeSel);
    if (down & KEY_START) s_quit = true;
    if (down & KEY_SELECT) open_update();
    if (down & KEY_TOUCH) {
        for (int b = 0; b < HB_COUNT; b++) {
            Rect r = header_rect(b);
            if (tp->px >= r.x && tp->px < r.x + r.w && tp->py >= r.y && tp->py < r.y + r.h) header_action(b);
        }
        for (int k = 0; k < H_COUNT; k++) {
            Rect r = home_rect(k);
            if (k >= CARD_COUNT) { r.y -= 2; r.h += 20; } // inclut le libellé
            if (tp->px >= r.x && tp->px < r.x + r.w && tp->py >= r.y && tp->py < r.y + r.h) {
                if (s_homeSel == k) open_item(k);
                else s_homeSel = k;
            }
        }
    }
}

static void resume_texts(const char **name, const char **desc, const char **cta)
{
    if (s_resume) {
        *name = s_resume->name;
        *desc = "Relance ta dernière partie en un geste.";
        *cta = "Jouer";
    } else {
        *name = "Aucun jeu récent";
        *desc = "Lance un jeu depuis la Bibliothèque : il apparaîtra ici.";
        *cta = "Bibliothèque";
    }
}

static void draw_home_top(void)
{
    ui_background(TOP_W, SCR_H, true);
    const Item *it = &s_items[s_homeSel];
    const char *name = it->name, *desc = it->desc, *cta = it->cta;
    if (s_homeSel == H_RESUME) resume_texts(&name, &desc, &cta);
    draw_hero(28, 30, 150, it->tint, it->icon, s_homeSel == H_RESUME ? s_resume : NULL);
    draw_hero_text(198, 32, 186, it->kind, name, desc, cta);
    if (s_updAvailable) {
        char b[64];
        snprintf(b, sizeof(b), "Mise à jour %s disponible", s_updInfo.version);
        ui_text(28, 190, 0.45f, COL(0x04, 0x76, 0xAC, 0xFF), 0, b);
    }
    draw_status_bar(NULL);
}

static void draw_home_bottom(void)
{
    ui_background(BOT_W, SCR_H, false);
    // En-tête : profil, bascule Luma, capture, notifications
    Rect p = header_rect(HB_PROFILE);
    ui_card(p.x, p.y, p.w, p.h, p.h / 2, false);
    C2D_DrawCircleSolid(p.x + 12, p.y + 12, 0.6f, 10, C_LAV);
    ui_text(p.x + 5, p.y + 6, 0.36f, COL(0x4B, 0x3F, 0xA3, 0xFF), 0, "HN");
    ui_text(p.x + 27, p.y + 5, 0.44f, C_INK, 0, "Invité");
    Rect l = header_rect(HB_LUMA);
    ui_card(l.x, l.y, l.w, l.h, l.h / 2, false);
    ui_icon(sprites_ic_swap_idx, l.x + 6, l.y + 4, 16);
    ui_text(l.x + 25, l.y + 5, 0.44f, C_INK, 0, "Luma");
    for (int b = HB_CAPTURE; b <= HB_BELL; b++) {
        Rect r = header_rect(b);
        C2D_DrawCircleSolid(r.x + 12, r.y + 12, 0.5f, 12.5f, C_LINE);
        C2D_DrawCircleSolid(r.x + 12, r.y + 12, 0.55f, 11.5f, C_WHITE);
        ui_icon(b == HB_CAPTURE ? sprites_ic_camera_idx : sprites_ic_bell_idx, r.x + 4, r.y + 4, 16);
    }
    if (s_updAvailable) {
        Rect r = header_rect(HB_BELL);
        C2D_DrawCircleSolid(r.x + 21, r.y + 3, 0.7f, 4, COL(0xE5, 0x48, 0x4D, 0xFF));
    }

    for (int i = 0; i < CARD_COUNT; i++) {
        Rect r = home_rect(i);
        const Item *it = &s_items[i];
        ui_card(r.x, r.y, r.w, r.h, 12, i == s_homeSel);
        ui_rrect(r.x, r.y, r.w, 76, 12, it->tint);
        C2D_DrawRectSolid(r.x, r.y + 64, 0.55f, r.w, 12, it->tint); // coins bas droits
        if (i == H_RESUME && s_resume && s_resume->hasIcon) {
            ui_rrect(r.x + r.w / 2 - 26, 38 + 8, 52, 52, 8, C_WHITE);
            C2D_DrawImageAt(s_resume->icon, r.x + r.w / 2 - 24, 38 + 10, 0.7f, NULL, 1.0f, 1.0f);
        } else {
            C2D_DrawCircleSolid(r.x + r.w / 2, r.y + 38, 0.6f, 22, COL(0xFF, 0xFF, 0xFF, 0xD0));
            ui_icon(it->icon, r.x + r.w / 2 - 14, r.y + 24, 28);
        }
        const char *name = it->name, *d, *c;
        if (i == H_RESUME) resume_texts(&name, &d, &c);
        ui_text(r.x + 8, r.y + 80, 0.36f, C_INK_MUTED, 0, it->kind);
        ui_text(r.x + 8, r.y + 94, 0.48f, C_INK, 0, i == H_RESUME ? name : it->shortName);
    }
    for (int i = CARD_COUNT; i < H_COUNT; i++) {
        Rect r = home_rect(i);
        const Item *it = &s_items[i];
        if (i == s_homeSel) ui_selection_ring(r.x, r.y, r.w, r.h, r.w / 2);
        else C2D_DrawCircleSolid(r.x + r.w / 2, r.y + r.h / 2, 0.5f, r.w / 2 + 1, C_LINE);
        C2D_DrawCircleSolid(r.x + r.w / 2, r.y + r.h / 2, 0.55f, r.w / 2, C_WHITE);
        ui_icon(it->icon, r.x + 7, r.y + 7, 20);
        float lw = ui_text_width(0.36f, it->shortName);
        ui_text(r.x + (r.w - lw) / 2, r.y + r.h + 3, 0.36f, C_INK_SOFT, 0, it->shortName);
    }
    draw_hint("A : ouvrir   SELECT : mises à jour   START : Luma");
}

// ---------------------------------------------------------------------------
// Bibliothèque

#define ROW_Y0 38
#define ROW_H 36
#define ROWS 5

static Section *cur_section(void)
{
    return &library_sections()[s_tab];
}

static void draw_loading(const char *msg)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    ui_frame_begin();
    C2D_TargetClear(s_top, C_WHITE);
    C2D_SceneBegin(s_top);
    ui_background(TOP_W, SCR_H, true);
    ui_card(110, 92, 180, 44, 14, false);
    float w = ui_text_width(0.55f, msg);
    ui_text(200 - w / 2, 104, 0.55f, C_INK, 0, msg);
    C2D_TargetClear(s_bot, C_WHITE);
    C2D_SceneBegin(s_bot);
    ui_background(BOT_W, SCR_H, false);
    C3D_FrameEnd(0);
}

static void ensure_scanned(Section *s)
{
    if (s->scanned) return;
    draw_loading("Chargement...");
    library_scan(s);
}

static void remember_last(const Entry *e)
{
    g_settings.lastTitle = e->titleId;
    g_settings.lastMedia = e->mediatype;
    snprintf(g_settings.lastName, sizeof(g_settings.lastName), "%s", e->name);
    save_settings();
}

static void launch_entry(Section *s)
{
    if (s->count == 0) return;
    Entry *e = &s->items[s->sel];
    // Jeu 3DS ou homebrew installé en CIA : on peut le lancer directement.
    switch (e->titleId ? KIND_3DS_TITLE : s->kind) {
    case KIND_3DS_TITLE:
        if (R_SUCCEEDED(launcher_prepare_title(e->titleId, e->mediatype))) {
            remember_last(e);
            s_quit = true;
        } else {
            toast("Impossible de lancer ce jeu.");
        }
        break;
    case KIND_HOMEBREW: toast("Le lancement des homebrews arrive dans la prochaine version."); break;
    case KIND_ROM: toast("Le lancement des ROMs (via les émulateurs) arrive dans la prochaine version."); break;
    }
}

static void clamp_list(int *sel, int *scroll, int count, int rows)
{
    if (*sel >= count) *sel = count ? count - 1 : 0;
    if (*sel < 0) *sel = 0;
    if (*sel < *scroll) *scroll = *sel;
    if (*sel >= *scroll + rows) *scroll = *sel - rows + 1;
}

static void update_library(u32 down, touchPosition *tp)
{
    if (down & KEY_R) s_tab = (s_tab + 1) % SECTION_COUNT;
    if (down & KEY_L) s_tab = (s_tab + SECTION_COUNT - 1) % SECTION_COUNT;
    Section *s = cur_section();
    ensure_scanned(s);
    if (down & KEY_DOWN) s->sel++;
    if (down & KEY_UP) s->sel--;
    if (down & KEY_RIGHT) s->sel += ROWS;
    if (down & KEY_LEFT) s->sel -= ROWS;
    clamp_list(&s->sel, &s->scroll, s->count, ROWS);
    if (down & KEY_Y) {
        s->scanned = false;
        ensure_scanned(s);
        toast("Liste actualisée.");
    }
    if (down & KEY_A) launch_entry(s);
    if ((down & KEY_X) && s->count && s->items[s->sel].titleId) {
        // Mauvais classement ? X le déplace entre « 3DS » et « Homebrew ».
        bool toHb = s->kind == KIND_3DS_TITLE;
        library_toggle_title_section(s->items[s->sel].titleId, toHb);
        library_scan(s);
        s_resume = g_settings.lastTitle ? library_find_tid(g_settings.lastTitle) : NULL;
        clamp_list(&s->sel, &s->scroll, s->count, ROWS);
        toast(toHb ? "Déplacé dans Homebrew." : "Déplacé dans Jeux 3DS.");
    }
    if (down & KEY_B) s_screen = SCR_HOME;
    if (down & KEY_TOUCH) {
        if (tp->py < 32) {
            int t = tp->px / (BOT_W / SECTION_COUNT);
            if (t >= 0 && t < SECTION_COUNT) s_tab = t;
        } else if (tp->py >= ROW_Y0 && tp->py < ROW_Y0 + ROWS * ROW_H) {
            int i = s->scroll + (tp->py - ROW_Y0) / ROW_H;
            if (i < s->count) {
                if (i == s->sel) launch_entry(s);
                else s->sel = i;
            }
        }
    }
}

static void draw_library_top(void)
{
    ui_background(TOP_W, SCR_H, true);
    Section *s = cur_section();
    if (s->count == 0) {
        draw_hero(28, 30, 150, s->c1, sprites_ic_list_idx, NULL);
        char title[64];
        snprintf(title, sizeof(title), "Aucun jeu %s", s->label);
        if (s->kind == KIND_ROM) {
            char p[160];
            snprintf(p, sizeof(p), "Dépose tes jeux dans %s/%s/ puis appuie sur Y.", ROMS_ROOT, s->folder);
            draw_hero_text(198, 32, 186, s->full, title, p, NULL);
        } else {
            draw_hero_text(198, 32, 186, s->full, title,
                           s->kind == KIND_HOMEBREW ? "Les fichiers .3dsx de sdmc:/3ds/ apparaîtront ici." : "Aucun jeu installé trouvé.", NULL);
        }
    } else {
        Entry *e = &s->items[s->sel];
        draw_hero(28, 30, 150, s->c1, sprites_ic_pad_idx, e->hasIcon ? e : NULL);
        draw_hero_text(198, 32, 186, s->full, e->name,
                       e->titleId ? (s->kind == KIND_3DS_TITLE ? "Jeu installé sur la console." : "Homebrew installé (CIA).") : e->path, "Jouer");
    }
    draw_status_bar("/ Bibliothèque");
}

static void draw_library_bottom(void)
{
    ui_background(BOT_W, SCR_H, false);
    Section *secs = library_sections();
    float tw = (float)BOT_W / SECTION_COUNT;
    for (int i = 0; i < SECTION_COUNT; i++) {
        float x = i * tw;
        bool on = i == s_tab;
        ui_rrect(x + 3, 6, tw - 6, 22, 11, on ? C_INK : C_WHITE);
        float w = ui_text_width(0.4f, secs[i].label);
        ui_text(x + (tw - w) / 2, 10, 0.4f, on ? C_WHITE : C_INK, 0, secs[i].label);
    }
    Section *s = cur_section();
    for (int r = 0; r < ROWS; r++) {
        int i = s->scroll + r;
        if (i >= s->count) break;
        Entry *e = &s->items[i];
        float y = ROW_Y0 + r * ROW_H;
        ui_card(8, y, 304, ROW_H - 6, 10, i == s->sel);
        if (e->hasIcon) C2D_DrawImageAt(e->icon, 13, y + 2, 0.7f, NULL, 26.0f / 48.0f, 26.0f / 48.0f);
        else ui_rrect(13, y + 2, 26, 26, 7, s->c1);
        ui_text(48, y + 7, 0.48f, C_INK, 0, e->name);
    }
    if (s->count == 0) ui_text(16, 110, 0.48f, C_INK_SOFT, 0, "Dossier vide pour l'instant.");
    draw_hint("L/R : console   A : jouer   X : changer de rangée   Y : actualiser");
}

// ---------------------------------------------------------------------------
// Réglages

typedef enum { SET_THEME, SET_SOUNDS, SET_AUTOBOOT, SET_AUTOUPD, SET_CHANNEL, SET_ACCOUNT, SET_RESCAN, SET_UPDATE, SET_RECOVERY, SET_ABOUT, SET_COUNT } SetRow;

#define SET_ROWS 6
#define SET_Y0 34
#define SET_H 32

typedef struct {
    const char *name, *group, *desc;
    size_t icon;
    bool isToggle;
} SetDef;

static const SetDef s_setDefs[SET_COUNT] = {
    {"Thème", "APPARENCE", "Couleur pastel des fonds de Heivoli.", sprites_ic_theme_idx, false},
    {"Sons du menu", "APPARENCE", "Petits sons de navigation (arrivent dans une prochaine version).", sprites_ic_sound_idx, true},
    {"Démarrage auto", "SYSTÈME", "Démarrer la console directement sur Heivoli. Désactivé : elle démarre sur son menu d'origine (Luma).", sprites_ic_power_idx, true},
    {"Mises à jour auto", "SYSTÈME", "Vérifie au démarrage s'il existe une nouvelle version, par Wi-Fi.", sprites_ic_update_idx, true},
    {"Canal", "SYSTÈME", "Stable pour tout le monde, Bêta pour tester les nouveautés en avance.", sprites_ic_list_idx, false},
    {"Compte Heivoli", "COMPTE", "Connexion par QR code ou e-mail : arrive avec le site Heivoli.", sprites_ic_account_idx, false},
    {"Bibliothèque", "JEUX", "Relit les dossiers de ROMs, les homebrews et les jeux installés.", sprites_ic_pad_idx, false},
    {"Rechercher une MAJ", "SYSTÈME", "Vérifie tout de suite s'il existe une nouvelle version.", sprites_ic_update_idx, false},
    {"Mode secours", "SYSTÈME", "Réinstaller la dernière version ou revenir à la précédente. Aussi accessible en maintenant L+R au démarrage.", sprites_ic_info_idx, false},
    {"À propos", "SYSTÈME", "Heivoli Home, le menu de Heivoli Network.", sprites_ic_info_idx, false},
};

static bool set_toggle_value(int row)
{
    switch (row) {
    case SET_SOUNDS: return g_settings.sounds;
    case SET_AUTOBOOT: return luma_autoboot_enabled();
    case SET_AUTOUPD: return g_settings.autoUpdate;
    default: return false;
    }
}

static void set_value_text(int row, char *out, size_t cap)
{
    switch (row) {
    case SET_THEME: snprintf(out, cap, "%s", ui_theme_name(g_settings.theme)); break;
    case SET_CHANNEL: snprintf(out, cap, "%s", g_settings.beta ? "Bêta" : "Stable"); break;
    case SET_ACCOUNT: snprintf(out, cap, "Bientôt"); break;
    case SET_RESCAN: snprintf(out, cap, "Actualiser"); break;
    case SET_UPDATE: snprintf(out, cap, "Vérifier"); break;
    case SET_RECOVERY: snprintf(out, cap, "Ouvrir"); break;
    case SET_ABOUT: snprintf(out, cap, "v%s", APP_VERSION); break;
    default: snprintf(out, cap, "%s", set_toggle_value(row) ? "Activé" : "Désactivé"); break;
    }
}

static void activate_setting(int row)
{
    switch (row) {
    case SET_THEME:
        g_settings.theme = (g_settings.theme + 1) % THEME_COUNT;
        ui_set_theme(g_settings.theme);
        save_settings();
        break;
    case SET_SOUNDS: g_settings.sounds = !g_settings.sounds; save_settings(); break;
    case SET_AUTOBOOT:
        if (!luma_config_present()) {
            toast("Luma3DS v13 ou plus récent est nécessaire.");
        } else if (luma_set_autoboot(!luma_autoboot_enabled()) == 0) {
            toast(luma_autoboot_enabled() ? "La console démarrera sur Heivoli." : "La console démarrera sur son menu d'origine.");
        } else {
            toast("Impossible de modifier la configuration de Luma.");
        }
        break;
    case SET_AUTOUPD: g_settings.autoUpdate = !g_settings.autoUpdate; save_settings(); break;
    case SET_CHANNEL:
        g_settings.beta = !g_settings.beta;
        s_updAvailable = false;
        save_settings();
        break;
    case SET_ACCOUNT: toast("Le compte Heivoli arrive avec le site."); break;
    case SET_RESCAN: {
        Section *secs = library_sections();
        for (int i = 0; i < SECTION_COUNT; i++) secs[i].scanned = false;
        draw_loading("Actualisation...");
        library_scan(&secs[0]);
        s_resume = g_settings.lastTitle ? library_find_tid(g_settings.lastTitle) : NULL;
        toast("Bibliothèque actualisée.");
        break;
    }
    case SET_UPDATE:
        s_screen = SCR_UPDATE;
        s_upd = UPD_IDLE;
        break;
    case SET_RECOVERY: s_screen = SCR_RECOVERY; s_recSel = 0; break;
    default: break;
    }
}

static void update_settings(u32 down, touchPosition *tp)
{
    if (down & KEY_DOWN) s_setSel++;
    if (down & KEY_UP) s_setSel--;
    clamp_list(&s_setSel, &s_setScroll, SET_COUNT, SET_ROWS);
    if (down & KEY_A) activate_setting(s_setSel);
    if (down & KEY_B) s_screen = SCR_HOME;
    if (down & KEY_TOUCH) {
        if (tp->py < 30 && tp->px < 44) s_screen = SCR_HOME;
        else if (tp->py >= SET_Y0 && tp->py < SET_Y0 + SET_ROWS * SET_H) {
            int i = s_setScroll + (tp->py - SET_Y0) / SET_H;
            if (i < SET_COUNT) {
                if (i == s_setSel || tp->px > 240) activate_setting(i);
                s_setSel = i;
            }
        }
    }
}

static void draw_settings_top(void)
{
    ui_background(TOP_W, SCR_H, true);
    const SetDef *d = &s_setDefs[s_setSel];
    C2D_DrawCircleSolid(28 + 75, 30 + 75, 0.5f, 76, C_LINE);
    C2D_DrawCircleSolid(28 + 75, 30 + 75, 0.55f, 75, C_WHITE);
    ui_icon(d->icon, 28 + 75 - 36, 30 + 75 - 36, 72);
    char v[48];
    set_value_text(s_setSel, v, sizeof(v));
    draw_hero_text(198, 32, 186, d->group, d->name, d->desc, NULL);
    float w = ui_text_width(0.5f, v) + 24;
    ui_card(198, 152, w, 26, 13, false);
    ui_text(210, 156, 0.5f, C_INK, 0, v);
    draw_status_bar("/ Réglages");
}

static void draw_settings_bottom(void)
{
    ui_background(BOT_W, SCR_H, false);
    C2D_DrawCircleSolid(22, 16, 0.5f, 13, C_LINE);
    C2D_DrawCircleSolid(22, 16, 0.55f, 12, C_WHITE);
    ui_text(17, 6, 0.6f, C_INK, 0, "<");
    ui_text(44, 6, 0.62f, C_INK, 0, "Réglages");
    for (int r = 0; r < SET_ROWS; r++) {
        int i = s_setScroll + r;
        if (i >= SET_COUNT) break;
        float y = SET_Y0 + r * SET_H;
        ui_card(8, y, 304, SET_H - 5, 10, i == s_setSel);
        ui_icon(s_setDefs[i].icon, 16, y + 4, 19);
        ui_text(44, y + 5, 0.47f, C_INK, 0, s_setDefs[i].name);
        if (s_setDefs[i].isToggle) {
            ui_toggle(268, y + 3, set_toggle_value(i));
        } else {
            char v[48];
            set_value_text(i, v, sizeof(v));
            float w = ui_text_width(0.42f, v);
            ui_text(298 - w, y + 7, 0.42f, C_INK_SOFT, 0, v);
            ui_text(301, y + 6, 0.42f, C_INK_MUTED, 0, ">");
        }
    }
    draw_hint("Haut/Bas : choisir   A : changer   B : retour");
}

// ---------------------------------------------------------------------------
// Mise à jour

static void render_frame(void);

static bool on_progress(u64 now, u64 total, void *user)
{
    (void)user;
    s_updFrac = total ? (float)now / (float)total : 0;
    u64 t = osGetTime();
    if (t - s_lastProgressDraw > 100) {
        s_lastProgressDraw = t;
        hidScanInput();
        if (hidKeysDown() & KEY_B) return false;
        if (!aptMainLoop()) return false;
        render_frame();
    }
    return true;
}

static void do_check(void)
{
    s_upd = UPD_CHECKING;
    render_frame();
    int rc = updater_check(manifest_url(), &s_updInfo, s_updMsg, sizeof(s_updMsg));
    if (rc < 0) {
        s_upd = UPD_ERROR;
    } else if (rc == 1 || s_updForce) {
        s_updAvailable = rc == 1;
        s_upd = UPD_AVAILABLE;
    } else {
        s_upd = UPD_UPTODATE;
    }
}

static void do_install(void)
{
    s_upd = UPD_DOWNLOADING;
    s_updFrac = 0;
    s_lastProgressDraw = 0;
    int rc = updater_install(&s_updInfo, s_selfPath, on_progress, NULL, s_updMsg, sizeof(s_updMsg));
    if (rc == 0) {
        s_upd = UPD_DONE;
        s_updAvailable = false;
    } else {
        s_upd = UPD_ERROR;
    }
}

static void update_update(u32 down)
{
    switch (s_upd) {
    case UPD_IDLE:
    case UPD_UPTODATE:
    case UPD_ERROR:
        if (down & KEY_A) do_check();
        break;
    case UPD_AVAILABLE:
        if (down & KEY_A) do_install();
        break;
    case UPD_DONE:
        if (down & KEY_A) s_quit = true;
        break;
    default:
        break;
    }
    if ((down & KEY_B) && s_upd != UPD_DONE) {
        s_updForce = false;
        s_screen = SCR_HOME;
    }
}

static void draw_update_top(void)
{
    ui_background(TOP_W, SCR_H, true);
    C2D_DrawCircleSolid(103, 105, 0.5f, 76, C_LINE);
    C2D_DrawCircleSolid(103, 105, 0.55f, 75, C_WHITE);
    ui_icon(sprites_ic_update_idx, 103 - 36, 105 - 36, 72);
    char line[200];
    const char *kind = g_settings.beta ? "MISES À JOUR · BÊTA" : "MISES À JOUR";
    switch (s_upd) {
    case UPD_IDLE:
        snprintf(line, sizeof(line), "Version installée : %s. Connecte-toi au Wi-Fi puis appuie sur A.", APP_VERSION);
        draw_hero_text(198, 32, 186, kind, "Rechercher", line, "A : Vérifier");
        break;
    case UPD_CHECKING:
        draw_hero_text(198, 32, 186, kind, "Recherche...", "Connexion au serveur Heivoli.", NULL);
        break;
    case UPD_UPTODATE:
        snprintf(line, sizeof(line), "Tu as la dernière version (%s).", APP_VERSION);
        draw_hero_text(198, 32, 186, kind, "Tout est à jour", line, NULL);
        break;
    case UPD_AVAILABLE:
        snprintf(line, sizeof(line), "Version %s", s_updInfo.version);
        draw_hero_text(198, 32, 186, kind, line, s_updInfo.notes[0] ? s_updInfo.notes : "Nouvelle version disponible.", "A : Installer");
        break;
    case UPD_DOWNLOADING:
        draw_hero_text(198, 32, 186, kind, "Téléchargement", "B : annuler", NULL);
        ui_progress(198, 120, 180, 10, s_updFrac);
        snprintf(line, sizeof(line), "%d %%", (int)(s_updFrac * 100));
        ui_text(198, 136, 0.5f, C_INK_SOFT, 0, line);
        break;
    case UPD_DONE:
        draw_hero_text(198, 32, 186, kind, "Installé !", "Appuie sur A : Heivoli va se fermer. Relance-le pour utiliser la nouvelle version.", "A : Fermer");
        break;
    case UPD_ERROR:
        draw_hero_text(198, 32, 186, kind, "Oups", s_updMsg, "A : Réessayer");
        break;
    }
    draw_status_bar("/ Mises à jour");
}

static void draw_update_bottom(void)
{
    ui_background(BOT_W, SCR_H, false);
    ui_card(12, 16, 296, 120, 12, false);
    char line[256];
    snprintf(line, sizeof(line), "Version : %s\nInstallation : %s\nCanal : %s\nServeur : %s", APP_VERSION,
             updater_is_3dsx() ? ".3dsx (développement)" : "CIA", g_settings.beta ? "Bêta" : "Stable", manifest_url());
    ui_text_wrap(24, 26, 0.42f, C_INK, 272, line);
    ui_text_wrap(16, 150, 0.4f, C_INK_MUTED, 290, "En cas de souci : maintiens L+R au démarrage pour ouvrir le mode secours.");
    draw_hint(s_upd == UPD_DOWNLOADING ? "B : annuler" : "A : continuer   B : retour");
}

// ---------------------------------------------------------------------------
// Mode secours (L+R au démarrage, ou depuis les Réglages)

static void update_recovery(u32 down)
{
    bool hasBak = updater_is_3dsx() && updater_has_backup(s_selfPath);
    int n = hasBak ? 3 : 2;
    if (down & KEY_DOWN) s_recSel = (s_recSel + 1) % n;
    if (down & KEY_UP) s_recSel = (s_recSel + n - 1) % n;
    if (down & KEY_B) { s_screen = SCR_HOME; return; }
    if (!(down & KEY_A)) return;
    int choice = s_recSel;
    if (!hasBak && choice == 1) choice = 2;
    if (choice == 0) {
        s_updForce = true;
        s_screen = SCR_UPDATE;
        do_check();
    } else if (choice == 1) {
        char err[128];
        if (updater_restore_backup(s_selfPath, err, sizeof(err)) == 0) {
            s_screen = SCR_UPDATE;
            s_upd = UPD_DONE;
        } else {
            toast(err);
        }
    } else {
        s_screen = SCR_HOME;
    }
}

static void draw_recovery_top(void)
{
    ui_background(TOP_W, SCR_H, true);
    C2D_DrawCircleSolid(103, 105, 0.5f, 76, C_LINE);
    C2D_DrawCircleSolid(103, 105, 0.55f, 75, C_WHITE);
    ui_icon(sprites_ic_info_idx, 103 - 36, 105 - 36, 72);
    draw_hero_text(198, 32, 186, "SYSTÈME", "Mode secours",
                   "Si Heivoli ne marche plus bien, réinstalle la dernière version par Wi-Fi ou reviens à la précédente.", NULL);
    draw_status_bar("/ Secours");
}

static void draw_recovery_bottom(void)
{
    ui_background(BOT_W, SCR_H, false);
    bool hasBak = updater_is_3dsx() && updater_has_backup(s_selfPath);
    const char *opts[3] = {"Réinstaller la dernière version",
                           hasBak ? "Restaurer la version précédente" : "Démarrer normalement",
                           "Démarrer normalement"};
    int n = hasBak ? 3 : 2;
    for (int i = 0; i < n; i++) {
        float y = 26 + i * 50;
        ui_card(14, y, 292, 40, 12, i == s_recSel);
        ui_text(28, y + 11, 0.52f, C_INK, 0, opts[i]);
    }
    draw_hint("Haut/Bas : choisir   A : valider");
}

// ---------------------------------------------------------------------------

static void render_frame(void)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    ui_frame_begin();

    C2D_TargetClear(s_top, C_WHITE);
    C2D_SceneBegin(s_top);
    switch (s_screen) {
    case SCR_HOME: draw_home_top(); break;
    case SCR_LIBRARY: draw_library_top(); break;
    case SCR_SETTINGS: draw_settings_top(); break;
    case SCR_UPDATE: draw_update_top(); break;
    case SCR_RECOVERY: draw_recovery_top(); break;
    }

    C2D_TargetClear(s_bot, C_WHITE);
    C2D_SceneBegin(s_bot);
    switch (s_screen) {
    case SCR_HOME: draw_home_bottom(); break;
    case SCR_LIBRARY: draw_library_bottom(); break;
    case SCR_SETTINGS: draw_settings_bottom(); break;
    case SCR_UPDATE: draw_update_bottom(); break;
    case SCR_RECOVERY: draw_recovery_bottom(); break;
    }
    draw_toast();

    C3D_FrameEnd(0);
}

int main(int argc, char **argv)
{
    if (argc > 0 && argv[0] && !strncmp(argv[0], "sdmc:", 5))
        snprintf(s_selfPath, sizeof(s_selfPath), "%s", argv[0]);

    romfsInit();
    cfguInit();
    amInit();
    ptmuInit();
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    s_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    s_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    ui_init();
    init_items();
    s_sheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    if (s_sheet) {
        s_hasSheet = true;
        ui_set_sheet(s_sheet);
    }
    library_prepare_folders();
    settings_load(SETTINGS_PATH, &g_settings);
    ui_set_theme(g_settings.theme);

    // Mode secours : L+R maintenus au démarrage.
    hidScanInput();
    u32 held = hidKeysHeld();
    if ((held & (KEY_L | KEY_R)) == (KEY_L | KEY_R)) {
        s_screen = SCR_RECOVERY;
    } else {
        if (g_settings.lastTitle) {
            draw_loading("Chargement...");
            s_resume = library_find_tid(g_settings.lastTitle);
        }
        if (g_settings.autoUpdate && net_wifi_connected()) {
            draw_loading("Recherche de MAJ...");
            char err[160];
            if (updater_check(manifest_url(), &s_updInfo, err, sizeof(err)) == 1) s_updAvailable = true;
        }
    }

    while (!s_quit && aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        touchPosition tp;
        hidTouchRead(&tp);

        switch (s_screen) {
        case SCR_HOME: update_home(down, &tp); break;
        case SCR_LIBRARY: update_library(down, &tp); break;
        case SCR_SETTINGS: update_settings(down, &tp); break;
        case SCR_UPDATE: update_update(down); break;
        case SCR_RECOVERY: update_recovery(down); break;
        }
        if (s_quit) break;
        render_frame();
    }

    library_free();
    if (s_sheet) C2D_SpriteSheetFree(s_sheet);
    ui_exit();
    net_exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    ptmuExit();
    amExit();
    cfguExit();
    romfsExit();

    // Si un jeu a été choisi, le saut se fait maintenant.
    launcher_commit_jump();
    return 0;
}
