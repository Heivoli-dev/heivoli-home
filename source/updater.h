#pragma once
#include "net.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char version[16];
    char url_3dsx[256];
    char url_cia[256];
    char sha_3dsx[65];
    char sha_cia[65];
    char notes[256];
} UpdateInfo;

// Compare deux versions "X.Y.Z". <0 si a<b, 0 si égales, >0 si a>b.
int version_compare(const char *a, const char *b);

// Lit le manifeste sur le serveur. Retourne 1 si une version plus récente existe,
// 0 si à jour, <0 en cas d'erreur (message dans err).
int updater_check(const char *manifest_url, UpdateInfo *info, char *err, size_t errlen);

// Télécharge, vérifie (SHA-256) et installe la mise à jour.
// - Version .3dsx : remplace le fichier en gardant une copie .bak.
// - Version CIA   : installe le CIA via le service AM.
// Retourne 0 si OK, 1 si annulé, <0 si erreur.
int updater_install(const UpdateInfo *info, const char *self_path,
                    net_progress_cb cb, void *user, char *err, size_t errlen);

// Télécharge le .3dsx du manifeste et le place à `dest_path` (copie .bak de l'ancien).
// Utilisé par l'installateur pour mettre à jour le système (sdmc:/boot.3dsx).
int updater_install_3dsx_to(const UpdateInfo *info, const char *dest_path,
                            net_progress_cb cb, void *user, char *err, size_t errlen);

// Remplace `dest_path` par `src_path` en gardant l'ancien en .bak.
int updater_replace_file(const char *src_path, const char *dest_path, char *err, size_t errlen);

// Restaure la copie .bak de la version .3dsx précédente.
int updater_restore_backup(const char *self_path, char *err, size_t errlen);
bool updater_has_backup(const char *self_path);

// true si on tourne en .3dsx (Homebrew Launcher), false si installé en CIA.
bool updater_is_3dsx(void);
