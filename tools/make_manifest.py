#!/usr/bin/env python3
"""Génère server/latest.txt avec les empreintes SHA-256 des fichiers compilés.

Usage :
    python3 tools/make_manifest.py 0.3.1 github "Notes de version"     (Releases GitHub)
    python3 tools/make_manifest.py 0.2.0 https://ton-site/3ds "Notes de version"
    python3 tools/make_manifest.py 0.3.0 https://ton-site/3ds "Notes" --beta   (canal Bêta)

Cherche heivoli-home.3dsx et heivoli-home.cia à la racine du projet.
Envoie ensuite latest.txt, le .3dsx et le .cia sur ton serveur.
"""
import hashlib
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parent.parent


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    beta = "--beta" in sys.argv
    sys.argv = [a for a in sys.argv if a != "--beta"]
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    version, base = sys.argv[1], sys.argv[2].rstrip("/")
    if base == "github":
        base = "https://github.com/Heivoli-dev/heivoli-home/releases/download/" + ("beta" if beta else "v" + version)
    notes = sys.argv[3] if len(sys.argv) > 3 else ""
    lines = ["version=" + version]
    for ext in ("3dsx", "cia"):
        f = root / ("heivoli-home." + ext)
        if f.exists():
            lines.append(f"{ext}={base}/{f.name}")
            lines.append(f"sha256_{ext}={sha(f)}")
        else:
            print(f"(pas de {f.name}, ignoré)")
    lines.append("notes=" + notes.replace("\n", " "))
    out = root / "server" / ("latest-beta.txt" if beta else "latest.txt")
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(out.read_text(encoding="utf-8"))


if __name__ == "__main__":
    main()
