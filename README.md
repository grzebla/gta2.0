# gta2-native

Natywna, wieloplatformowa reimplementacja **GTA 2** napisana od podstaw w **C++20**, na
**SDL3** i **SDL_GPU** (Metal na macOS, Direct3D 12 na Windows, Vulkan na Linux).
Celem jest wysoka zgodność z oryginałem, mierzona automatycznymi testami porównawczymi,
oraz przewidywalna, deterministyczna symulacja.

> Repozytorium nie zawiera i nie rozprowadza żadnych plików gry. Do uruchomienia z
> prawdziwymi danymi potrzebna jest własna, legalnie posiadana instalacja GTA 2.

## Stan (2026-10-12)

Działający pionowy wycinek, bez mechanik gry:
- natywne okno SDL3 i renderer SDL_GPU (jedno instancjonowane wywołanie rysowania), kamera 2D;
- stały krok symulacji 30 Hz z interpolowanym renderem;
- deterministyczna symulacja na liczbach stałoprzecinkowych, uruchamiana także bez okna i GPU;
- parser map `.gmp` (DMAP/CMAP/UMAP, strefy) wg dokumentu formatu DMA Design;
- framework testów porównawczych: scenariusze JSON, replay wejścia, ślady JSONL, porównanie z
  tolerancjami i raport pierwszej rozbieżności.

Reguły ruchu i mapa demonstracyjna są **syntetyczne** (prototyp), nie odwzorowują GTA 2.
Plan: [docs/roadmap.md](docs/roadmap.md), stan mechanik: [docs/mechanics-matrix.md](docs/mechanics-matrix.md).

## Szybki start

```bash
cmake --preset release && cmake --build --preset release && ctest --preset release
./build/release/bin/gta2n                          # strzałki/WASD, kółko myszy = zoom, Esc = wyjście
./build/release/bin/gta2n --game-dir ~/GTA2 --level data/wil.gmp   # z własnymi danymi gry
./build/release/bin/g2n-headless run tests/conformance/scenarios/synthetic-wall-slide.json --out trace.jsonl
```

Szczegóły: [docs/building.md](docs/building.md) · [docs/testing.md](docs/testing.md) ·
[docs/architecture.md](docs/architecture.md).

## Struktura

| Katalog | Zawartość |
|---|---|
| `engine/core` | typy, fixed point, kąty, RNG, zegar, identyfikatory, kamera |
| `engine/assets` | formaty plików GTA 2 ([docs/formats](docs/formats/README.md)) |
| `engine/sim` | symulacja (bez SDL/OS/GPU) |
| `engine/scene`, `engine/render` | listy rysowania i renderer SDL_GPU |
| `engine/platform` | SDL3: okno, wejście |
| `engine/testing`, `apps/g2n-headless` | framework porównawczy i runner bez okna |
| `apps/gta2n` | aplikacja gry |
| `tests/` | testy jednostkowe, scenariusze i wzorce, testy infrastruktury GPU |
| `infra/aws`, `scripts/aws`, `tests/gfx` | oryginalne GTA 2 na EC2 G4dn ([docs/aws/integration.md](docs/aws/integration.md)) |

## Źródła wiedzy o oryginale

[GTA2_RE](https://github.com/CriminalRETeam/gta2_re) (bez licencji) służy wyłącznie do
analizy zachowania — kod nie jest kopiowany ([docs/reference/gta2re.md](docs/reference/gta2re.md)).
Formaty plików opisujemy na podstawie opublikowanych dokumentów DMA Design i pomiarów.

## Licencja

Nie wybrano jeszcze licencji dla kodu projektu (decyzja właściciela). GTA 2 jest znakiem
towarowym i własnością jej właścicieli; projekt nie jest z nimi powiązany.
