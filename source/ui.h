#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <stdbool.h>

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240

#define COL(r, g, b, a) C2D_Color32((r), (g), (b), (a))

// Palette "Wii U / 3DS" pastel
#define C_INK       COL(0x23, 0x26, 0x2F, 0xFF)
#define C_INK_SOFT  COL(0x55, 0x5A, 0x6B, 0xFF)
#define C_INK_MUTED COL(0x6B, 0x6F, 0x86, 0xFF)
#define C_WHITE     COL(0xFF, 0xFF, 0xFF, 0xFF)
#define C_LINE      COL(0xE1, 0xE2, 0xEE, 0xFF)
#define C_CYAN      COL(0x2F, 0xBD, 0xEB, 0xFF)
#define C_CYAN_SOFT COL(0xC4, 0xEC, 0xFA, 0xFF)

// Fonds pastel des cartes
#define C_LAV   COL(0xE6, 0xDF, 0xFF, 0xFF)
#define C_SKY   COL(0xD9, 0xEC, 0xFF, 0xFF)
#define C_MINT  COL(0xD8, 0xF3, 0xE6, 0xFF)
#define C_PEACH COL(0xFF, 0xE6, 0xD6, 0xFF)

#define THEME_COUNT 4

void ui_init(void);
void ui_exit(void);
void ui_frame_begin(void);
void ui_set_theme(int theme);
const char *ui_theme_name(int theme);
void ui_set_sheet(C2D_SpriteSheet sheet);

u32 ui_mix(u32 a, u32 b, float t);

void ui_text(float x, float y, float scale, u32 color, u32 flags, const char *str);
void ui_text_wrap(float x, float y, float scale, u32 color, float width, const char *str);
float ui_text_width(float scale, const char *str);

void ui_rrect(float x, float y, float w, float h, float r, u32 c);
void ui_rrect_grad(float x, float y, float w, float h, float r, u32 top, u32 bot);

// Fond pastel du thème (top = écran du haut, avec une fine trame de points).
void ui_background(float w, float h, bool top);
// Carte blanche à bord fin ; sélection = contour blanc + anneau cyan.
void ui_card(float x, float y, float w, float h, float r, bool selected);
void ui_selection_ring(float x, float y, float w, float h, float r);
// Barre d'information blanche.
void ui_bar(float x, float y, float w, float h);
// Bouton sombre en pilule (action principale).
void ui_pill_dark(float x, float y, float w, float h, const char *label);
// Pastille pastel plate (onglets, vignettes sans image).
void ui_soft_tile(float x, float y, float w, float h, float r, u32 c, bool selected);
// Icône du jeu d'icônes (index tex3ds), dessinée à la taille `size`.
void ui_icon(size_t idx, float x, float y, float size);
void ui_toggle(float x, float y, bool on);
void ui_progress(float x, float y, float w, float h, float frac);
