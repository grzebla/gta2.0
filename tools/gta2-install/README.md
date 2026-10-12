# Przygotowanie lokalnej instalacji GTA 2 (bez uruchamiania instalatorów)

Pliki gry są tylko do lokalnego użytku: **nigdy** nie trafiają do Git, CI ani publicznych miejsc.
Skrypty z tego katalogu niczego nie uruchamiają; tylko czytają archiwa.

## Wydanie 9.6 (Rockstar, 2004) — kompletne zasoby

`GTA2.exe` (362 331 961 B, SHA-256 `0cd38049…b083a0`) to instalator **InstallShield „Setup Player”**
(single-file EXE z osadzonymi `data1.hdr`, `data1.cab`, `data2.cab`).

```bash
W=~/Games/GTA2-work
python3 -I tools/gta2-install/carve_isplayer.py ~/Downloads/GTA2INSTALLER/GTA2.exe "$W"   # 15 plików Disk1/*
brew install unshield                                                                     # 1.6.2
unshield -d "$W/extracted" -g "App Executables" x "$W/Disk1/data1.hdr"                     # 344 pliki
mv "$W/extracted/App_Executables" ~/Games/GTA2
python3 -I tools/gta2-install/manifest.py ~/Games/GTA2 "$W/manifest.json"
```

Wynik: `gta2.exe` 9.6.0.0 (PE 2004-04-27), 17 map `.gmp`, 4 style `.sty`, 109 `.scr`, 30 `.mmp`,
4 `.gxt`, 4 pary `.sdt/.raw`, 119 `.wav` (radio), 26 `.tga`, `intro.bik` — 344 pliki, 384,5 MiB.

## Wydanie 10.5 (Freeloader, 2005) — binarka referencyjna

`gta2freeloader.exe` to **sam plik wykonywalny gry 10.5** (`FileVersion 10.5.0.0`, PE 2001-01-29,
OriginalFilename `gta2.exe`) — wersja, na której opiera się GTA2_RE. `35_53/54/55.exe` to
samorozpakowujące się archiwa DynaZIP z instalatorem „Active Delivery”; w środku są zwykłe
pliki Microsoft CAB z grą (`freeloader.dat` = `gta2freeloader.exe`).

```bash
W=~/Games/GTA2-work/freeloader
for f in 35_53 35_54 35_55; do unzip -q -o ~/Downloads/gta2freeloader/$f.exe -d "$W/$f"; done
for c in "$W"/*/ic*.cab; do 7z x -y -o"$W/tree" "$c"; done                                 # 341 plików
mv "$W/tree" ~/Games/GTA2-10.5
```

## Zgodność danych 9.6 ↔ 10.5 (zmierzona 2026-10-12)

| Grupa | Wynik |
|---|---|
| 17 × `.gmp`, 4 × `.sty`, `.sdt/.raw`, `e.gxt`, 104 × `.scr`, 97 × `.wav`, `.tga`, `.gci`, `.seq` | **identyczne bajtowo** |
| `.mmp` wieloosobowe | różnią się tylko opisem („2player” → „2 player”) |
| `res-*.scr` (multiplayer „Residential”) | inna treść, ten sam rozmiar i format |
| `d3ddll.dll`, `3dfx.dll`, `gta2 manager.exe`, `readme.txt` | różne |
| tylko 9.6 | 22 × `.wav` (muzyka), `intro.bik`, `MSSDS3DH.M3D`, `rockstar.ico` |
| tylko 10.5 | teksty f/g/i (`.gxt`), `softdll.dll`, `player/*`, pliki launchera Freeloader |

Wniosek: formaty map i grafiki są w pełni zgodne (to te same pliki). Silnik używa danych 9.6
(`G2N_GAME_DIR=~/Games/GTA2`); 10.5 służy jako binarka referencyjna (np. testy na EC2 zgodne z
adresami GTA2_RE). Test: `G2N_GAME_DIR_105=~/Games/GTA2-10.5` włącza porównanie w `[gamedata]`.
