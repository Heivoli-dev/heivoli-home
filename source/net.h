#pragma once
#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

// Rappel de progression : retourne false pour annuler le téléchargement.
typedef bool (*net_progress_cb)(u64 now, u64 total, void *user);

bool net_init(void);
void net_exit(void);
bool net_wifi_connected(void);

// Télécharge une ressource en mémoire (taille max maxlen). *out est terminé par '\0'.
// À libérer avec free(). Retourne 0 si OK.
int net_get_mem(const char *url, char **out, size_t *outlen, size_t maxlen);

// Télécharge vers un fichier. Retourne 0 si OK, 1 si annulé, <0 si erreur.
int net_download_file(const char *url, const char *path, net_progress_cb cb, void *user);

const char *net_last_error(void);
