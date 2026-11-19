#include "launcher.h"
#include <string.h>

// Lancement d'un autre titre (jeu, Nimbus, homebrew CIA...).
// On utilise le "chainloader" de libctru : il programme le saut, qui est effectué
// par libctru au bon moment, pendant la fermeture de Heivoli (aptExit).
// L'ancienne méthode (APT_DoApplicationJump appelé à la main après avoir tout fermé)
// donnait un écran noir sur 2DS.

static bool s_pending = false;

Result launcher_prepare_title(u64 titleId, u8 mediatype)
{
    aptSetChainloader(titleId, mediatype);
    s_pending = true;
    return 0;
}

void launcher_commit_jump(void)
{
    // Rien à faire : libctru effectue le saut en quittant l'appli.
    s_pending = false;
}

void launcher_open_miiverse(void)
{
    // À TESTER sur console : Miiverse est un applet système.
    aptLaunchLibraryApplet(APPID_MIIVERSE, NULL, 0, 0);
}
