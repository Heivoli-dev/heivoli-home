#pragma once
#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

// Configuration de Luma3DS (v13 et plus récent : fichier texte config.ini).
#define LUMA_CONFIG_PATH  "sdmc:/luma/config.ini"
// Emplacement que Luma lance au démarrage en mode autoboot "3ds" : c'est là
// que l'installateur place le système Heivoli.
#define SYSTEM_BOOT_PATH  "sdmc:/boot.3dsx"

bool luma_config_present(void);

// Lit la valeur d'une clé ("autoboot_mode", ...). 0 si trouvée.
int luma_config_get(const char *key, char *out, size_t cap);

// Remplace la valeur d'une clé existante, en gardant tout le reste du fichier.
// Une copie de sécurité du fichier d'origine est faite la première fois.
// 0 si OK, -1 si la clé est absente, -2 en cas d'erreur d'écriture.
int luma_config_set(const char *key, const char *value);

// Démarrage direct sur Heivoli (autoboot_mode = 3ds) ou menu d'origine (off).
bool luma_autoboot_enabled(void);
int luma_set_autoboot(bool on);

// Redémarre la console (prise en compte de la nouvelle configuration de Luma).
void system_reboot(void);
