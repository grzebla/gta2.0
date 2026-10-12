# Macierz mechanik GTA 2 (plan i stan)

Stan na 2026-10-12 (gałąź `migrate/native-cpp-engine`). Kolumna „Weryfikacja” mówi, jak
potwierdzimy zgodność z oryginałem. **Żadna mechanika nie jest jeszcze potwierdzona
danymi z oryginalnej gry.** Odniesienia do GTA2_RE to wyłącznie nazwy plików i funkcji
(analiza zachowania, bez kopiowania kodu — [reference/gta2re.md](reference/gta2re.md)).

Legenda stanu: **brak** · **prototyp** (działa, reguły wymyślone, nie GTA 2) ·
**częściowo** (część formatu lub zachowania zgodna ze źródłem) · **zgodne** (potwierdzone testem porównawczym).

| # | System | Stan w silniku | Gdzie (nasz kod) | Odniesienie GTA2_RE (nazwy) | Zależy od | Weryfikacja |
|---|---|---|---|---|---|---|
| 1 | Pętla gry, stały tick, kolejność aktualizacji | prototyp (stały tick 30 Hz, deterministyczny) | `sim/world.cpp`, `core/clock.hpp` | `Game_0x40.cpp` (`UpdateGame`, `GetFrameDurationMs`) | — | pomiar długości ticku i kolejności zdarzeń w oryginale |
| 2 | RNG | prototyp (SplitMix64) | `core/rng.hpp` | `rng.cpp` | 1 | porównanie sekwencji z nagrania oryginału (jeśli algorytm da się ustalić z wyjść) |
| 3 | Wejście i nagrywanie powtórek | częściowo (replay scenariuszy) | `testing/scenario.*`, `sim/input.hpp` | `input.cpp`, `keybrd_0x204.cpp`, `InputRecorder_67F8B0.cpp` | 1 | odtworzenie tego samego wejścia w oryginale na EC2 |
| 4 | Mapa: odczyt GMP | częściowo (DMAP/CMAP/UMAP, ZONE) | `assets/gmp_map.*` | `LoadChunk_5AB4B0`, `Load_RGEN_588B30` | — | testy `[gamedata]` na oryginalnych plikach |
| 5 | Mapa: geometria 3D, skosy, ziemia | brak (w renderze tylko widok z góry) | `scene/draw_list.cpp` | `FindGroundZForCoord_4E5B60`, `DrawGradientSlope_4F6630`, `DrawDiagonalWall_4F02D0` | 4 | zrzuty ekranu oryginału vs nasze (gfx-e2e) |
| 6 | Style (`.sty`): kafle, sprite'y, palety | brak | — | `gtx_0x106C.cpp` | 4 | porównanie zdekodowanych kafli z zrzutami oryginału |
| 7 | Renderowanie i kamera | prototyp (SDL_GPU, kamera 2D, batching) | `render/`, `core/camera2d.*` | `Camera.cpp` (`Camera_0xBC::Update_435FF0`, `ApplyShake_436140`), `Draw_4F6A20` | 5, 6 | zrzuty z oryginału na EC2, porównanie obrazów |
| 8 | Ruch pieszego (gracza) | prototyp (wymyślone przyspieszenia) | `sim/world.cpp` | `Char_B4::TickMovementStateMachine_54C900`, `CanStepForward_54FEC0`, `DoPedControlInputs_566C80` | 1, 3, 4 | ślady pozycji/prędkości z oryginału co tick |
| 9 | Kolizje z mapą | prototyp (blokada osiowa, ślizg) | `sim/world.cpp` | `collide.cpp`, `CheckMapZCollision_5A21F0`, `CanMoveOntoSlopeTile_4E0130` | 4, 5 | scenariusze „wjazd w ścianę” na oryginalnych mapach |
| 10 | Pojazdy: fizyka | brak | — | `CarPhysics_B0.cpp` (`ApplyInputsAndIntegratePhysics_562F30`, `CalculateFrontWheelForce_561E50`), `Car_BC.cpp` | 1, 9, 6 (definicje pojazdów w `.sty`) | ślady pozycji/kąta/prędkości auta co tick |
| 11 | Pojazdy: kolizje, uszkodzenia | brak | — | `HandleCarCollision_55FF20`, `ApplyImpactDamage_43D5D0`, `BinarySearchCollisionTime_55C560` | 10 | scenariusze zderzeń |
| 12 | Wsiadanie/wysiadanie | brak | — | `CanBeEnteredByPed_4451E0`, `EnterCarOrFlee_467BD0` | 8, 10 | sekwencje stanów w śladach |
| 13 | Piesi i AI | brak (tylko syntetyczni „przechodnie”) | `sim/world.cpp` (`Wanderer`) | `char.cpp`, `Char_Pool.cpp`, `FollowTargetStateMachine_46AC20`, `FleeOnFootTillSafe_4678E0`, `ComputePath_554AB0` | 1, 2, 8 | statystyki zachowań przy stałym seedzie (jeśli RNG odtworzony) |
| 14 | Ruch uliczny, sieć dróg (strzałki, RGEN) | brak (strzałki są parsowane w `block_info`) | `assets/gmp_map.hpp` | `Load_RGEN_588B30`, `CarsService_446790` | 4, 10 | trasy pojazdów AI |
| 15 | Policja i poziom poszukiwania | brak | — | `Police_7B8::Service_570270`, `DispatchNewCrewToPursuit_56FAA0`, `CreateRoadblock_575FF0`, `ClearWantedPoints_420B80` | 13, 10 | scenariusze przestępstw i licznik poszukiwania |
| 16 | Broń i obrażenia | brak | — | `Weapon_30.cpp`, `Explosion_30.cpp`, `ApplyBlastDamage_541850` | 8, 13 | dmg/HP w śladach |
| 17 | Gangi i respekt | brak (strefy ZONE parsowane) | `assets/gmp_map.*` | `Gang.cpp` (`Gang_144`, `GangPool_CA8`, `IncrementRespect_4BEE50`, `DecrementRespect_4BEEA0`, `ApplyKillRespectChange_4BEF70`, `GetGangAtCoords_4DFB50`) | 4, 13, 16 | zmiany respektu po zdarzeniach |
| 18 | Misje i skrypty (`.scr`) | brak | — | `LoadMissionScript_512400`, `ExecuteScriptThreads_5127A0`, `ExecOpCode_5061C0`, `Gosub_50CE50` | 1, 13, 10 | wykonanie skryptów krok po kroku vs oryginał |
| 19 | Teksty (`.gxt`) | brak | — | — | — | porównanie ciągów z grą |
| 20 | Dźwięk (`.sdt/.raw`), radio | brak | — | `cSampleManager.cpp`, `CycleRadioStation_57EEE0`, `HandleAICarEngineSound_418190` | platform (audio SDL3) | odsłuch/sumy próbek |
| 21 | HUD i frontend | brak | — | — | 6, 7 | zrzuty ekranu |
| 22 | Zapis stanu gry | brak | — | `LoadSave_511F80`, `CopyPlayerDataToSave_56A1A0`, `LoadPlySlotDat_56B990` | wszystkie systemy stanu | wczytanie zapisów oryginału |

Elementy infrastruktury potwierdzone testami (nie mechaniki gry): determinizm symulacji
(bit-exact, testy CTest), format kontenera i bloków GMP (testy syntetyczne zgodne z
dokumentem formatu), framework porównawczy (testy jednostkowe).
