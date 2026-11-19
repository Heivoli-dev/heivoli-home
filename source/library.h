#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <stdbool.h>

typedef enum {
    KIND_3DS_TITLE, // jeu ou appli installé (vient du menu HOME)
    KIND_HOMEBREW,  // fichier .3dsx dans sdmc:/3ds/
    KIND_ROM        // ROM rangée par l'utilisateur dans sdmc:/heivoli/roms/<console>/
} EntryKind;

typedef struct {
    char name[96];
    char path[256];
    u64 titleId;
    u8 mediatype;
    bool hasIcon;
    C3D_Tex iconTex;
    Tex3DS_SubTexture iconSub;
    C2D_Image icon;
} Entry;

typedef struct {
    const char *label;  // nom court affiché dans l'onglet
    const char *full;   // nom complet
    const char *folder; // sous-dossier de ROMS_ROOT (NULL pour 3DS/Homebrew)
    const char *exts;   // extensions acceptées, séparées par '|'
    EntryKind kind;
    u32 c1, c2;         // couleurs de la vignette
    Entry *items;
    int count, cap;
    int sel, scroll;
    bool scanned;
} Section;

#define SECTION_COUNT 8

void library_prepare_folders(void);
Section *library_sections(void);
void library_scan(Section *s);
void library_free(void);

// Cherche un titre installé dont le nom contient `needle` (insensible à la casse).
const Entry *library_find_title(const char *needle);
// Cherche un titre installé par son identifiant (NULL si absent).
const Entry *library_find_tid(u64 tid);

// Corrige le classement d'un titre installé (jeu <-> homebrew). Rescanner ensuite.
bool library_toggle_title_section(u64 tid, bool toHomebrew);
