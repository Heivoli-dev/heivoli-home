# Bannière 3D « diorama » — appli de captures

Une petite 3DS ouverte : une capture sur l'écran du haut, un QR code qui s'envole de
l'écran du bas vers un téléphone qui flotte à côté. Boucle de 4 s + jingle (clic-clac et 4 notes).

- `make_banner.py` : génère la scène et l'animation (`banner.gltf` + `banner.bin`)
- `textures/` : textures de la scène (petites, puissances de 2)
- `jingle.wav` : son de la bannière (2,2 s, la limite est 3 s)
- `preview.py` : aperçu approximatif (`preview.gif`), pas le rendu exact du menu HOME
- `build-banner.sh` : sur Mac, convertit avec pycgfx et produit `banner.bnr`

La bannière ne s'affiche qu'avec une appli installée en CIA dont le SMDH a le drapeau
`extendedbanner`.
