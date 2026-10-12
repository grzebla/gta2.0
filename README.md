# GTA2.0

Natywna, wieloplatformowa reimplementacja **GTA 2** napisana od podstaw w **C++20**, na
**SDL3** i **SDL_GPU** (Metal na macOS, Direct3D 12 na Windows, Vulkan na Linux).
Celem jest wysoka zgodność z oryginałem, mierzona automatycznymi testami porównawczymi,
oraz przewidywalna, deterministyczna symulacja.

> Repozytorium nie zawiera i nie rozprowadza żadnych plików gry. Do uruchomienia z
> prawdziwymi danymi potrzebna jest własna, legalnie posiadana instalacja GTA 2.

## Stan (2026-10-12)

Działający pionowy wycinek, bez mechanik gry:
- natywne okno SDL3 i renderer SDL_GPU (instancjonowane quady z atlasem tekstur), kamera 2D
  śledząca gracza i swobodna;
- parsery oryginalnych map `.gmp` i stylów `.sty`, zweryfikowane na wszystkich 17 mapach i
  4 stylach GTA 2; render pokryw bloków Downtown/Residential/Industrial z oryginalnymi
  teksturami (ściany 3D, skosy i oświetlenie — w toku, milestone M1);
- stały krok symulacji 30 Hz, deterministyczna symulacja na liczbach stałoprzecinkowych,
  uruchamiana także bez okna i GPU;
- framework testów porównawczych: scenariusze JSON, replay wejścia, ślady JSONL.

Reguły ruchu są **syntetyczne** (prototyp), nie odwzorowują GTA 2.
Plan: [docs/roadmap.md](docs/roadmap.md), stan mechanik: [docs/mechanics-matrix.md](docs/mechanics-matrix.md).
Rozwój prowadzi para agentów pod kontrolą Orchestratora: [tools/orchestrator/README.md](tools/orchestrator/README.md).

## Szybki start

```bash
cmake --preset release && cmake --build --preset release && ctest --preset release
./build/release/bin/gta2                          # strzałki/WASD, kółko myszy = zoom, Esc = wyjście
./build/release/bin/gta2 --game-dir ~/Games/GTA2 --level data/wil.gmp   # z własnymi danymi gry
./build/release/bin/gta20-headless run tests/conformance/scenarios/synthetic-wall-slide.json --out trace.jsonl
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
| `engine/testing`, `apps/gta20-headless` | framework porównawczy i runner bez okna |
| `apps/gta2` | aplikacja gry |
| `tests/` | testy jednostkowe, scenariusze i wzorce, testy infrastruktury GPU |
| `infra/aws`, `scripts/aws`, `tests/gfx` | oryginalne GTA 2 na EC2 G4dn ([docs/aws/integration.md](docs/aws/integration.md)) |

## Źródła wiedzy o oryginale

[GTA2_RE](https://github.com/CriminalRETeam/gta2_re) (bez licencji) służy wyłącznie do
analizy zachowania — kod nie jest kopiowany ([docs/reference/gta2re.md](docs/reference/gta2re.md)).
Formaty plików opisujemy na podstawie opublikowanych dokumentów DMA Design i pomiarów.

## Licencja

Nie wybrano jeszcze licencji dla kodu projektu (decyzja właściciela). GTA 2 jest znakiem
towarowym i własnością jej właścicieli; projekt nie jest z nimi powiązany.
