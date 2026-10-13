# Analiza oryginalnych map (tylko lokalnie, tylko agregaty)

Skrypty czytają mapy `.gmp` z `GTA20_GAME_DIR` wyłącznie do odczytu i wypisują same liczby i procenty
— bez rekordów bloków, współrzędnych, kafli i pikseli. Ich wyników nie commitujemy; do repo trafiają
tylko wybrane liczby w specyfikacjach, z odnośnikiem do polecenia.

| Skrypt | Wynik | Używa |
|---|---|---|
| `slope_aggregates.py <game-dir> [--inventory FILE]` | dla każdego kodu skosu 0–63: słowa ścian, bity `flat`, pokrywy 1023, klasy sąsiadów; z `--inventory` porównuje liczby komórek z raportem T0010 (kod 1 przy niezgodności) | [docs/specs/world-slopes.md](../../docs/specs/world-slopes.md) sekcja 1.1 |
| `check_slope_table.py <world-slopes.md> [inventory.txt]` | tabela z sekcji 3 ma każdy kod 0–63 dokładnie raz; z raportem T0010 — liczby komórek, map i miast zgadzają się z inwentaryzacją | sekcja 8 tego dokumentu |

```sh
GTA20_GAME_DIR=~/Games/GTA2 ./build/debug/bin/gta20-headless inventory > "$TMPDIR/inventory.txt"
python3 -I tools/map-analysis/slope_aggregates.py "$GTA20_GAME_DIR" --inventory "$TMPDIR/inventory.txt"
python3 -I tools/map-analysis/check_slope_table.py docs/specs/world-slopes.md "$TMPDIR/inventory.txt"
```
