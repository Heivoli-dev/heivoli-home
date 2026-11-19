#!/bin/bash
# Compile Heivoli (le système) puis Heivoli Installer (l'appli CIA qui l'installe).
# Lance-le depuis le Terminal :
#   cd ~/Desktop/heivoli-home && bash compiler-mac.sh
set -e
cd "$(dirname "$0")"
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
export PATH=$DEVKITPRO/tools/bin:$DEVKITPRO/pacman/bin:$PWD/tools/bin:$PATH

if [ ! -d "$DEVKITARM" ]; then
  echo "devkitPro n'est pas installé."
  echo "1) Télécharge et installe : https://github.com/devkitPro/pacman/releases/latest  (fichier .pkg)"
  echo "2) Puis lance : sudo dkp-pacman -S 3ds-dev 3ds-curl"
  exit 1
fi
if [ ! -f "$DEVKITPRO/portlibs/3ds/lib/libcurl.a" ]; then
  echo "Installation de curl pour 3DS..."
  sudo dkp-pacman -S --needed --noconfirm 3ds-curl
fi
if [ ! -f romfs/cacert.pem ]; then
  echo "Téléchargement des certificats (cacert.pem)..."
  curl -fsSL -o romfs/cacert.pem https://curl.se/ca/cacert.pem
fi

VERSION=$(grep -E '^VERSION_(MAJOR|MINOR|MICRO)' Makefile | awk '{print $3}' | paste -sd. -)

echo "== 1/2 : système Heivoli $VERSION =="
make
echo "OK : heivoli-home.3dsx"

echo "== 2/2 : Heivoli Installer =="
mkdir -p installer/romfs
cp heivoli-home.3dsx installer/romfs/heivoli-system.3dsx
echo "$VERSION" > installer/romfs/heivoli-system.ver
cp romfs/cacert.pem installer/romfs/cacert.pem
make -C installer
echo "OK : installer/heivoli-installer.3dsx"

if command -v makerom >/dev/null && command -v bannertool >/dev/null; then
  make -C installer cia
  echo ""
  echo "Terminé : installe installer/heivoli-installer.cia avec FBI."
else
  echo ""
  echo "Pour le CIA de l'installateur, mets makerom et bannertool (versions macOS)"
  echo "dans heivoli-home/tools/bin/ puis relance ce script."
  echo "En attendant, tu peux tester avec installer/heivoli-installer.3dsx (dossier 3ds de la carte SD)."
fi
