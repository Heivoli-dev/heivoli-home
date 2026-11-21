# Heivoli — système + installateur (v0.3)

Heivoli se compose de deux programmes :

- **Heivoli Installer** (`installer/`, CIA) : l'appli qu'on installe avec FBI. Elle contient le système
  Heivoli et sait l'**installer**, choisir le **démarrage** (Heivoli ou menu d'origine), le **mettre à jour**
  par Wi-Fi, le **désinstaller** et **redémarrer** la console.
- **Le système Heivoli** (`source/`) : le menu lui-même. L'installateur le place en `sdmc:/boot.3dsx`
  et règle Luma (`autoboot_mode = 3ds` dans `sdmc:/luma/config.ini`) pour démarrer directement dessus.
  Son bouton **Luma** (en haut de l'écran du bas, deux appuis) remet le démarrage d'origine et redémarre.

À la première installation, ton `boot.3dsx` actuel (Homebrew Menu) est gardé dans
`sdmc:/heivoli/backup/` ; la désinstallation le remet. La configuration de Luma d'origine est aussi
sauvegardée dans `sdmc:/heivoli/backup/luma-config.ini`.

**Limite de cette étape** : en démarrage direct, le menu HOME d'origine ne tourne pas derrière ;
appuyer sur HOME dans un jeu peut éteindre la console au lieu de revenir dans Heivoli.
L'étape suivante (remplacer le menu HOME) règle ça.

## Mises à jour par Wi-Fi (GitHub)

Heivoli et Heivoli Installer lisent leurs mises à jour dans le dossier `releases/` de ce dépôt :

- Stable : `releases/latest.txt`
- Bêta : `releases/latest-beta.txt`
- Fichiers de chaque version : `releases/vX.Y.Z/` (`heivoli-home.3dsx`, `heivoli-installer.3dsx`, `.cia` si dispo)

Pour publier une version :
1. Augmente `VERSION_MAJOR/MINOR/MICRO` dans `Makefile` **et** `installer/Makefile`, puis `bash compiler-mac.sh`.
2. Copie les fichiers compilés dans `releases/vX.Y.Z/`.
3. `python3 tools/make_manifest.py X.Y.Z github "Ce qui change"` puis copie `server/latest.txt` en `releases/latest.txt`.
4. Commit + push : les consoles trouvent la nouvelle version au prochain démarrage.

---

## Compiler (Mac)

```
cd ~/Desktop/heivoli-home
bash compiler-mac.sh
```

Le script compile le système, l'intègre dans l'installateur, compile l'installateur puis fait
`installer/heivoli-installer.cia` si **makerom** et **bannertool** sont dans `tools/bin/`.
Sans eux, teste avec `installer/heivoli-installer.3dsx` depuis le Homebrew Launcher.

---


Le menu 3DS de Heivoli Network : style menus système Wii U / 3DS, fonds pastel,
accueil en cartes + dock d'icônes, comme sur la maquette du canvas.

## Nouveautés de la v0.2

- **Nouvel accueil** : 3 grandes cartes (Reprendre, Bibliothèque, Communauté) + dock de 6 icônes rondes au trait fin.
- **Carte « Reprendre »** : relance le dernier jeu 3DS lancé depuis Heivoli, avec son icône.
- **Écran Réglages** : thème pastel (Lavande, Ciel, Menthe, Pêche), sons, démarrage automatique,
  mises à jour auto, canal Stable/Bêta, compte, actualiser la bibliothèque, rechercher une MAJ,
  mode secours, à propos. Tout est enregistré dans `sdmc:/heivoli/settings.ini`.
- **Version CIA** : `make cia` produit `heivoli-home.cia`, installable comme un vrai titre.
- **Canal Bêta** : deux fichiers de mise à jour possibles sur le serveur (`latest.txt` et `latest-beta.txt`).

## Ce qui fonctionne / ce qui reste

| Fonction | État |
|---|---|
| Accueil cartes + dock, croix + tactile, barre d'état (logo, heure, batterie) | ✅ |
| Bibliothèque : 3DS, Homebrew, DS, GBA, GBC, GB, SNES, NES | ✅ |
| Jeux 3DS installés (SD + cartouche) avec nom et icône, lancement | ✅ |
| Homebrews `.3dsx` et ROMs listés | ✅ listés, lancement à venir |
| Communauté → Miiverse (redirigé vers Juxt par Nimbus) | ⚠️ à tester sur console |
| En ligne → ouvre Nimbus | ✅ si Nimbus est installé en CIA |
| Réglages | ✅ (sons et compte : interrupteurs prêts, fonctions à venir) |
| Mise à jour Wi-Fi (CIA et .3dsx), SHA-256, mode secours (L+R au démarrage) | ✅ |
| Démarrage automatique à l'allumage | voir plus bas |

## Compiler

Ce qu'il te faut (tu as peut-être déjà une partie) :

1. **devkitPro** avec le groupe `3ds-dev` → <https://devkitpro.org/wiki/Getting_Started>
2. **curl pour 3DS** : `dkp-pacman -S 3ds-curl` (installe aussi mbedtls et zlib)
3. Pour le CIA : **makerom** (projet *Project_CTR*) et **bannertool**, à télécharger sur leurs pages GitHub
   et à mettre dans ton PATH (ou indiquer leur chemin : `make cia MAKEROM=... BANNERTOOL=...`).
4. Le fichier `https://curl.se/ca/cacert.pem` dans `romfs/cacert.pem`.
5. L'adresse de ton serveur dans `source/config.h` (`UPDATE_BASE_URL`).

Puis :

```
make        # -> heivoli-home.3dsx (pratique pour tester vite)
make cia    # -> heivoli-home.cia  (la vraie version, à installer avec FBI)
```

> Le code n'a pas encore été compilé : la première compilation peut remonter quelques
> erreurs selon ta version de libctru, ou des remarques de makerom sur `app.rsf`.
> Envoie-moi la sortie de `make`, je corrige. La logique de mise à jour et des réglages
> a été testée sur PC.

## Installer

Copie `heivoli-home.cia` sur la carte SD, installe-le avec **FBI**, et il apparaît dans le menu HOME.

## Démarrage automatique (comme le système)

La 3DS démarre toujours sur le **menu HOME de Nintendo** : un CIA seul ne peut pas prendre sa place.
Deux façons d'y arriver :

**Option A — tout de suite, via Luma (simple, avec des limites)**
Luma3DS sait démarrer directement sur une appli à l'allumage (`autoboot_mode = 3ds` dans
`sdmc:/luma/config.ini`), mais ce mode lance en réalité le fichier `sdmc:/boot.3dsx`
(normalement le Homebrew Menu). Pour démarrer sur Heivoli : garde une copie de ton `boot.3dsx`,
remplace-le par `heivoli-home.3dsx` renommé, puis active l'autoboot dans la configuration de Luma
(maintenir SELECT à l'allumage).
Limite importante : dans ce mode, le menu HOME ne tourne pas derrière. Appuyer sur HOME dans un jeu
ou le quitter peut éteindre la console au lieu de revenir dans Heivoli.

**Option B — le vrai objectif : remplacer le menu HOME**
Heivoli devient lui-même le menu HOME (Luma peut charger un code de remplacement pour un titre système).
C'est le seul moyen d'avoir le bouton HOME qui revient dans Heivoli, la mise en veille, les notifications…
C'est un gros chantier : Heivoli devra gérer tout ce que fait le menu HOME. En cas de problème, on
désactive le remplacement depuis le menu de démarrage de Luma et la console retrouve son menu d'origine.

## Publier une mise à jour

1. Augmente `VERSION_MAJOR/MINOR/MICRO` dans le `Makefile`, puis `make && make cia`.
2. Stable : `python3 tools/make_manifest.py 0.2.1 https://ton-site/3ds "Ce qui change"`
   Bêta : ajoute `--beta` (écrit `server/latest-beta.txt`).
3. Envoie sur ton serveur : le fichier `latest*.txt`, `heivoli-home.cia` et `heivoli-home.3dsx`.

La console ne télécharge que si la version est plus récente, et n'installe le fichier que si son
empreinte SHA-256 correspond.

## En cas de souci

Maintiens **L + R** au démarrage (ou Réglages → Mode secours) :
réinstaller la dernière version, revenir à la précédente (.3dsx), ou démarrer normalement.

## Organisation du code

```
source/
  main.c        écrans : accueil, bibliothèque, réglages, mises à jour, secours
  ui.c/.h       style Wii U / 3DS pastel : cartes, sélection cyan, icônes, interrupteurs
  settings.c/.h réglages enregistrés sur la carte SD
  library.c/.h  jeux 3DS (AM + SMDH), homebrews (.3dsx), dossiers de ROMs
  launcher.c/.h lancement d'un jeu 3DS, ouverture de Miiverse
  updater.c/.h  manifeste, téléchargement, vérification, installation CIA/.3dsx, restauration
  net.c/.h      Wi-Fi + HTTPS (libcurl)
  sha256.c/.h   empreinte des fichiers
  config.h      URL du serveur, dossiers, identifiant du titre
gfx/            logo + icônes au trait (atlas tex3ds)
assets/         bannière et son du CIA
app.rsf         description du titre CIA (UniqueId 0xFE1F0)
tools/make_manifest.py   génère latest.txt / latest-beta.txt
```

## Prochaines étapes

- Choisir l'option de démarrage automatique (A ou B) et l'intégrer.
- Lancer les homebrews (service `hb:ldr` de Luma) et les ROMs (cœurs RetroArch 3DS, lanceur DS).
- Bannières/jaquettes officielles pour les ROMs.
- Compte Heivoli (QR code + e-mail), amis, galerie des captures Rosalina, sons du menu.
