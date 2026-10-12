# Testy

| Rodzaj | Gdzie | Uruchamianie | Wymaga |
|---|---|---|---|
| Jednostkowe (doctest) | `tests/unit/` | `ctest --preset <p>` | nic |
| Determinizm scenariuszy | `tests/conformance/` | CTest `conformance.determinism.*` | nic |
| Regresja scenariuszy | `tests/conformance/golden/` | CTest `conformance.regression.*` | nic |
| Dane oryginalnej gry | testy oznaczone `[gamedata]` | `G2N_GAME_DIR=… ctest` | własna instalacja GTA 2 |
| Benchmark CPU/pamięci | `g2n-headless bench` | CI (Release) → podsumowanie zadania | nic |
| Czas klatki | `gta2n --frames N` | lokalnie | okno/GPU |
| Render bez okna | `gta2n --offscreen WxH --screenshot f.bmp` | CI (lavapipe, próbne) / lokalnie | Vulkan/Metal/D3D12 |
| Infrastruktura gfx-compat | `tests/gfx/windows/tests` (Pester), `tests/scripts` (Bash) | `infra-lint` | nic (atrapa `aws`) |

Sanitizery: preset `asan` (ASan + UBSan, `-fno-sanitize-recover`) w CI na Linux i macOS.

## Framework porównawczy

Przepływ: **scenariusz JSON → replay wejścia na stałym kroku → ślad stanu co tick →
porównanie z tolerancjami → raport z pierwszą rozbieżnością**.

### Scenariusz (`schemaVersion: 1`)

```jsonc
{
  "id": "synthetic-wall-slide",          // syntetyczne MUSZĄ mieć prefiks synthetic-
  "provenance": "synthetic",             // synthetic | originalGta2 | engine
  "simulation": { "tickRateHz": 30, "ticks": 240, "seed": 7 },
  "world": { "map": "data/wil.gmp", "spawn": { "x": 100.5, "y": 104.5 } },  // map opcjonalna
  "inputs": [ { "fromTick": 0, "toTick": 239, "buttons": ["forward"] } ],
  "capture": { "everyTicks": 4, "kinds": ["player"] },
  "tolerances": { "default": { "absolute": 0 },
                  "kinds": { "player": { "headingUnits": { "absolute": 0, "wrapPeriod": 65536 } } } }
}
```

* Komentarze są dozwolone; nieznane pola i przyciski są błędem.
* Przyciski: `forward`, `backward`, `left`, `right`, `attack`, `jump`, `enterExit`,
  `previousWeapon`, `nextWeapon`, `special`. Zakresy `fromTick..toTick` są włączne,
  nakładające się sumują.
* `world.map` jest względna wobec katalogu gry. Bez niej używana jest **syntetyczna** mapa testowa.
* Tolerancja: `|e−a| ≤ absolute` lub `≤ relative·|e|`; `wrapPeriod` liczy najkrótszą drogę po
  okręgu; `NaN` zgadza się tylko z `NaN`. Domyślnie równość dokładna.

### Ślad (JSONL, kanoniczny)

Linia 1: `{"header":{…}}`, potem `{"tick":N,"entities":[{"id","kind","properties":{…}}]}`.
`tick: N` to stan po N tickach (0 = początkowy). Klucze są posortowane, liczby zapisane
najkrótszą postacią odtwarzalną bit w bit (`std::to_chars`), wartości nieskończone jako
`"nan"`/`"inf"`/`"-inf"`, bez znaczników czasu. Ten sam przebieg daje identyczny plik.

### Raport

`g2n-headless compare` zwraca kod 0 (zgodne), 1 (rozbieżne) lub 2 (błąd wejścia) i wypisuje
Markdown, opcjonalnie też JSON. Raport zawiera: pierwszą rozbieżność (nagłówek, potem
najniższy tick / encja / właściwość), statystyki na (rodzaj, właściwość) — maks.
odchylenie z tickiem, pierwszy tick poza tolerancją — oraz listę rozbieżności
(`HeaderMismatch`, `MissingTick`, `UnexpectedTick`, `MissingEntity`, `UnexpectedEntity`,
`KindMismatch`, `MissingProperty`, `UnexpectedProperty`, `ValueOutOfTolerance`).

### Wzorce regresyjne

`tests/conformance/golden/*.golden.jsonl` generuje `g2n-headless run`. Zmiana zachowania
symulacji wymaga świadomej regeneracji i podniesienia `version()` adaptera `engine`.
Obecne wzorce pochodzą z **prototypowych** reguł ruchu na **syntetycznej** mapie —
nie mówią nic o GTA 2.

### Dane referencyjne z oryginału

Ślady `provenance: originalGta2` mają powstawać z oryginalnej gry uruchamianej na EC2
([aws/integration.md](aws/integration.md)). Do czasu ich nagrania w repozytorium nie ma
żadnych danych referencyjnych GTA 2.
