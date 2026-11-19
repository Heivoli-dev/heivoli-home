#pragma once
#include <3ds.h>
#include <stdbool.h>

typedef struct {
    int theme;          // 0 Lavande, 1 Ciel, 2 Menthe, 3 Pêche
    bool sounds;
    bool autoboot;      // préférence (voir README : démarrage automatique)
    bool autoUpdate;
    bool beta;          // canal Bêta au lieu de Stable
    u64 lastTitle;      // dernier jeu 3DS lancé (carte "Reprendre")
    u8 lastMedia;
    char lastName[96];
} Settings;

extern Settings g_settings;

void settings_defaults(Settings *s);
// Lit le fichier ; garde les valeurs par défaut pour les clés absentes.
void settings_load(const char *path, Settings *s);
int settings_save(const char *path, const Settings *s);
