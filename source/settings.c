#include "settings.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Settings g_settings;

void settings_defaults(Settings *s)
{
    memset(s, 0, sizeof(*s));
    s->theme = 0;
    s->sounds = true;
    s->autoboot = false;
    s->autoUpdate = true;
    s->beta = false;
}

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static bool as_bool(const char *v)
{
    return !strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "on");
}

void settings_load(const char *path, Settings *s)
{
    settings_defaults(s);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#' || line[0] == ';') continue;
        *eq = '\0';
        char *k = trim(line), *v = trim(eq + 1);
        if (!strcmp(k, "theme")) {
            s->theme = atoi(v);
            if (s->theme < 0 || s->theme > 3) s->theme = 0;
        } else if (!strcmp(k, "sounds")) s->sounds = as_bool(v);
        else if (!strcmp(k, "autoboot")) s->autoboot = as_bool(v);
        else if (!strcmp(k, "auto_update")) s->autoUpdate = as_bool(v);
        else if (!strcmp(k, "beta")) s->beta = as_bool(v);
        else if (!strcmp(k, "last_title")) s->lastTitle = strtoull(v, NULL, 16);
        else if (!strcmp(k, "last_media")) s->lastMedia = (u8)atoi(v);
        else if (!strcmp(k, "last_name")) snprintf(s->lastName, sizeof(s->lastName), "%s", v);
    }
    fclose(f);
}

int settings_save(const char *path, const Settings *s)
{
    char tmp[300];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fprintf(f, "# Réglages Heivoli Home\n");
    fprintf(f, "theme=%d\n", s->theme);
    fprintf(f, "sounds=%d\n", s->sounds ? 1 : 0);
    fprintf(f, "autoboot=%d\n", s->autoboot ? 1 : 0);
    fprintf(f, "auto_update=%d\n", s->autoUpdate ? 1 : 0);
    fprintf(f, "beta=%d\n", s->beta ? 1 : 0);
    fprintf(f, "last_title=%016llX\n", (unsigned long long)s->lastTitle);
    fprintf(f, "last_media=%d\n", s->lastMedia);
    fprintf(f, "last_name=%s\n", s->lastName);
    fclose(f);
    // Écriture atomique : on remplace l'ancien fichier seulement si tout est écrit.
    remove(path);
    return rename(tmp, path) == 0 ? 0 : -1;
}
