#pragma once

// Version de cette build : définie dans le Makefile (VERSION_MAJOR/MINOR/MICRO).
#ifndef APP_VERSION
#define APP_VERSION "0.3.1"
#endif

// Mises à jour publiées dans le dossier releases/ du dépôt GitHub Heivoli-dev/heivoli-home.
// - Stable : releases/latest.txt
// - Bêta   : releases/latest-beta.txt
// Chaque version a son sous-dossier : releases/vX.Y.Z/
#define UPDATE_BASE_URL        "https://raw.githubusercontent.com/Heivoli-dev/heivoli-home/main/releases"
#define UPDATE_MANIFEST_STABLE UPDATE_BASE_URL "/latest.txt"
#define UPDATE_MANIFEST_BETA   UPDATE_BASE_URL "/latest-beta.txt"

// Certificats racine pour le HTTPS (fichier cacert.pem à mettre dans romfs/).
#define CA_BUNDLE "romfs:/cacert.pem"

// Dossiers sur la carte SD, créés au premier lancement.
#define HEIVOLI_ROOT  "sdmc:/heivoli"
#define ROMS_ROOT     HEIVOLI_ROOT "/roms"
#define TMP_DIR       HEIVOLI_ROOT "/tmp"
#define SETTINGS_PATH HEIVOLI_ROOT "/settings.ini"

// Emplacement du système Heivoli, installé par Heivoli Installer (Luma démarre dessus).
#define DEFAULT_3DSX_PATH "sdmc:/boot.3dsx"

// Identifiant du titre CIA (doit correspondre à UniqueId dans app.rsf).
#define HEIVOLI_TITLE_ID 0x000400000FE1F000ULL

#define USER_AGENT "HeivoliHome/" APP_VERSION " (Nintendo 3DS)"
