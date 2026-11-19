// Boîte à outils de dessin au-dessus de citro2d : style menu système Wii U / 3DS,
// fonds pastel, cartes blanches, sélection cyan.
// Toutes les formes arrondies sont opaques : citro2d dessine les coins avec des
// cercles qui se chevauchent, ce qui donnerait des artefacts avec de la transparence.
#include "ui.h"
#include <string.h>

static C2D_TextBuf s_buf;
static C2D_SpriteSheet s_sheet = NULL;
static int s_theme = 0;

typedef struct {
    const char *name;
    u32 top0, top1, bot0, bot1, dots;
} Theme;

static Theme s_themes[THEME_COUNT];

void ui_init(void)
{
    s_buf = C2D_TextBufNew(8192);
    const Theme t[THEME_COUNT] = {
        {"Lavande", COL(0xF4, 0xF0, 0xFF, 0xFF), COL(0xE4, 0xF1, 0xFF, 0xFF), COL(0xF8, 0xF5, 0xFF, 0xFF), COL(0xEB, 0xF5, 0xFF, 0xFF), COL(0xDD, 0xDA, 0xEE, 0xFF)},
        {"Ciel",    COL(0xEA, 0xF5, 0xFF, 0xFF), COL(0xDC, 0xEB, 0xFF, 0xFF), COL(0xF2, 0xF8, 0xFF, 0xFF), COL(0xE3, 0xF0, 0xFF, 0xFF), COL(0xD0, 0xDF, 0xF0, 0xFF)},
        {"Menthe",  COL(0xEA, 0xFB, 0xF2, 0xFF), COL(0xDC, 0xF3, 0xEC, 0xFF), COL(0xF2, 0xFC, 0xF7, 0xFF), COL(0xE3, 0xF6, 0xEE, 0xFF), COL(0xCF, 0xE6, 0xDC, 0xFF)},
        {"Pêche",   COL(0xFF, 0xF3, 0xEA, 0xFF), COL(0xFD, 0xE6, 0xEE, 0xFF), COL(0xFF, 0xF8, 0xF2, 0xFF), COL(0xFD, 0xEE, 0xF2, 0xFF), COL(0xF0, 0xDA, 0xDE, 0xFF)},
    };
    memcpy(s_themes, t, sizeof(t));
}

void ui_exit(void)
{
    C2D_TextBufDelete(s_buf);
}

void ui_frame_begin(void)
{
    C2D_TextBufClear(s_buf);
}

void ui_set_theme(int theme)
{
    if (theme < 0 || theme >= THEME_COUNT) theme = 0;
    s_theme = theme;
}

const char *ui_theme_name(int theme)
{
    if (theme < 0 || theme >= THEME_COUNT) theme = 0;
    return s_themes[theme].name;
}

void ui_set_sheet(C2D_SpriteSheet sheet)
{
    s_sheet = sheet;
}

u32 ui_mix(u32 a, u32 b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    u32 out = 0;
    for (int i = 0; i < 4; i++) {
        float ca = (a >> (i * 8)) & 0xFF;
        float cb = (b >> (i * 8)) & 0xFF;
        u32 c = (u32)(ca + (cb - ca) * t + 0.5f);
        out |= (c & 0xFF) << (i * 8);
    }
    return out;
}

static bool parse(C2D_Text *t, const char *str)
{
    if (!str || !*str) return false;
    if (!C2D_TextParse(t, s_buf, str)) return false;
    C2D_TextOptimize(t);
    return true;
}

void ui_text(float x, float y, float scale, u32 color, u32 flags, const char *str)
{
    C2D_Text t;
    if (!parse(&t, str)) return;
    C2D_DrawText(&t, C2D_WithColor | flags, x, y, 0.9f, scale, scale, color);
}

void ui_text_wrap(float x, float y, float scale, u32 color, float width, const char *str)
{
    C2D_Text t;
    if (!parse(&t, str)) return;
    C2D_DrawText(&t, C2D_WithColor | C2D_WordWrap, x, y, 0.9f, scale, scale, color, width);
}

float ui_text_width(float scale, const char *str)
{
    C2D_Text t;
    float w = 0;
    if (!parse(&t, str)) return 0;
    C2D_TextGetDimensions(&t, scale, scale, &w, NULL);
    return w;
}

void ui_rrect(float x, float y, float w, float h, float r, u32 c)
{
    ui_rrect_grad(x, y, w, h, r, c, c);
}

void ui_rrect_grad(float x, float y, float w, float h, float r, u32 top, u32 bot)
{
    if (w <= 0 || h <= 0) return;
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r < 0) r = 0;
    const float z = 0.5f;
    float k = r / h;
    u32 midTop = ui_mix(top, bot, k);
    u32 midBot = ui_mix(top, bot, 1 - k);
    C2D_DrawRectangle(x + r, y, z, w - 2 * r, r, top, top, midTop, midTop);
    C2D_DrawRectangle(x, y + r, z, w, h - 2 * r, midTop, midTop, midBot, midBot);
    C2D_DrawRectangle(x + r, y + h - r, z, w - 2 * r, r, midBot, midBot, bot, bot);
    if (r > 0.5f) {
        u32 cTop = ui_mix(top, midTop, 0.5f);
        u32 cBot = ui_mix(midBot, bot, 0.5f);
        C2D_DrawCircleSolid(x + r, y + r, z, r, cTop);
        C2D_DrawCircleSolid(x + w - r, y + r, z, r, cTop);
        C2D_DrawCircleSolid(x + r, y + h - r, z, r, cBot);
        C2D_DrawCircleSolid(x + w - r, y + h - r, z, r, cBot);
    }
}

void ui_background(float w, float h, bool top)
{
    const Theme *t = &s_themes[s_theme];
    u32 a = top ? t->top0 : t->bot0;
    u32 b = top ? t->top1 : t->bot1;
    // Dégradé diagonal léger
    C2D_DrawRectangle(0, 0, 0.1f, w, h, a, ui_mix(a, b, 0.5f), ui_mix(a, b, 0.5f), b);
    if (top) {
        for (float y = 8; y < h; y += 8)
            for (float x = 8; x < w; x += 8)
                C2D_DrawRectSolid(x, y, 0.12f, 1, 1, t->dots);
    }
}

void ui_selection_ring(float x, float y, float w, float h, float r)
{
    ui_rrect(x - 5, y - 5, w + 10, h + 10, r + 5, C_CYAN_SOFT);
    ui_rrect(x - 3, y - 3, w + 6, h + 6, r + 3, C_CYAN);
    ui_rrect(x - 1.5f, y - 1.5f, w + 3, h + 3, r + 1.5f, C_WHITE);
}

void ui_card(float x, float y, float w, float h, float r, bool selected)
{
    if (selected) ui_selection_ring(x, y, w, h, r);
    else ui_rrect(x - 1, y - 1, w + 2, h + 2, r + 1, C_LINE);
    ui_rrect(x, y, w, h, r, C_WHITE);
}

void ui_bar(float x, float y, float w, float h)
{
    C2D_DrawRectSolid(x, y, 0.4f, w, h, C_WHITE);
    C2D_DrawRectSolid(x, y, 0.45f, w, 1, C_LINE);
    C2D_DrawRectSolid(x, y + h - 1, 0.45f, w, 1, C_LINE);
}

void ui_pill_dark(float x, float y, float w, float h, const char *label)
{
    ui_rrect(x, y, w, h, h / 2, C_INK);
    float s = 0.55f;
    float tw = ui_text_width(s, label);
    ui_text(x + (w - tw) / 2, y + h / 2 - 9, s, C_WHITE, 0, label);
}

void ui_soft_tile(float x, float y, float w, float h, float r, u32 c, bool selected)
{
    if (selected) ui_selection_ring(x, y, w, h, r);
    ui_rrect(x, y, w, h, r, c);
}

void ui_icon(size_t idx, float x, float y, float size)
{
    if (!s_sheet) return;
    C2D_Image img = C2D_SpriteSheetGetImage(s_sheet, idx);
    float s = size / 48.0f;
    C2D_DrawImageAt(img, x, y, 0.8f, NULL, s, s);
}

void ui_toggle(float x, float y, bool on)
{
    ui_rrect(x, y, 34, 20, 10, on ? C_CYAN : COL(0xD5, 0xD8, 0xE3, 0xFF));
    C2D_DrawCircleSolid(on ? x + 24 : x + 10, y + 10, 0.6f, 7.5f, C_WHITE);
}

void ui_progress(float x, float y, float w, float h, float frac)
{
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    ui_rrect(x, y, w, h, h / 2, COL(0xE6, 0xE7, 0xF0, 0xFF));
    float fw = w * frac;
    if (fw >= h) ui_rrect(x, y, fw, h, h / 2, C_CYAN);
}
