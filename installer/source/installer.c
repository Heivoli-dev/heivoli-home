// Heivoli Installer — l'appli (CIA) qui installe le système Heivoli sur la console.
//
// Ce qu'elle fait :
//  - installer / réinstaller le système Heivoli (copie intégrée, sans Wi-Fi) ;
//  - choisir sur quoi la console démarre : Heivoli ou le menu d'origine (Luma) ;
//  - mettre à jour Heivoli par Wi-Fi ;
//  - désinstaller Heivoli (remet le Homebrew Menu d'origine) ;
//  - redémarrer la console.
//
// Étape 1 (cette version) : Heivoli est placé en sdmc:/boot.3dsx et Luma démarre
// directement dessus (autoboot_mode = 3ds).
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "luma_config.h"
#include "net.h"
#include "sprites.h"
#include "ui.h"
#include "updater.h"

#define EMBEDDED_SYSTEM  "romfs:/heivoli-system.3dsx"
#define EMBEDDED_VERSION "romfs:/heivoli-system.ver"
#define STATE_PATH       HEIVOLI_ROOT "/install.ini"
#define BACKUP_DIR       HEIVOLI_ROOT "/backup"
#define HBMENU_BACKUP    BACKUP_DIR "/boot.3dsx"

typedef struct {
    bool installed;
    char version[16];
} InstallState;

enum { A_INSTALL, A_BOOT, A_UPDATE, A_UNINSTALL, A_REBOOT, A_COUNT };

static C3D_RenderTarget *s_top, *s_bot;
static C2D_SpriteSheet s_sheet;
static bool s_hasSheet = false;

static InstallState s_state;
static char s_embeddedVersion[16] = "?";
static bool s_hasLuma = false;
static bool s_autoboot = false;

static int s_sel = 0;
static bool s_quit = false;
static bool s_confirmUninstall = false;
static bool s_rebootSuggested = false;

static char s_busy[96] = "";
static float s_progress = -1;
static u64 s_lastDraw = 0;

static char s_msg[200] = "";
static bool s_msgIsError = false;

// ---------------------------------------------------------------------------
// État de l'installation

static bool exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

static void load_state(void)
{
    memset(&s_state, 0, sizeof(s_state));
    FILE *f = fopen(STATE_PATH, "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "installed=", 10)) s_state.installed = line[10] == '1';
            else if (!strncmp(line, "version=", 8)) snprintf(s_state.version, sizeof(s_state.version), "%s", line + 8);
        }
        fclose(f);
    }
    // Si le fichier système a disparu, on ne le considère plus installé.
    if (s_state.installed && !exists(SYSTEM_BOOT_PATH)) s_state.installed = false;

    f = fopen(EMBEDDED_VERSION, "r");
    if (f) {
        if (fgets(s_embeddedVersion, sizeof(s_embeddedVersion), f))
            s_embeddedVersion[strcspn(s_embeddedVersion, "\r\n")] = 0;
        fclose(f);
    }
    s_hasLuma = luma_config_present();
    s_autoboot = s_hasLuma && luma_autoboot_enabled();
}

static void save_state(void)
{
    mkdir(HEIVOLI_ROOT, 0777);
    FILE *f = fopen(STATE_PATH, "w");
    if (!f) return;
    fprintf(f, "installed=%d\nversion=%s\n", s_state.installed ? 1 : 0, s_state.version);
    fclose(f);
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    char *buf = malloc(64 * 1024);
    int rc = buf ? 0 : -1;
    size_t n;
    while (rc == 0 && (n = fread(buf, 1, 64 * 1024, in)) > 0)
        if (fwrite(buf, 1, n, out) != n) rc = -1;
    free(buf);
    fclose(in);
    if (fclose(out) != 0) rc = -1;
    return rc;
}

static void message(const char *txt, bool error)
{
    snprintf(s_msg, sizeof(s_msg), "%s", txt);
    s_msgIsError = error;
}

// ---------------------------------------------------------------------------
// Dessin

static void render_frame(void);

static void set_busy(const char *txt)
{
    snprintf(s_busy, sizeof(s_busy), "%s", txt ? txt : "");
    s_progress = -1;
    if (txt) render_frame();
}

static bool on_progress(u64 now, u64 total, void *user)
{
    (void)user;
    s_progress = total ? (float)now / (float)total : 0;
    u64 t = osGetTime();
    if (t - s_lastDraw > 100) {
        s_lastDraw = t;
        hidScanInput();
        if (hidKeysDown() & KEY_B) return false;
        if (!aptMainLoop()) return false;
        render_frame();
    }
    return true;
}

static const char *action_label(int a)
{
    switch (a) {
    case A_INSTALL: return s_state.installed ? "Réinstaller Heivoli" : "Installer Heivoli";
    case A_BOOT: return s_autoboot ? "Démarrer sur le menu d'origine" : "Démarrer sur Heivoli";
    case A_UPDATE: return "Mettre à jour par Wi-Fi";
    case A_UNINSTALL: return s_confirmUninstall ? "Confirmer la désinstallation" : "Désinstaller Heivoli";
    default: return s_rebootSuggested ? "Redémarrer maintenant" : "Redémarrer la console";
    }
}

static size_t action_icon(int a)
{
    switch (a) {
    case A_INSTALL: return sprites_ic_update_idx;
    case A_BOOT: return sprites_ic_swap_idx;
    case A_UPDATE: return sprites_ic_globe_idx;
    case A_UNINSTALL: return sprites_ic_info_idx;
    default: return sprites_ic_power_idx;
    }
}

static const char *action_desc(int a)
{
    switch (a) {
    case A_INSTALL: return "Copie le système Heivoli sur la carte SD et règle Luma pour démarrer dessus. Ton Homebrew Menu est mis de côté.";
    case A_BOOT: return s_autoboot ? "La console démarrera sur son menu d'origine. Heivoli reste installé." : "La console démarrera directement sur Heivoli.";
    case A_UPDATE: return "Télécharge la dernière version de Heivoli depuis le serveur Heivoli Network.";
    case A_UNINSTALL: return "Retire Heivoli, remet ton Homebrew Menu et le démarrage d'origine.";
    default: return "Redémarre la console pour appliquer les changements.";
    }
}

static void draw_top(void)
{
    ui_background(TOP_W, SCR_H, true);
    C2D_DrawCircleSolid(100, 100, 0.5f, 71, C_LINE);
    C2D_DrawCircleSolid(100, 100, 0.55f, 70, C_WHITE);
    if (s_hasSheet)
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(s_sheet, sprites_logo_idx), 100 - 48, 100 - 48, 0.6f, NULL, 1.5f, 1.5f);

    float x = 196;
    ui_text(x, 26, 0.45f, C_INK_MUTED, 0, "HEIVOLI INSTALLER");
    ui_text_wrap(x, 42, 0.78f, C_INK, 190, s_state.installed ? "Heivoli est installé" : "Heivoli n'est pas installé");
    char line[160];
    snprintf(line, sizeof(line), "Version installée : %s\nVersion fournie : %s\nDémarrage : %s\nLuma3DS : %s",
             s_state.installed ? s_state.version : "-", s_embeddedVersion,
             s_autoboot ? "Heivoli" : "menu d'origine", s_hasLuma ? "détecté" : "introuvable");
    ui_text_wrap(x, 92, 0.46f, C_INK_SOFT, 196, line);

    if (s_busy[0]) {
        ui_card(24, 176, 352, 28, 12, false);
        ui_text(36, 181, 0.46f, C_INK, 0, s_busy);
        if (s_progress >= 0) ui_progress(236, 186, 130, 8, s_progress);
    } else if (s_msg[0]) {
        ui_card(24, 176, 352, 28, 12, false);
        ui_text_wrap(36, 181, 0.44f, s_msgIsError ? COL(0xC0, 0x2E, 0x36, 0xFF) : C_INK, 330, s_msg);
    } else if (s_rebootSuggested) {
        ui_text(24, 184, 0.46f, COL(0x04, 0x76, 0xAC, 0xFF), 0, "Redémarre la console pour appliquer les changements.");
    }

    ui_bar(0, 212, TOP_W, 28);
    ui_text(10, 217, 0.5f, C_INK, 0, "heivoli");
    ui_text(70, 218, 0.44f, C_INK_MUTED, 0, "/ Installer v" APP_VERSION);
}

static void draw_bottom(void)
{
    ui_background(BOT_W, SCR_H, false);
    ui_text(12, 6, 0.6f, C_INK, 0, "Heivoli Installer");
    for (int a = 0; a < A_COUNT; a++) {
        float y = 30 + a * 36;
        ui_card(8, y, 304, 30, 10, a == s_sel);
        ui_icon(action_icon(a), 16, y + 5, 20);
        u32 col = (a == A_UNINSTALL && s_confirmUninstall) ? COL(0xC0, 0x2E, 0x36, 0xFF) : C_INK;
        ui_text(46, y + 6, 0.48f, col, 0, action_label(a));
    }
    ui_text_wrap(12, 192, 0.4f, C_INK_SOFT, 296, action_desc(s_sel));
    ui_text(12, 224, 0.4f, C_INK_MUTED, 0, "A : valider   START : quitter");
}

static void render_frame(void)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    ui_frame_begin();
    C2D_TargetClear(s_top, C_WHITE);
    C2D_SceneBegin(s_top);
    draw_top();
    C2D_TargetClear(s_bot, C_WHITE);
    C2D_SceneBegin(s_bot);
    draw_bottom();
    C3D_FrameEnd(0);
}

// ---------------------------------------------------------------------------
// Actions

static bool require_luma(void)
{
    if (s_hasLuma) return true;
    message("Luma3DS v13 ou plus récent est nécessaire (sdmc:/luma/config.ini introuvable).", true);
    return false;
}

static void do_install(void)
{
    if (!require_luma()) return;
    if (!exists(EMBEDDED_SYSTEM)) {
        message("Copie intégrée de Heivoli introuvable : recompile l'installateur.", true);
        return;
    }
    set_busy("Installation de Heivoli...");
    mkdir(HEIVOLI_ROOT, 0777);
    mkdir(BACKUP_DIR, 0777);
    mkdir(TMP_DIR, 0777);

    // Première installation : on garde le Homebrew Menu de côté.
    if (!s_state.installed && exists(SYSTEM_BOOT_PATH) && !exists(HBMENU_BACKUP)) {
        if (copy_file(SYSTEM_BOOT_PATH, HBMENU_BACKUP) != 0) {
            set_busy(NULL);
            message("Impossible de sauvegarder ton boot.3dsx actuel. Rien n'a été modifié.", true);
            return;
        }
    }
    const char *tmp = TMP_DIR "/system.3dsx";
    char err[128];
    if (copy_file(EMBEDDED_SYSTEM, tmp) != 0 || updater_replace_file(tmp, SYSTEM_BOOT_PATH, err, sizeof(err)) != 0) {
        set_busy(NULL);
        message("Copie du système impossible (carte SD pleine ou protégée ?).", true);
        return;
    }
    s_state.installed = true;
    snprintf(s_state.version, sizeof(s_state.version), "%s", s_embeddedVersion);
    save_state();

    if (luma_set_autoboot(true) != 0) {
        set_busy(NULL);
        message("Heivoli est copié, mais le réglage de démarrage de Luma n'a pas pu être modifié.", true);
        return;
    }
    s_autoboot = true;
    s_rebootSuggested = true;
    s_sel = A_REBOOT;
    set_busy(NULL);
    message("Heivoli est installé. Redémarre la console pour le lancer.", false);
}

static void do_toggle_boot(void)
{
    if (!require_luma()) return;
    if (!s_state.installed && !s_autoboot) {
        message("Installe d'abord Heivoli.", true);
        return;
    }
    if (luma_set_autoboot(!s_autoboot) != 0) {
        message("Impossible de modifier la configuration de Luma.", true);
        return;
    }
    s_autoboot = !s_autoboot;
    s_rebootSuggested = true;
    message(s_autoboot ? "La console démarrera sur Heivoli." : "La console démarrera sur son menu d'origine.", false);
}

static void do_update(void)
{
    if (!s_state.installed) {
        message("Installe d'abord Heivoli.", true);
        return;
    }
    set_busy("Recherche d'une mise à jour...");
    UpdateInfo info;
    char err[160];
    int rc = updater_check(UPDATE_MANIFEST_STABLE, &info, err, sizeof(err));
    if (rc < 0) {
        set_busy(NULL);
        message(err, true);
        return;
    }
    if (version_compare(info.version, s_state.version) <= 0) {
        set_busy(NULL);
        message("Heivoli est déjà à jour.", false);
        return;
    }
    char b[64];
    snprintf(b, sizeof(b), "Téléchargement de la version %s...", info.version);
    set_busy(b);
    s_lastDraw = 0;
    rc = updater_install_3dsx_to(&info, SYSTEM_BOOT_PATH, on_progress, NULL, err, sizeof(err));
    set_busy(NULL);
    if (rc != 0) {
        message(err, rc < 0);
        return;
    }
    snprintf(s_state.version, sizeof(s_state.version), "%s", info.version);
    save_state();
    s_rebootSuggested = true;
    message("Mise à jour installée. Redémarre la console.", false);
}

static void do_uninstall(void)
{
    if (!s_confirmUninstall) {
        s_confirmUninstall = true;
        message("Appuie encore une fois pour confirmer la désinstallation.", false);
        return;
    }
    s_confirmUninstall = false;
    set_busy("Désinstallation...");
    if (s_hasLuma) luma_set_autoboot(false);
    s_autoboot = false;

    char bak[300];
    snprintf(bak, sizeof(bak), "%s.bak", SYSTEM_BOOT_PATH);
    remove(bak);
    remove(SYSTEM_BOOT_PATH);
    if (exists(HBMENU_BACKUP)) copy_file(HBMENU_BACKUP, SYSTEM_BOOT_PATH);

    s_state.installed = false;
    s_state.version[0] = 0;
    save_state();
    s_rebootSuggested = true;
    set_busy(NULL);
    message("Heivoli est désinstallé. Ton Homebrew Menu est revenu.", false);
}

static void run_action(int a)
{
    if (a != A_UNINSTALL) s_confirmUninstall = false;
    switch (a) {
    case A_INSTALL: do_install(); break;
    case A_BOOT: do_toggle_boot(); break;
    case A_UPDATE: do_update(); break;
    case A_UNINSTALL: do_uninstall(); break;
    case A_REBOOT:
        set_busy("Redémarrage...");
        system_reboot();
        set_busy(NULL);
        message("Le redémarrage a échoué : éteins et rallume la console.", true);
        break;
    }
}

// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    romfsInit();
    cfguInit();
    amInit();
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    s_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    s_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    ui_init();
    s_sheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    if (s_sheet) {
        s_hasSheet = true;
        ui_set_sheet(s_sheet);
    }
    load_state();
    if (!s_hasLuma) message("Luma3DS v13 ou plus récent est nécessaire (sdmc:/luma/config.ini introuvable).", true);

    while (!s_quit && aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_DOWN) s_sel = (s_sel + 1) % A_COUNT;
        if (down & KEY_UP) s_sel = (s_sel + A_COUNT - 1) % A_COUNT;
        if (down & (KEY_UP | KEY_DOWN)) s_confirmUninstall = false;
        if (down & KEY_A) run_action(s_sel);
        if (down & KEY_START) s_quit = true;
        if (down & KEY_TOUCH) {
            touchPosition tp;
            hidTouchRead(&tp);
            if (tp.py >= 30 && tp.py < 30 + A_COUNT * 36) {
                int a = (tp.py - 30) / 36;
                if (a == s_sel) run_action(a);
                else {
                    s_sel = a;
                    s_confirmUninstall = false;
                }
            }
        }
        render_frame();
    }

    if (s_sheet) C2D_SpriteSheetFree(s_sheet);
    ui_exit();
    net_exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    amExit();
    cfguExit();
    romfsExit();
    return 0;
}
