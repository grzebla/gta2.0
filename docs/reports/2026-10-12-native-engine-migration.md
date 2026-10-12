# Raport: migracja na natywny silnik C++20 / SDL3 / SDL_GPU (2026-10-12)

## 1. Zmiany i commity

Gałąź `migrate/native-cpp-engine` → PR #2 (od `main`, bez kodu OpenGTA2):

| Commit | Zakres |
|---|---|
| `f8cb38f` | przeniesienie z zamkniętego PR #1 tylko elementów bez .NET: utwardzone werdykty `gfx-compat` (`GfxAnalysis.psm1` + Pester), ochrona przed pętlą restartów, skrypty operatora `scripts/aws/*`, testy Bash z atrapą `aws`, scenariusze JSON, archiwalny audyt |
| `b8d3dc1` | silnik: core, assets (GMP), sim, scene, render (SDL_GPU), platform (SDL3), testing, aplikacje `gta2n` i `g2n-headless`, testy doctest i CTest, CI `native-ci` |
| `8c48748` | CI: pełne zależności SDL3 na Linuksie, kolejność `windows.h`/`psapi.h` |
| `62d42e2` | dokumentacja, `CLAUDE.md`, usunięcie skilla `csharp-xunit`, poprawka testu (`-Wdouble-promotion`) |

Usunięte założenia: rozwój na bazie OpenGTA2/MonoGame/.NET. PR #1 zamknięty z komentarzem;
gałąź `integrate/opengta2-upstream` zostaje jako archiwum. Na `main` nigdy nie trafił kod
OpenGTA2 ani .NET. Historia `main` jest nienaruszona (bez force push).

Zachowane bez zmian funkcji: CloudFormation (host G4dn, OIDC/IAM), workflow `gfx-session`,
`gfx-compat`, `gfx-e2e`, `infra-lint`, bootstrap Windows, DDrawCompat, dgVoodoo2, watchdog,
reaper, budżet i skrypty kosztowe.

## 2. Stan kompilacji

| Platforma | Kompilator | Debug | Release | ASan+UBSan | Uruchomienie |
|---|---|---|---|---|---|
| macOS ARM64 (lokalnie: macOS 27.0.1) | Apple Clang 21 | ✅ | ✅ | ✅ | ✅ okno (Metal), 179 klatek; ✅ offscreen (Metal) |
| macOS ARM64 (CI `macos-15`) | Apple Clang | — | ✅ | ✅ | — |
| Windows x64 (CI `windows-2025`) | MSVC + Windows SDK (shadery D3D12) | ✅ | ✅ | — | nieuruchamiane |
| Linux x64 (CI `ubuntu-24.04`) | Clang | — | ✅ | ✅ | ✅ offscreen przez Vulkan (lavapipe) |

CI `native-ci` na commicie `62d42e2`: wszystkie 8 zadań zielone (6 konfiguracji build+test,
analiza statyczna, render smoke), `infra-lint` zielony. Pierwszy przebieg wykrył dwa
problemy specyficzne dla platform (brakujące pakiety deweloperskie SDL3 na Ubuntu,
kolejność `windows.h`/`psapi.h` po sortowaniu nagłówków) i jeden błąd testu
(`-Wdouble-promotion` w nowszym Clangu) — wszystkie poprawione. Kompilacja jest bez
ostrzeżeń (`-Werror`, MSVC `/W4 /WX`).

Zależności przypięte URL + SHA-256: SDL 3.4.18 (zgodny z digestem GitHub), doctest 2.5.3,
nlohmann/json 3.12.0; shadery SPIR-V z glslang 16.6.0 (SHA-256 zgodny z digestem).

## 3. Wyniki testów

| Zestaw | Wynik |
|---|---|
| CTest (36 przypadków doctest + 6 testów scenariuszy + 1 test `wall-slide`): Debug, Release, headless, ASan/UBSan na macOS | 43/43 ✅ |
| clang-format 23.1.3 (`--Werror`), clang-tidy 22.1.8 (bugprone, performance, portability, wybrane modernize/cppcoreguidelines) | 0 uwag |
| Granice modułów (`check-module-boundaries.sh`) | ✅ |
| Pester (`GfxAnalysis`) | 17/17 ✅ |
| Bash (`ec2-gpu-session.test.sh`, atrapa `aws`) | 33/33 ✅ |
| actionlint, shellcheck, cfn-lint | ✅ |
| Render offscreen: Metal (lokalnie) i Vulkan/lavapipe (CI) | ✅ identyczna scena |

Benchmark headless (macOS ARM64, Release): ~60 ns na tick w stanie ustalonym (100 000
ticków, gracz + 2 agentów, 11,5 MiB RSS). Krótki scenariusz (180 ticków) ~7 µs na tick —
dominuje jednorazowe wczytanie mapy. Czas klatki `gta2n` (Debug, vsync, Metal): średnio
13,5 ms, p99 25,3 ms, symulacja dokładnie 30 Hz bez pominiętych kroków.

## 4. Macierz mechanik

[../mechanics-matrix.md](../mechanics-matrix.md): 22 systemy. Stan: pętla/stały tick, RNG,
ruch i kolizje — **prototyp** (reguły wymyślone); GMP i replay wejścia — **częściowo**;
pozostałe (pojazdy, AI, policja, broń, gangi, misje, dźwięk, zapis, style, HUD) — **brak**.
Żadna mechanika nie jest jeszcze potwierdzona danymi z oryginału.

## 5. Zasoby oryginalnej gry

Lokalnie brak instalacji GTA 2, więc mapa demonstracyjna i dane testowe są **syntetyczne**
i tak oznaczone. Parser GMP pochodzi z dokumentu DMA Design v8.2. Trzy punkty wymagają
potwierdzenia na oryginalnych plikach — test `[gamedata]` sam się włącza po ustawieniu
`G2N_GAME_DIR`:
1. liczba wpisów w kolumnie DMAP (`height − offset`),
2. kolejność osi `base[256][256]` i UMAP,
3. czy mapy PC używają DMAP.

Brakuje parserów `.sty`, `.scr`, `.gxt`, `.sdt`.

## 6. Gotowość AWS

Bez zmian w zasobach AWS (tylko odczyty). Plan PAID; limit GPU 0 vCPU, wniosek
`CASE_OPENED`. Pierwszy test **da się wykonać bez advanced features i OIDC** skryptami
operatora (symulacja IAM: 39/39 potrzebnych akcji dozwolonych). Odpowiedzi na pytania
weryfikacyjne (WDDM na T4, sesja interaktywna, watchdog, zatrzymanie, restarty, proces vs
render): [../aws/integration.md](../aws/integration.md). Najważniejsze ryzyko: sterowniki
GRID/gaming są zablokowane przez SCP, a publiczny sterownik Tesla startuje w TCC.
Przełączenie na WDDM jest zaimplementowane, ale dopiero `probe` pokaże, czy działa.

## 7. Nierozwiązane problemy

1. Weryfikacja parsera GMP na oryginalnych mapach (brak danych lokalnie).
2. Windows: aplikacja zbudowana w CI, ale okno/D3D12 nie było uruchomione ręcznie.
3. Linux: okno (X11/Wayland) nieuruchamiane; zweryfikowany tylko render offscreen.
4. Render: brak tekstur (`.sty`), ścian 3D, skosów, przezroczystości warstw, HUD.
5. Shadery w trzech językach (GLSL/HLSL/MSL) utrzymywane ręcznie. Przy większej liczbie
   shaderów warto rozważyć SDL_shadercross.
6. Sterownik T4: WDDM z publicznym sterownikiem Tesla niepotwierdzony; GitHub OIDC wymaga
   advanced features.
7. Licencja projektu niewybrana; nazwa repozytorium — propozycja `gta2-native` czeka na decyzję
   ([../repo-rename.md](../repo-rename.md)).
8. Narzędzie nagrywające ślady z oryginału — tylko plan.

## 8. Kroki po zatwierdzeniu limitu GPU

1. `infra/aws/create-budget.sh <e-mail> 15` (alerty; nie twardy limit).
2. 💰 `scripts/aws/deploy-gfx-stack.sh` (sprawdza limit ≥ 4 vCPU, wymaga „uruchom-ec2”).
3. `scripts/aws/upload-game.sh <zip GTA 2 10.5>` → wpisać SHA-256 do `tests/gfx/compat-matrix.json`.
4. 💰 `scripts/aws/run-gfx-compat-local.sh probe` → ocena wg `docs/ci/gfx-compat-checklist.md`
   (WDDM, sesja, `gpuProcessSeen`).
5. 💰 `scripts/aws/run-gfx-compat-local.sh compat` → przy `COMPATIBLE` wybór konfiguracji.
6. `scripts/aws/cost-check.sh`; host zatrzymany (stały koszt ~5 USD/mies. za dysk).
7. Później: advanced features (decyzja) → stos OIDC → `gfx-compat`/`gfx-e2e` z GitHub Actions.

## 9. Priorytety

1. Dane oryginalne lokalnie (`G2N_GAME_DIR`): potwierdzenie GMP, parser `.sty`, tekstury kafli.
2. Pierwszy `probe`/`compat` na EC2 po zatwierdzeniu limitu.
3. Geometria 3D bloków i skosy w renderze; kamera z wysokością.
4. Projekt narzędzia nagrywającego ślady z oryginału (wejście + stan co tick).
5. Ruch pieszego i kolizje zgodne z oryginałem, weryfikowane śladami `originalGta2`.

Źródła zewnętrzne: [dokument formatu GMP (gtamp.com)](https://gtamp.com/gta2/gta2-gmp-map-file-format/),
[SDL3 wiki: SDL_CreateGPUShader](https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader),
[AWS: NVIDIA drivers for EC2](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/install-nvidia-driver.html).
