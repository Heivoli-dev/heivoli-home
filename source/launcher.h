#pragma once
#include <3ds.h>
#include <stdbool.h>

// Prépare le saut vers un jeu/appli 3DS installé. Si ça réussit, l'appli
// doit quitter sa boucle principale : le système lance alors le titre.
Result launcher_prepare_title(u64 titleId, u8 mediatype);
void launcher_commit_jump(void);

// Ouvre l'appli Miiverse du système (redirigée vers Juxt par Nimbus/Pretendo).
void launcher_open_miiverse(void);
