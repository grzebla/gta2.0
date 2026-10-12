#!/usr/bin/env python3
"""Writes a SHA-256 manifest of a local GTA 2 installation (paths, sizes, hashes - no content).

Usage: python3 -I tools/gta2-install/manifest.py <game-dir> <out.json>
"""
import hashlib
import json
import os
import sys


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def main(root, out):
    files = []
    for directory, _, names in os.walk(root):
        for name in names:
            path = os.path.join(directory, name)
            files.append({"path": os.path.relpath(path, root), "size": os.path.getsize(path), "sha256": sha256(path)})
    files.sort(key=lambda f: f["path"].lower())
    with open(out, "w") as f:
        json.dump({"file_count": len(files), "total_bytes": sum(f["size"] for f in files), "files": files}, f, indent=1)
    print(f"{len(files)} files -> {out}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
