# Testy

| Rodzaj | Gdzie | Uruchamianie | Wymaga |
|---|---|---|---|
| Jednostkowe (doctest) | `tests/unit/` | `ctest --preset <p>` | nic |
| Determinizm scenariuszy | `tests/conformance/` | CTest `conformance.determinism.*` | nic |
| Regresja scenariuszy | `tests/conformance/golden/` | CTest `conformance.regression.*` | nic |
| Dane oryginalnej gry | testy oznaczone `[gamedata]` | `GTA20_GAME_DIR=… ctest` | własna instalacja GTA 2 |
| Benchmark CPU/pamięci | `gta20-headless bench` | CI (Release) → podsumowanie zadania | nic |
| Czas klatki | `gta2 --frames N` | lokalnie | okno/GPU |
| Render bez okna | `gta2 --offscreen WxH --screenshot f.bmp` | CI (lavapipe, próbne) / lokalnie | Vulkan/Metal/D3D12 |
| Infrastruktura gfx-compat | `tests/gfx/windows/tests` (Pester), `tests/scripts` (Bash) | `infra-lint` | nic (atrapa `aws`) |

Sanitizery: preset `asan` (ASan + UBSan, `-fno-sanitize-recover`) w CI na Linux i macOS.

## Zrzuty renderu z oryginalnych danych (`gamedata.render.*`)

`gamedata.render.wil`, `.ste` i `.bil` renderują offscreen fragment dzielnicy z oryginalnych
danych (`cmake/RunRenderCheck.cmake`) i sprawdzają tylko, czy obraz nie jest pusty
(≥ 500 kolorów, ≥ 95% pokrycia). To **smoke test**: nie dowodzi wierności M1 i nie zastępuje
obowiązkowej bramki M1 z danymi. Bez `GTA20_GAME_DIR` (ani starego `G2N_GAME_DIR`) albo bez
urządzenia GPU test kończy się jako SKIP, zanim cokolwiek zapisze.

Zrzut pochodzi z oryginalnych danych, więc **nigdy** nie trafia do drzewa źródeł ani do
katalogu budowania (a więc ani do artefaktów CI, które są ścieżkami w checkoutcie:
`build/<preset>/…`, `smoke.bmp`, `gfx-results/`). Trafia do katalogu zewnętrznego:

* `GTA20_RENDER_CAPTURE_DIR` — zmienna cache CMake (absolutna ścieżka). Domyślnie katalog
  cache użytkownika: `%LOCALAPPDATA%\gta20\render-captures` (Windows),
  `$XDG_CACHE_HOME/gta20/render-captures` lub `~/.cache/gta20/render-captures`.
* Zmienna środowiskowa `GTA20_RENDER_CAPTURE_DIR` w czasie `ctest` ma pierwszeństwo przed
  wartością z cache.

Przed uruchomieniem renderera skrypt sprawdza katalog: musi być absolutny i leżeć poza
checkoutem oraz katalogiem budowania — zarówno w zapisanej postaci, jak i po rozwiązaniu
dowiązań symbolicznych na każdym istniejącym poziomie ścieżki. Dowiązanie prowadzące do
checkoutu, dowiązanie wewnątrz checkoutu i wiszące dowiązanie są odrzucane z błędem
wskazującym, co ustawić; nie ma cichego powrotu do katalogu budowania. Każde uruchomienie
tworzy osobny katalog `<root>/<UTC YYYYMMDDTHHMMSSZ>-<dzielnica>-<8 losowych znaków>/` z plikiem
`render-<dzielnica>.bmp` i wypisuje jego ścieżkę (przed renderem i jako
`Render capture saved: …`). Jeśli renderer nic nie zapisał (SKIP z braku GPU, błąd), pusty
katalog uruchomienia jest usuwany; istniejące zrzuty nigdy nie są usuwane ani nadpisywane.

Lokalnie:

```sh
cmake --preset debug -DGTA20_RENDER_CAPTURE_DIR="$HOME/gta20-captures"   # opcjonalnie
cmake --build --preset debug
GTA20_GAME_DIR=~/Games/GTA2 ctest --preset debug -R gamedata.render -V | grep 'Render capture'
# albo jednorazowo: GTA20_RENDER_CAPTURE_DIR=/abs/poza/repo GTA20_GAME_DIR=… ctest …
```

Obrazy przeglądaj lokalnie; nie commituj ich i nie wysyłaj nigdzie.

Obsługę katalogu sprawdzają testy bez danych `rendercheck.capture.*`
(`tests/conformance/rendercheck/`): atrapa renderera w skrypcie CMake i katalogi w
katalogu tymczasowym. Pokrywają akceptowaną ścieżkę zewnętrzną, odrzucenie ścieżek w źródłach i
w katalogu budowania (także przez `..`), ścieżkę ze spacjami, niezależne katalogi kolejnych
uruchomień, dowiązania symboliczne (SKIP, gdy system ich nie pozwala tworzyć), brak lub
względną ścieżkę, nadpisanie zmienną środowiskową, SKIP bez danych i bez GPU, niezmienione
progi obrazu oraz to, że skonfigurowany katalog leży poza bieżącym checkoutem.

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

`gta20-headless compare` zwraca kod 0 (zgodne), 1 (rozbieżne) lub 2 (błąd wejścia) i wypisuje
Markdown, opcjonalnie też JSON. Raport zawiera: pierwszą rozbieżność (nagłówek, potem
najniższy tick / encja / właściwość), statystyki na (rodzaj, właściwość) — maks.
odchylenie z tickiem, pierwszy tick poza tolerancją — oraz listę rozbieżności
(`HeaderMismatch`, `MissingTick`, `UnexpectedTick`, `MissingEntity`, `UnexpectedEntity`,
`KindMismatch`, `MissingProperty`, `UnexpectedProperty`, `ValueOutOfTolerance`).

### Wzorce regresyjne

`tests/conformance/golden/*.golden.jsonl` generuje `gta20-headless run`. Zmiana zachowania
symulacji wymaga świadomej regeneracji i podniesienia `version()` adaptera `engine`.
Obecne wzorce pochodzą z **prototypowych** reguł ruchu na **syntetycznej** mapie —
nie mówią nic o GTA 2.

### Dane referencyjne z oryginału

Ślady `provenance: originalGta2` mają powstawać z oryginalnej gry uruchamianej na EC2
([aws/integration.md](aws/integration.md)). Do czasu ich nagrania w repozytorium nie ma
żadnych danych referencyjnych GTA 2.
