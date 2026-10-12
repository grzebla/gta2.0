> **Dokument archiwalny (2026-10-12).** Audyt powstał, gdy rozważaliśmy rozwój na bazie
> OpenGTA2 (C#/MonoGame). Projekt tego nie robi — budujemy własny silnik C++20/SDL3
> (patrz `docs/architecture.md`). Zachowano go, bo mapuje systemy oryginalnej gry na
> ścieżki w GTA2_RE (tylko nazwy plików i funkcji, bez kodu) i opisuje kolejność ticku
> oryginału. Opisy stanu OpenGTA2 nie dotyczą naszego kodu.

# OpenGTA2 — macierz funkcji i luk (audyt)

Data audytu: 2026-10-12. Dokument opisuje stan kodu w `src/` względem systemów oryginalnego GTA2 (PC 10.5),
z odniesieniami do dekompilacji `CriminalRETeam/gta2_re`.

## Legenda

- **fakt (kod)** — stwierdzenie sprawdzone bezpośrednio w kodzie (ścieżka, klasa, metoda, ewentualnie numer linii przy commicie z sekcji „Metodologia”).
- **szacunek** — wniosek, interpretacja albo ocena skutku, której nie da się potwierdzić samym odczytem kodu (np. bez danych gry, bez uruchomienia, bez kompilacji).
- Stany implementacji w OpenGTA2:
  - **podpięte** — zaimplementowane i wywoływane w pętli gry (`GtaGame` → scena → komponenty),
  - **tylko parsowane/modelowane** — dane są wczytywane albo istnieje model, ale nic w grze z nich nie korzysta,
  - **zaślepka/TODO** — jest szkielet, wyliczenie albo komentarz TODO bez działania,
  - **brak** — w kodzie nie ma nic związanego z systemem.
- Dokument nie podaje procentów ukończenia.

---

## Metodologia

**Przeczytane źródła**

| Repozytorium | Commit | Zakres odczytu |
|---|---|---|
| OpenGTA2 (lokalnie, gałąź `integrate/opengta2-upstream`) | `2b72b4d0afcdf029fa018437eaf91c8c17257a26` | Wszystkie pliki `*.cs`, `*.fx`, `*.csproj`, `Assets/Assets.mgcb`, `Assets/Fonts/DebugFont.spritefont`, `app.manifest`, `.config/dotnet-tools.json`, `Directory.Build.props`, `.editorconfig`, `OpenGta2.sln`, `gtadocs.md`. Łącznie ok. 6,4 tys. linii C#/HLSL. |
| `CriminalRETeam/gta2_re` (gałąź `master`) | `a0d15ab4ef01a242c9466d6ae9158311bdded3e0` | Lista plików (tree API), `README.md`, `CLAUDE.md`, `docs/missing_functions.md`, `docs/structure_scores.md`; nazwy klas i funkcji z nagłówków `Source/*.hpp`; liczba znaczników statusu w `Source/*.cpp`; kolejność wywołań w `Game_0x40::UpdateGame_4B9410` (przeczytana, opisana własnymi słowami). |

**Zasady dotyczące gta2_re.** Repozytorium nie ma licencji. Czytano je wyłącznie po to, by ustalić, jakie systemy istnieją w oryginale i gdzie leżą. Dokument nie zawiera fragmentów jego kodu, podaje tylko ścieżki plików oraz nazwy klas i funkcji. Kopię płytką (`--depth 1`) trzymano poza projektem, w katalogu scratchpad sesji.

**Status dopasowania w gta2_re.** Każda zreimplementowana funkcja ma w gta2_re znacznik: `MATCH_FUNC` (kod maszynowy zgodny bajt w bajt z oryginałem i sprawdzany przy budowaniu), `WIP_FUNC` (zaimplementowana, jeszcze niezgodna) albo `STUB_FUNC` (niezaimplementowana). Liczby w kolumnach „Odpowiednik w GTA2_RE” to wynik `grep -c` na tych znacznikach w danym pliku `.cpp` przy podanym commicie, w zapisie „M/W/S”. W całym `Source/` jest 3414 znaczników `MATCH_FUNC`, 23 `WIP_FUNC` i 12 `STUB_FUNC`; `docs/missing_functions.md` wylicza dodatkowo 1069 funkcji bez znacznika (głównie CRT, thunki i małe kopie funkcji inline). Plik `docs/structure_scores.md` jest częściowo nieaktualny: wymienia jako WIP funkcje, które w kodzie mają już `MATCH_FUNC`, np. `CarPhysics_B0::ComputePointVelocity_561380` — fakt (kod).

**Czego nie zrobiono.**
- Nie budowano ani nie uruchamiano projektu. Na tej maszynie jest tylko .NET SDK 8.0.301 (`dotnet --list-sdks`), a wszystkie projekty celują w `net10.0`/`net10.0-windows` — fakt (kod/środowisko).
- Nie uruchamiano testów. Brak oryginalnych danych gry (`OPENGTA2_PATH` nie jest ustawione).

**Zmiany równoległe w trakcie audytu.** Audyt opisuje kod z commita `2b72b4d`. W trakcie audytu, poza nim, gałąź przesunęła się o dwa commity (stan końcowy `69109837029ee3f3519b1deaf1ad8150501bbaa7`) — fakt (`git log`):
- `261f70f` „Fix two GameData parser bugs found by synthetic tests”. Naprawia `RiffChunkStream.Seek(SeekOrigin.End)` (ignorowany offset) i rozmiar `StringHeader` w `ScriptParser` (wypełnienie do 12 bajtów zamiast 9 psuło każdy wpis tablicy napisów po pierwszym; ten błąd przeoczyłem przy własnym odczycie). Dodaje też odczyt `OPENGTA2_PATH` z procesu w `Client` i `GameData.UnitTests` (bez `DebugConsole`), `global.json` (SDK 10.0.100) i testy `Category=Synthetic` w `src/OpenGta2.GameData.UnitTests/Synthetic/`.
- `6910983` „Add conformance test framework…”. Dodaje projekty `src/OpenGta2.Conformance` (m.in. `Simulation/FixedStepClock.cs`, `DeterministicRandom.cs`, `InputTimeline.cs`, `ISimulationAdapter.cs`, `ScenarioRunner.cs`; adapter wyłącznie syntetyczny `Adapters/SyntheticKinematicsAdapter.cs`), `.Cli`, `.UnitTests` oraz `OpenGta2.CrossPlatform.slnf` (bez `OpenGta2.Client`). Żaden z projektów `Conformance` nie referencjonuje `OpenGta2.Client` — fakt (`*.csproj`). Szwy z sekcji „Architektura” to więc nadal miejsca, w których trzeba podpiąć prawdziwy adapter OpenGTA2.

Tam, gdzie te commity zmieniają wnioski, zaznaczono to w tekście („po `261f70f`”).

---

## Podsumowanie

| # | System | Stan w OpenGTA2 (HEAD) | Pewność |
|---|---|---|---|
| 1 | Renderowanie i kamera | Mapa 3D (wszystkie typy skosów), światła punktowe, sprite'y pieszych, czcionka stylu, kamera z góry — **podpięte**. HUD, animacje kafli, pojazdy, obiekty, cząsteczki — **brak**. | fakt (kod) |
| 2 | Odczyt map i zasobów | RIFF, GMP (część kawałków), STY (część), SCR, GXT, SDT/RAW — **czytniki są**. RGEN — **TODO**. GCI, SEQ, MMP, TGA, zapisy — **brak**. | fakt (kod) |
| 3 | Pojazdy, fizyka, kolizje | Pojazdów **brak**. Kolizja 2D pieszego ze ścianami na jednym poziomie Z — **podpięte, uproszczone**. | fakt (kod) |
| 4 | Sterowanie graczem | Chodzenie i obrót z klawiatury — **podpięte**. Strzał zmapowany, ale nieużywany. | fakt (kod) |
| 5 | Piesi i AI | Model `Ped` z dwiema animacjami, tylko gracz. AI — **brak**. | fakt (kod) |
| 6 | Policja (wanted level) | **brak** | fakt (kod) |
| 7 | Broń i obrażenia | **brak** (tylko `Control.Shoot` w mapowaniu klawiszy) | fakt (kod) |
| 8 | Gangi i respekt | **brak** (tylko `ZoneType.Gang` z parsera stref) | fakt (kod) |
| 9 | Misje i skrypty | Parser SCR — **jest**. Interpreter obsługuje 2 polecenia, nie jest podpięty do klienta. | fakt (kod) |
| 10 | Dźwięk | Czytnik SDT/RAW i komponent testowy odtwarzający 2 próbki — **podpięte (test)**. | fakt (kod) |
| 11 | Zapis stanu gry | **brak** | fakt (kod) |

---

## Architektura OpenGTA2

### Projekty

| Projekt | TFM / platforma | Rola | Pewność |
|---|---|---|---|
| `src/OpenGta2.GameData` | `net10.0`, bez zależności zewnętrznych, `AllowUnsafeBlocks` | Czytniki formatów (Riff, Map, Style, Scripts, Audio, GXT) | fakt (kod) |
| `src/OpenGta2.Client` | `net10.0-windows`, `WinExe`, `PlatformTarget=x86`, `UseWindowsForms=true`, `MonoGame.Framework.WindowsDX` 3.8.4.1 | Gra: pętla, sceny, komponenty, renderowanie, kolizje, pieszy gracza | fakt (kod) |
| `src/OpenGta2.DebugConsole` | `net10.0`, `Exe` | Narzędzie: wypisuje mapę albo (zakomentowane) polecenia SCR przez `LogScriptRuntime` | fakt (kod) |
| `src/OpenGta2.GameData.UnitTests` | `net10.0`, xUnit 2.9.3, Moq, Shouldly | Testy czytników, wszystkie `[Trait("Category","DataTests")]`, wymagają danych gry | fakt (kod) |

Ważne dla testów: **cała logika rozgrywki** (kolizje `CollisionMap`, `Ped`, `PedManager`, `Controls`, `Camera`) leży w `OpenGta2.Client`. Ten projekt celuje w `net10.0-windows`/x86 z WindowsDX, więc projekt testowy na macOS/Linux nie może go dziś referencjonować — fakt (kod) co do lokalizacji klas; szacunek co do niemożności referencji (wynika z TFM `-windows` i pakietu WindowsDX).

### Pętla gry i model klatki

1. `src/OpenGta2.Client/Program.cs` tworzy `GtaGame` i woła `game.Run()`. Pętlę prowadzi MonoGame (`Game.Run` → `Tick` → `Update`/`Draw`) — fakt (kod).
2. `GtaGame` (`src/OpenGta2.Client/GtaGame.cs`) nie ustawia `IsFixedTimeStep` ani `TargetElapsedTime` (grep: brak wystąpień w `src/`) — fakt (kod). Obowiązują więc wartości domyślne MonoGame, czyli stały krok około 1/60 s z mechanizmem nadrabiania zaległych kroków — szacunek (domyślne zachowanie frameworka, nie sprawdzone w tym repo). Włączone są VSync (`SynchronizeWithVerticalRetrace = true`, linia 24) i 4× MSAA — fakt (kod).
3. `GtaGame.Update(GameTime)` (linie 76–92), w tej kolejności — fakt (kod):
   - `_controls.Update()`, czyli odczyt `Keyboard.GetState()` (`Controls.cs` linie 74–78),
   - przy pierwszym wywołaniu `FirstUpdate()`, który aktywuje `LoadingWorldScene` z zakodowanymi na sztywno `data/bil.gmp` i `data/bil.sty` (linia 47; inne poziomy i intro są zakomentowane w liniach 44–49),
   - wyjście z gry po wciśnięciu `Escape` (`Control.Menu`),
   - `base.Update(gameTime)`, czyli aktualizacja komponentów MonoGame w kolejności z kolekcji `Components`.
4. **Sceny** (`src/OpenGta2.Client/Scenes/`): `Scene : GameComponent`. `GtaGame.ActivateScene` usuwa (Dispose) i czyści wszystkie komponenty, a potem dodaje scenę. `Scene.AddComponent<T>()` tworzy komponent przez `Utilities/ComponentActivator<T>`, czyli wstrzykiwanie zależności przez konstruktor z `Game.Services` (`GameServiceContainer`) — fakt (kod).
   - `LoadingWorldScene.Initialize` synchronicznie wczytuje poziom (`LevelProvider.LoadLevel`) i od razu aktywuje następną scenę — fakt (kod).
   - `TestWorldScene.Initialize` (linie 17–30) rejestruje usługi `Camera` i `PedManager`, potem komponenty w kolejności: `AudioTestComponent`, `MapComponent`, `SpriteTestComponent`, `PlayerControllerComponent`, `PedManagerComponent`, `CameraComponent`, `DebuggingDrawingComponent`. `UpdateOrder` nigdzie nie jest ustawiany, więc kolejność aktualizacji wynika z kolejności dodania — fakt (kod) co do braku `UpdateOrder`; szacunek co do stabilności sortowania w MonoGame.
   - Skutek tej kolejności: `MapComponent.Update` → `LevelProvider.Update(camera)` używa frustum kamery z **poprzedniego** ticku, bo `CameraComponent.Update` (ustawia `Camera.Frustum.Matrix`) wykonuje się później — fakt (kod).
5. **Krok czasu.** Logika używa zmiennego `dt` z `GameTime` przez `Utilities/GameTimeExtensions.GetDelta()` (`ElapsedGameTime.TotalSeconds` jako `float`): `PlayerControllerComponent.Update` (linie 58 i 61), `CameraComponent.Update` (linia 27), `PedManagerComponent.Update` → `Ped.UpdateAnimation(dt)` (linia 75), a także `DebuggingDrawingComponent.Draw` (tylko licznik FPS) — fakt (kod).
6. **Arytmetyka.** Pozycje i obroty są w `float` (`Microsoft.Xna.Framework.Vector3`), kierunek liczony przez `MathF.Sin`/`MathF.Cos` (`PlayerControllerComponent.cs` linia 60) — fakt (kod). Oryginał używa stałoprzecinkowego `Fix16` (`gta2_re/Source/fix16.cpp`, `Fix16_Point.cpp`) — fakt (nazwy plików i typów). Wyniki `MathF.Sin`/`Cos` mogą się różnić bitowo między x64 a ARM64 i między wersjami runtime — szacunek.
7. **RNG.** Dwie niezasiane instancje `System.Random`: `AudioTestComponent._random` (`Components/AudioTestComponent.cs` linia 13) i `SoundLibrary._random` (`GameData/Audio/SoundLibrary.cs` linia 11, losowe odchylenie częstotliwości próbki w linii 31) — fakt (kod). W symulacji ruchu nie ma RNG, bo nie ma AI — fakt (kod).
8. **Stan statyczny/globalny** — fakt (kod):
   - `Diagnostics/DiagnosticHighlight` to statyczna lista zapisywana w trakcie **symulacji** (`CollisionMap.CheckWall`, linie 98, 112, 122, 151; metody `[Conditional("DEBUG")]`) i czyszczona w trakcie **rysowania** (`DebuggingDrawingComponent.Draw`). To sprzężenie symulacji z renderem przez stan statyczny.
   - `Diagnostics/DiagnosticValues` (statyczny słownik), `Diagnostics/PerformanceCounters.Drawing` (statyczna instancja, `Stopwatch`).
   - `Rendering/QuadRenderer._verts` (modyfikowalna tablica statyczna, tylko render).
   - `TestGamePath.Directory` (statyczny odczyt zmiennej środowiskowej przy każdym użyciu).
   - `CollisionMap.CheckWall` wywołuje `Debug.WriteLine` przy każdym trafieniu (linie 111 i 118).
9. **Gdzie żyje stan gry** — fakt (kod): kontener `Game.Services` (`Controls`, `AssetManager`, `LevelProvider`, `Camera`, `PedManager`), pola komponentów (np. `PlayerControllerComponent._player`, `_moveSlow`; `PedManagerComponent._pedAnimNum`/`_pedAnimOveride`; `MapComponent._noon`), obiekty `Ped` (pole prywatne `_animationTime`) i `Camera` (`Position`, `Mode`, `AttachedToPed`).
10. **Sprzężenie z GPU i oknem** — fakt (kod):
    - konstruktor `LevelProvider(GraphicsDevice)`; `LevelProvider.LoadLevel` wczytuje `Map` i `Style`, tworzy `StyleTextureSet` (tekstury GPU) i `CollisionMap` w jednej metodzie,
    - `Camera(GameWindow)` bierze proporcje z `ClientBounds`, a strumieniowanie chunków zależy od frustum,
    - wszystkie efekty i czcionka ładują się w `GtaGame.LoadContent` przez `ContentManager`.
    Za to `CollisionMap(Map)`, `Ped`, `PedManager` i cały `OpenGta2.GameData` nie potrzebują GPU.

### Szwy dla deterministycznego replayu i porównań

| Szew | Miejsce (klasa/metoda) | Co tam podpiąć | Co dziś blokuje |
|---|---|---|---|
| Wstrzykiwanie wejścia | `Controls.Update()` (`Controls.cs:74`); instancja `Controls` jest usługą (`GtaGame.LoadContent`, linia 57) | Źródło wejścia (`IInputSource`: na żywo / z nagrania), które dla ticku N zwraca stan klawiszy. `Controls` liczy już krawędzie (`IsKeyDown` = wciśnięty w tej klatce), więc wystarczy podmienić `Keyboard.GetState()`. | `Controls` jest klasą konkretną bez interfejsu i woła statyczne `Keyboard.GetState()`. `IntroComponent.Update` czyta `Keyboard.GetState()` bezpośrednio (linia 105), z pominięciem `Controls`. Wejście czytane jest raz na `Update`, a nie raz na tick symulacji (przy nadrabianiu kroków to jest to samo, ale tylko dzięki temu, że wszystko siedzi w `Update`) — fakt (kod). |
| Stały tick symulacji | `GtaGame.Update(GameTime)` (linie 76–92) | Jawne `IsFixedTimeStep = true` i `TargetElapsedTime`, a docelowo osobne `Simulation.Step(in TickInput)` ze stałym `dt` (albo stałym krokiem w ms jak w oryginale), wywoływane z `Update` lub z bezgłowego runnera. | Logika siedzi w `GameComponent.Update(GameTime)` i bierze `dt` z `GameTime`. Pętlę i nadrabianie kroków kontroluje MonoGame. Arytmetyka jest zmiennoprzecinkowa (`float`, `MathF`) — fakt (kod). |
| Zrzut stanu per tick | Koniec `GtaGame.Update` po `base.Update` | Serializacja: `PedManager.Peds` (`Position`, `Rotation`, `Remap`, `Animation`, `AnimationFrame`, prywatne `_animationTime`), `Camera` (`Position`, `Mode`), pola komponentów (`_moveSlow`, `_noon`, `_pedAnimNum`). | Stan jest rozproszony po usługach i polach prywatnych komponentów. Brak modelu „świata” i brak serializacji — fakt (kod). |
| Bezgłowe wczytanie poziomu | `LevelProvider.LoadLevel` (`Levels/LevelProvider.cs:52–78`) | Rozdzielenie na część symulacyjną (`Map`, `CollisionMap`) i renderową (`StyleTextureSet`, chunki). `CollisionMap(Map)` już nie zależy od GPU. | Konstruktor `LevelProvider` wymaga `GraphicsDevice`. `MapComponent.Update` wywołuje strumieniowanie chunków (kod renderowy) w `Update` — fakt (kod). |
| Deterministyczny RNG | `SoundLibrary` (konstruktor), `AudioTestComponent` | Usługa `IRandom` z ziarnem zapisywanym w nagłówku replaya; osobne strumienie dla symulacji i dla audio/efektów (żeby dźwięk nie wpływał na symulację). | Dwa niezasiane `new Random()` — fakt (kod). |
| Odcięcie diagnostyki | `DiagnosticHighlight.Add` w `CollisionMap` | Instancyjny kolektor diagnostyki albo no-op w trybie symulacji. | Statyczna lista zapisywana w symulacji, czyszczona w renderze — fakt (kod). |
| Testowalność poza Windows | `OpenGta2.Client.csproj` | Wydzielenie biblioteki `net10.0` (np. `OpenGta2.Simulation`) z `CollisionMap`, `Ped`, `PedManager`, modelem wejścia i krokiem symulacji. | TFM `net10.0-windows`, x86, WinForms, WindowsDX — fakt (kod). |

**Punkt odniesienia w oryginale (gta2_re, tylko nazwy).** Opis własnymi słowami — fakt (nazwy), szacunek (semantyka):
- `Game_0x40::ExecuteGame_4B9640` (`Source/Game_0x40.cpp`) najpierw aktualizuje sterowanie wszystkich graczy (`UpdateAllPlayersControls_4B9380`). Potem, zależnie od stanu gry (działa / pauza / „wykonaj jedną klatkę i wstrzymaj” — `SinglePlayerStepFrame_4B9710`), woła `UpdateGame_4B9410` i serwis dźwięku (`gRoot_sound_66B038.Service_40EFA0`).
- `Game_0x40::UpdateGame_4B9410` serwisuje podsystemy w stałej kolejności: graczy, reset kolejek kolizji i dźwięków kolizji, światła, dźwigi, zgniatarki, generatory, pieszych (`PedManager::PedsService_4703F0`), transport publiczny, garaże, samochody (`CarsService_446790`), sygnalizację, obiekty, wątki skryptów (`ScriptManager_C1EA8::ExecuteScriptThreads_5127A0`), ekipy ratunkowe, karetki, policję (`Police_7B8::Service_570270`), cząsteczki, animacje kafli, HUD, przesunięcie cyklu RNG (`rng::AdvanceCycle_48B900`), drzwi, wozy strażackie i „wybuchające” punkty. To dobry wzorzec kolejności ticku dla OpenGTA2.
- `Game_0x40::GetFrameDurationMs_4B8BB0`: w grze jednoosobowej długość klatki to stała w ms (jedna z dwóch wartości zależnie od flagi ograniczenia FPS), w sieci — wartość z sesji. Oryginał symuluje więc stałym krokiem, a nie zmiennym `dt`.
- `rng` (`Source/rng.cpp`/`rng.hpp`) to własny globalny generator (`gRng_6F6784`, `gpRng_67AB34`) z `srand_4F7A40`, `rand_4F7C00`, `get_int_4F7AE0`, `get_uint8_4F7B70`. Używają go m.in. `Ped.cpp`, `char.cpp`, `Char_Pool.cpp`, `Car_BC.cpp`, `Explosion_30.cpp`, `Particle_*.cpp`, `Weapon_30.cpp`, `Police_38.cpp`.
- `InputRecorder_67F8B0` (`Source/InputRecorder_67F8B0.cpp`) nagrywa i odtwarza bity wejścia (`ReplayHeader_10C`, `InputRecord_C`, `AttractFile`, `SaveReplay_4CDED0`, `input_init_replay_4CE740`, `LoadReplayHeader_4CE380`, `GetLastRecRngIdx_44AA80`). Z nazw wynika, że oryginalne dema (tryb attract) to deterministyczny replay „bity wejścia + stan RNG” — szacunek. To bezpośredni wzorzec dla naszego frameworka.

---

## 1. Renderowanie i kamera

**Stan w OpenGTA2 — potwierdzone kodem**

- **Podpięte** — fakt (kod):
  - Geometria mapy: `Levels/LevelProvider.cs` dzieli mapę na chunki 8×8 (`ChunkSize = 8`), ładuje i zwalnia je wg frustum kamery (`Update(Camera)`, `LoadChunk`, `UnloadChunk`). `Levels/SlopeGenerator.Push` generuje wierzchołki dla **wszystkich** wartości `SlopeType`: skosy 26°/7°/45°, ściany i skosy diagonalne, bloki częściowe i środkowe (`switch` w liniach 628–773). Ściany „flat” (przezroczyste) trafiają do osobnego bufora indeksów sortowanego po `drawOrder`. Są dwa TODO: `SlopeGenerator.cs:405` i `:427`, oba „`// TODO: double render if left.flat and right.flat`”.
  - Rysowanie: `Components/MapComponent.Draw` robi dwa przebiegi efektu `Assets/Effects/BlockFaceEffect.fx` (`Opaque` i `Flat` z alpha blending).
  - Oświetlenie: światła punktowe z kawałka `LGHT` (`LevelProvider.CollectLights`, maksymalnie 16 na chunk — `BlockFaceEffect.MaxLights`, z TODO „`// TODO: Performance is terrible. Lights should be in a quadtree for optimization.`” w linii 213), poziom ambient przełączany klawiszem `~` (`MapComponent.Update`), cieniowanie ścian i poziom światła lida (`SlopeGenerator.ApplyDefaultShading`, `FaceInfo.LightingLevel`). Przy `Camera.Position.Z > 40` światła są wyłączane.
  - Tekstury: `Levels/StyleTextureSet` składa kafle w jedną teksturę-tablicę (`Texture2D` z `arraySize = tileCount`) i leniwie tworzy tekstury sprite'ów per `(SpriteKind, number, remap)` z paletami i remapami. Jest TODO „`// TODO: Disposing`” (linia 13).
  - Sprite'y: `Components/PedManagerComponent.Draw` rysuje pieszych (z cieniem) przez `WorldSpriteEffect`. Numer sprite'a to stała `158 + _pedAnimNum` (linia 96), a klawisze `+`/`-` ręcznie zmieniają klatkę.
  - Tekst: `Rendering/FontRenderer` rysuje czcionką ze stylu (`FONB`) z TODO „`// TODO: Don't know how space is handled yet.`” (linia 33) i „`// TODO: Haven't figured out the rest of the charset yet`” (linia 46). `Components/SpriteTestComponent` rysuje stały napis „HI”.
  - Kamera: `Camera.cs` daje widok z góry w perspektywie (`CreatePerspectiveFieldOfView(PiOver4, …)`; komentarz mówi „90 fov”, kod ma 45°). Rzut jest lustrzany przez negację `M11`/`M13` (komentarz o LHS/RHS). Tryby `CameraMode.AttachedToPed` (wysokość +8 nad pieszym) i `Free` (klawisze numeryczne, `CameraComponent`).
  - Diagnostyka: `Diagnostics/DebuggingDrawingComponent` (FPS, liczniki, ramki kolizji, F1).
- **Niepodpięte domyślnie** — fakt (kod): `Components/IntroComponent` (wideo BIK przez `binkw32.dll`) jest używane tylko w zakomentowanej linii `GtaGame.cs:49`.
- **Tylko parsowane** — fakt (kod): `Map.Animations` (kawałek `ANIM`); `MapLight.Shape`, `OnTime`, `OffTime`; `Map.Objects` (`MOBJ`). Klient ich nie używa (grep).

**Brakuje** — fakt (kod: brak odpowiedniego kodu)
- Animacji kafli (`ANIM`), migania świateł (`OnTime`/`OffTime`), sprite'ów pojazdów, obiektów, broni i cząsteczek, obiektów mapy (`MOBJ`), sprite'ów „delta” (uszkodzenia, `DELS`/`DELX` nieczytane).
- HUD (strzałki, wanted, pager, kasa, mnożnik), menu (frontend), napisy z GXT na ekranie.
- Zachowań kamery z oryginału: zoom zależny od prędkości pojazdu, śledzenie pojazdu, wstrząsy, granice.
- Zmian mapy w trakcie gry: chunki są budowane raz i nie przebudowują się po zmianie bloku.
- Dynamicznego cyklu dnia i nocy ani poziomu ambient ze skryptu (`SET_AMBIENT_LEVEL` istnieje tylko w enumie).

**Odpowiednik w GTA2_RE** (ścieżki w `Source/`, M/W/S)
- Mapa: `MapRenderer.cpp` (58/1/0; np. `DrawGradientSlope_4F6630`, `DrawDiagonalWall_4F02D0`, `Draw_4F6A20` jako WIP).
- Kamera: `Camera.cpp` (32/0/0; `Camera_0xBC::Update_435FF0`, `UpdateFollowPedCamera_436540`, `UpdateFollowCarCamera_4364A0`, `ApplyShake_436140`, `UpdateBoundaries_435B90`).
- Sprite'y: `sprite.cpp` (95/0/0), `SpriteRenderer_1C.cpp` (14/0/0), `SpriteGrid_400.cpp` (31/0/0), `TextureCache_15D8.cpp` (19/1/0).
- Grafika niskopoziomowa: `gbh_graphics.cpp` (5/0/0), `Draw.cpp` (14/0/0).
- Światła: `Light_1D4CC.cpp` (4/0/0), `Light_28.cpp` (7/0/0).
- Animacje kafli: `TileAnim_2.cpp` (14/0/0; `UpdateTileAnimations_5BC310`, `AddGmpAnim_5BC2C0`).
- Cząsteczki: `Particle_8.cpp` (13/1/0), `Particle_4C.cpp` (20/1/0).
- HUD i menu: `Hud.cpp` (135/0/0), `Frontend.cpp` (109/3/0), `KanjiFont_8EC.cpp`.
- Wideo: `Bink.cpp` (13/0/0), `dma_video.cpp` (2/1/0).

**Zależności**
- HUD: GXT w kliencie, stan gracza (kasa, wanted, respekt), skrypty (briefy, strzałki) — szacunek.
- Kamera śledząca pojazd: system pojazdów.
- Animacje kafli: stały tick (oryginał aktualizuje je w `UpdateGame_4B9410`) — fakt (nazwy) / szacunek.

**Testy**
- Istniejące: brak testów renderowania — fakt (kod).
- Bez GPU: `SlopeGenerator.Push` to czysty kod CPU (macierze + `BufferArray`), więc można robić migawki liczby i pozycji wierzchołków oraz indeksów dla każdego `SlopeType` na syntetycznym `BlockInfo`. Konwersję palety (`StyleTextureSet.GetPaletteColor`) i macierze `Camera` też da się sprawdzić bez GPU po wydzieleniu z klas zależnych od `GraphicsDevice` — szacunek.

---

## 2. Odczyt map i zasobów

**Stan w OpenGTA2 — potwierdzone kodem**

| Format | Klasa / plik | Stan | Użycie w kliencie | Pewność |
|---|---|---|---|---|
| Kontener RIFF (GBMP/GBST/GBL) | `GameData/Riff/RiffReader.cs`, `RiffChunkStream.cs` | Zaimplementowany, z pamięcią podręczną pozycji kawałków | tak | fakt (kod) |
| GMP (wersja 500) | `GameData/Map/MapReader.cs` | Czyta `DMAP`, `MOBJ`, `ZONE`, `ANIM`, `LGHT`. Pomija `UMAP`, `CMAP`, `PSXM` (komentarz w linii 26). `RGEN` (skrzyżowania): blok `using` z „`// TODO`” (linie 54–57). Modele `Junction`, `JunctionLink`, `JunctionSegment` istnieją, ale nie są wypełniane; w `Junction.cs:12` jest „`// TODO: is this the correct data type?`”. | `DMAP` i `LGHT` tak; `MOBJ`, `ZONE`, `ANIM` nie | fakt (kod) |
| STY (wersja 700) | `GameData/Style/StyleReader.cs` | Czyta `PALB`, `PALX`, `PPAL`, `TILE`, `SPRG`, `SPRB`, `SPRX`, `FONB`. Nie czyta `CARI` (info o pojazdach), `OBJI` (info o obiektach), `DELS`/`DELX` (delty), `RECY`, `SPEC`, `PSXT`, `OVLY`. | tak (kafle, sprite'y pieszych, czcionka) | fakt (kod); lista brakujących kawałków na podstawie łańcuchów w `gta2_re/Source/gtx_0x106C.cpp` |
| SCR | `GameData/Scripts/Parsing/ScriptParser.cs` | Zaimplementowany: 6000 wskaźników, 65 536 bajtów danych, tablica napisów. `StringType` to pusty enum. | nie | fakt (kod) |
| GXT (wersja 100) | `GameData/GtaStringReader.cs` | Czyta `TKEY` i `TDAT` do słownika | nie | fakt (kod) |
| SDT/RAW | `GameData/Audio/SdtReader.cs`, `RawReader.cs`, `SoundLibrary.cs` | Zaimplementowane; generuje nagłówek WAV PCM16 mono | tak (`AudioTestComponent`) | fakt (kod) |
| BIK (wideo) | `Client/Components/IntroComponent.cs` | Przez natywny `binkw32.dll` z instalacji gry (P/Invoke) | tylko w zakomentowanym kodzie | fakt (kod) |
| GCI (fizyka pojazdów, `data\nyc.gci` w oryginale) | — | brak | — | fakt (kod); nazwa pliku z `gta2_re/Source/CarInfo_808.cpp` |
| SEQ (sekwencja poziomów), MMP (mapy multiplayer) | — | brak (wymienione tylko w `gtadocs.md`) | — | fakt (kod) |
| TGA (frontend), ustawienia (rejestr), zapisy (`.svg`, `.dat`, `.hsc`) | — | brak | — | fakt (kod) |

**Błędy znalezione przy odczycie**
1. `GameData/Style/Models/PaletteBase.cs:21`: `CodeObjRemapOffset => PedRemapOffset + CodeObjRemap`. Wszystkie pozostałe offsety dodają liczbę remapów **poprzedniego** typu, więc tu powinno być `+ PedRemap` — fakt (kod) co do niespójności; szacunek co do poprawnej formuły. Gdy `PedRemap != CodeObjRemap`, źle liczą się offsety remapów `CodeObj`, `MapObj`, `User` i `Font` — szacunek. Remapy pieszych (jedyne dziś używane) nie są dotknięte — fakt (kod).
2. `GameData/Audio/SoundLibrary.cs:85` (`SoundStream.Read`): część danych jest zawsze kopiowana od początku bufora (`_data[..readData]`) bez uwzględnienia bieżącej pozycji. Przy czytaniu strumienia w kilku porcjach kolejne porcje powtórzą początek próbki — fakt (kod). Czy `SoundEffect.FromStream` czyta w porcjach — szacunek.
3. `GameData/GtaStringReader.cs:63–70`: każdy bajt jest dekodowany osobno przez `Encoding.UTF8.GetString`, więc bajty ≥ 0x80 zamienią się w U+FFFD. Dla wersji językowych z diakrytykami (np. `f.gxt`, `g.gxt`) — szacunek.
4. `GameData/Map/Models/Map.cs` (`GetGroundZ`): pętla zaczyna od `zo = 1` i zwraca pierwszy blok z pustym lidem. To heurystyka, nie odpowiednik wyszukiwania podłoża z oryginału — fakt (kod); trafność — szacunek.
5. Naprawione po `261f70f` (poza tym audytem): `RiffChunkStream.Seek(SeekOrigin.End)` i rozmiar `StringHeader` w `ScriptParser` — fakt (`git show 261f70f`). Punkty 1–4 powyżej przy `69109837` są nadal otwarte — fakt (kod: tych plików commit nie zmienia).

**Brakuje**
- Parsowania `RGEN` (potrzebne do AI pojazdów), `CARI`, `OBJI`, `DELS`/`DELX`, `RECY`, `SPEC`.
- Plików GCI, SEQ, MMP, TGA, konfiguracji i zapisów — fakt (kod).
- Mapowania poziomu na pliki (`.seq`): poziom jest zakodowany na sztywno w `GtaGame.FirstUpdate` — fakt (kod).

**Odpowiednik w GTA2_RE**
- Mapa: `map_0x370.cpp` (109/3/0; `Map_0x370::LoadMap_4E95B0`, `FindGroundZForCoord_4E5B60`, `GetBlockSpec_4E00A0`, `ChangeBlock_4E8620`, `GetGangAtCoords_4DFB50`, `GetNearestZoneOfType_4DF240`).
- Skrzyżowania: `RouteFinder.cpp` (37/0/0; `Load_RGEN_588B30`).
- Styl: `gtx_0x106C.cpp` (58/0/0; `LoadSty_5AB750`, `LoadChunk_5AB4B0`, `SetDeltaDataPtrs_5AABF0`, `BuildCarInfoContainer_5AA9A0`).
- GXT: `text_0x14.cpp` (20/0/0; `TKEY_Load_5B5960`, `TDAT_Load_5B5A80`, `Find_5B5F90`).
- Audio: `cSampleManager.cpp` (63/0/0; `LoadWavSdtData_58E980`).
- GCI: `CarInfo_808.cpp` (28/0/0; `LoadFromGciFile_454A00`).
- Skrypty: `ScriptManager_C1EA8.cpp` (`LoadMissionScript_512400`, `LoadStringTbl_5121E0`).
- Pliki: `file.cpp` (19/0/0). Ustawienia: `registry.cpp` (31/0/0).
- SEQ i obrazy menu: `Frontend.cpp` (`LoadMapFilenames_4B4D00`, `Load_tgas_4B66B0`).

**Zależności** — szacunek
- Kolejne systemy (pojazdy, AI, skrypty, HUD) wymagają kolejno: GCI + `CARI` (pojazdy), `RGEN` + `ZONE` (AI ruchu), GXT w kliencie (HUD i briefy), SEQ (wybór poziomu).

**Testy**
- Istniejące (wszystkie `DataTests`, wymagają oryginalnych plików przez `OPENGTA2_PATH`) — fakt (kod):
  - `MapReaderTests` (`data/bil.gmp`: rozmiar 256×256, kolumna 41903, 16 animacji, 189 stref; test obiektów oczekuje 0),
  - `StyleReaderTests` (`data/bil.sty`: 992 kafle, `PaletteBase.Sprite == 2089`, wpisy palet),
  - `GtaStringReaderTests` (`data/e.gxt`: 2595 kluczy),
  - `ScriptParserTests` (`mike1m.SCR`, `wil.SCR`, `downtown-2p.SCR`, `MP2-2P.SCR`),
  - `ScriptInterpreterTests`.
- Bez danych gry: po `261f70f` istnieją testy `Category=Synthetic` dla RIFF, GXT, SCR i `Fixed16` (`src/OpenGta2.GameData.UnitTests/Synthetic/`) — fakt (`git show --stat`). Nadal brakuje syntetycznych GMP, STY i SDT/RAW. Każdy z czterech otwartych błędów powyżej da się złapać takim testem — szacunek.

---

## 3. Pojazdy, fizyka i kolizje

**Stan w OpenGTA2 — potwierdzone kodem**
- **Pojazdy: brak.** Nie ma klasy pojazdu ani fizyki — fakt (kod). Istnieją tylko:
  - parametry `GameData/Scripts/CommandParameters/SpawnCarParameters.cs` i wywołanie zwrotne `IScriptRuntime.SpawnCar`, którego jedyną implementacją jest `DebugConsole/LogScriptRuntime` (wypisuje na konsolę),
  - enum `DebugConsole/CarModel.cs` (z komentarzem, że służy tylko do debugowania).
- **Kolizje: podpięte, uproszczone.** `CollisionMap.CalculateMovement(Vector3 point, float radius, Vector2 delta)` (`src/OpenGta2.Client/CollisionMap.cs`) — fakt (kod):
  - działa wyłącznie w 2D na poziomie `(int)point.Z`; Z pieszego się nie zmienia,
  - sprawdza flagę `FaceInfo.Wall` ścian sąsiednich komórek, także ściany sąsiada (`MapExtensions.IsWall`),
  - po trafieniu zwraca punkt trafienia; ślizganie wzdłuż ściany jest zakomentowane (linie 52–62), więc ruch po prostu się zatrzymuje,
  - nie uwzględnia skosów, podłoża i lidów, spadania, `BulletWall` (parsowane, nieużywane), kolizji sprite–sprite ani pieszy–pieszy,
  - przy każdej detekcji wypisuje `Debug.WriteLine` i dopisuje `DiagnosticHighlight`.

**Brakuje** — fakt (kod: brak)
- Encji pojazdu, wsiadania i wysiadania, fizyki (siły, koła, poślizg, biegi), uszkodzeń i wybuchów.
- Przyczep, pociągów i autobusów, dźwigów, zgniatarek, garaży, drzwi, sygnalizacji.
- Kolizji 3D z mapą (skosy, wysokość podłoża), kolizji sprite'ów, kolejek zdarzeń kolizji.

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- Fizyka: `CarPhysics_B0.cpp` (118/0/0; `ApplyInputsAndIntegratePhysics_562F30`, `HandleCarCollision_55FF20`, `CalculateFrontWheelForce_561E50`, `BinarySearchCollisionTime_55C560`, `DoSkidmarks_55E260`).
- Pojazd: `Car_BC.cpp` (249/0/0; `CarsService_446790`, `ApplyImpactDamage_43D5D0`, `CanBeEnteredByPed_4451E0`), `Car_10.cpp` (8/0/0), `CarInfo_808.cpp` (28/0/0; `ComputeCarMassAndInertia_454410`).
- Kolizje: `collide.cpp` (3/0/0), `sprite.cpp` (`CollisionCheck_59E590`, `CheckMapZCollision_5A21F0`, `FindCollisionIntersectionPoint_5A2710`), `map_0x370.cpp` (`CanMoveOntoSlopeTile_4E0130` jako WIP).
- Infrastruktura: `PublicTransport.cpp` (41/1/0), `TrafficLights_194.cpp` (7/1/0), `Cranes.cpp`, `Crushers.cpp`, `Garage_48.cpp`, `Door_38.cpp`, `Door_4D4.cpp`, `CollisionSoundQueue_C88.cpp`.

**Zależności** — szacunek
- Parser GCI i `CARI`.
- Kolizja 3D z mapą (wysokość podłoża, skosy).
- Stały tick i decyzja: `Fix16` czy `float`. Jeśli porównujemy z oryginałem bit w bit, potrzebna jest arytmetyka stałoprzecinkowa jak w `fix16.cpp`.
- Sprite'y pojazdów w rendererze, sterowanie pojazdem.

**Testy**
- Istniejące: brak — fakt (kod).
- Bez GPU: `CollisionMap` przyjmuje sam `Map`, a `Map`/`CompressedMap`/`ColumnInfo`/`BlockInfo` to rekordy i struktury, które można zbudować w pamięci. Wystarczą testy „ruch w stronę ściany zatrzymuje się na promieniu”, „ruch równoległy przechodzi”, „ściana sąsiada też blokuje”. Dziś wymaga to referencji do projektu `-windows` albo przeniesienia klasy — szacunek.

---

## 4. Sterowanie graczem

**Stan w OpenGTA2 — potwierdzone kodem**
- `Controls.cs`: tylko klawiatura (`Keyboard.GetState()`), mapowanie na sztywno — Forward=`Up`, Backward=`Down`, Left=`Left`, Right=`Right`, Shoot=`LeftControl`, Menu=`Escape` (komentarz: „should be configurable at some point”) — fakt (kod).
- `Components/PlayerControllerComponent.cs` — **podpięte**, fakt (kod):
  - tworzy pieszego gracza na sztywnej pozycji `(11.5, 2.5, GetGroundZ(12, 2))` z remapem 25 (linia 29), a nie z polecenia `PLAYER_PED` w SCR,
  - obrót 2π rad/s, prędkość 2 bloki/s, F2 przełącza „wolny chód” ×0,1,
  - ruch przechodzi przez `CollisionMap`, animacja ustawiana jest na `Walking` albo `Idle`.
- `Control.Shoot` jest zmapowany, ale nigdzie nieużywany (grep) — fakt (kod).

**Brakuje** — fakt (kod: brak)
- Biegu, skoku, strzału, zmiany broni, wsiadania i wysiadania, sterowania pojazdem (gaz, hamulec ręczny, klakson).
- Konfiguracji klawiszy, pada, spadania i ruchu w osi Z.
- Spawnu gracza z danych SCR.

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- `Player.cpp` (78/0/0; `Player::HandleControls_5668D0`, `ControlInputs_566820`, `DoPedControlInputs_566C80`, `DoCarControlInputs_566C30`, `CharacterControls_566520`, `SetInputs_565740`, `RespawnPlayer_5670B0`).
- `InputRecorder_67F8B0.cpp` (35/0/0; `InputDevices_1::read_keyboard_and_gamepad_498CC0`, `game_pad_read_498D20`).
- `keybrd_0x204.cpp` (9/0/0; `LoadKbCfg_4D5E00`), `input.cpp` (DirectInput), `registry.cpp`.

**Zależności** — szacunek
- Wstrzykiwalne źródło wejścia (szew z sekcji „Architektura”).
- Pojazdy (sterowanie autem), broń (strzał), kolizje 3D (skok, spadanie).

**Testy**
- Istniejące: brak — fakt (kod).
- Bez GPU: `Controls` zasilany sztucznym `KeyboardState` (MonoGame pozwala go skonstruować z listy klawiszy — szacunek). Krok gracza: dane wejście + mapa → oczekiwana pozycja i obrót po N tickach przy stałym `dt`.

---

## 5. Piesi i AI

**Stan w OpenGTA2 — potwierdzone kodem**
- `Peds/Ped.cs`: `Position`, `Rotation`, `Remap`, dwie animacje (`Idle`: 12 klatek po 0,2 s, baza 53; `Walking`: 8 klatek po 0,06 s, baza 8), `UpdateAnimation(float dt)` — fakt (kod).
- `Peds/PedManager.cs`: zwykła `List<Ped>`. W grze jest jeden pieszy, czyli gracz — fakt (kod).
- **Brak AI** — fakt (kod).

**Brakuje** — fakt (kod: brak)
- Spawnu pieszych (generatory, strefy), zdrowia i śmierci, zachowań (chodzenie po chodnikach, ucieczka, atak, podążanie, wsiadanie do auta), grup, kierowców i ruchu ulicznego (AI pojazdów), znajdowania ścieżek, służb (karetki, straż).
- Typów pieszych i ich grafiki (remapy są tylko jako liczba).

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- Piesi: `Ped.cpp` (207/2/0; m.in. `AttackTargetStateMachine_46D460`, `FollowTargetStateMachine_46AC20`, `FleeOnFootTillSafe_4678E0`, `EnterCarOrFlee_467BD0`), `char.cpp` (69/1/1; `Char_B4::TickMovementStateMachine_54C900`, `CanStepForward_54FEC0`, `CheckAndHandleCollisions_5459C0`).
- Zarządzanie: `Char_Pool.cpp` (18/0/0; `PedManager::PedsService_4703F0`), `PedGroup.cpp` (38/0/0), `Ped_List_4.cpp`, `PedRefTable_7F8.cpp`.
- Ścieżki: `PathFinder_2FD4.cpp` (13/0/0; `ComputePath_554AB0`), `RouteFinder.cpp`.
- AI pojazdów: `CarAI_78.cpp` (31/2/0; `AI_Service_453BB0`, `FollowRoadDirection_44A1F0`).
- Spawn i służby: `Generators.cpp`, `EmergencyCrewPool_1E0.cpp`, `Ambulance_110.cpp`, `Firefighters.cpp`, `Taxi_4.cpp`.

**Zależności** — szacunek
- Kolizja 3D i typy podłoża (`GroundType` z `SlopeInfo` jest już parsowany).
- `RGEN` i strzałki ruchu (`BlockInfo.Arrows`), strefy (`ZONE`).
- Deterministyczny RNG, stały tick, broń i obrażenia (ataki), pojazdy (kierowcy).

**Testy**
- Istniejące: brak — fakt (kod).
- Bez GPU: `Ped.UpdateAnimation` — sekwencja klatek przy stałym `dt`. Ten test od razu pokaże wrażliwość na niecałkowite wielokrotności `dt` (porównanie `>` i odejmowanie `frameTime` w `float`) — szacunek.

---

## 6. Policja (wanted level)

**Stan w OpenGTA2 — potwierdzone kodem**
- **Brak.** Jedyne ślady to wartości enumów `ZoneType.ArrestRestart` i `ZoneType.Restart` (`GameData/Map/Models/ZoneType.cs`), wypełniane przez parser stref — fakt (kod).

**Brakuje**
- Całości: punktów i poziomu poszukiwania, zgłaszania przestępstw, wysyłania radiowozów, pościgów, blokad, aresztowania i respawnu po aresztowaniu, wskaźnika w HUD, policyjnego radia — fakt (kod: brak).

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- Dyspozytor: `Police_7B8.cpp` (22/0/0; `Police_7B8::Service_570270`, `RegisterCriminal_56F940`, `DispatchNewCrewToPursuit_56FAA0`, `TryBeginRoadblock_577320`).
- Ekipy i blokady: `Police_38.cpp` (29/1/0; `PoliceCrew_38`, `PoliceRoadblock_A4`, `PolicePursuitTarget_7C`, `CreateRoadblock_575FF0` jako WIP).
- Wanted przy pieszym: `Ped.cpp` (`add_wanted_points_470160`, `set_wanted_level_46EF40`, `get_wanted_star_count_46EF00`, `ClearWantedPoints_420B80`).
- Przestępstwa: `PlayerCrimeStats_3C.cpp` (8/0/0), `CrimeReportQueue_CC.cpp` (15/0/0).
- HUD: `Hud.cpp` (`UpdateWantedLevel_5D00B0`, `DrawWantedLevel_5D0110`).
- Gracz: `Player::Busted_5679E0`.
- Pomocnicze: `PatrolRoutePool_1D7E.cpp`, `CarChaseTaskTable_500.cpp`, `enums.hpp` (`cop_level_enum`).
- Skrypty: `miss2_0x11C.hpp` (`SCR_CMD_ALTER_WANTED_LEVEL`, `SCR_DECLARE_POLICELEVEL`).
- Radio: `sound_obj.cpp` (`EnqueueRadioCrimeCallout_427340`).

**Zależności** — szacunek
- AI pieszych i pojazdów, broń i obrażenia (przestępstwa), pojazdy (pościgi), HUD, RNG, strefy restartu.

**Testy**
- Istniejące: brak — fakt (kod).
- Bez GPU (po implementacji): czysta logika „zdarzenie przestępstwa → punkty → gwiazdki → zanik w czasie” na stałym ticku, z porównaniem do śladu z oryginału — szacunek.

---

## 7. Broń i obrażenia

**Stan w OpenGTA2 — potwierdzone kodem**
- **Brak** — fakt (kod). Ślady:
  - `Control.Shoot` (zmapowany, nieużywany),
  - `ScriptCommandType.GIVE_WEAPON`, `SET_CAR_BULLETPROOF`, `SET_CAR_FLAMEPROOF`, `SET_CAR_ROCKETPROOF` (tylko wartości enumu, interpreter ich nie obsługuje),
  - `FaceInfo.BulletWall` (parsowane, nieużywane).

**Brakuje** — fakt (kod: brak)
- Broni pieszych i pojazdów, amunicji, pocisków, trafień, zdrowia, śmierci, wybuchów, ognia, obrażeń pojazdów, power-upów.

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- Broń: `Weapon_30.cpp` (36/0/0; `pull_trigger_5E3670`, `spawn_bullet_5DCF60` oraz po jednej funkcji na typ broni, np. `pistol_5DD860`, `smg_5DDD20`, `flamethrower_5DD0F0`, `rocket_5E3850`), `Weapon_8.cpp` (11/0/0).
- Wybuchy: `Explosion_30.cpp` (28/0/0; `ApplyBlastDamage_541850`).
- Obiekty i pociski: `Object_5C.cpp` (106/0/0), `Object_3C.cpp`, `ObjectDefinitions_8CA8.cpp` — szacunek co do roli.
- Obrażenia pojazdów: `Car_BC.cpp` (`AccumulateDamage_43DA90`, `DamageArea_43CF30`).
- Ataki pieszych: `Ped.cpp` (`AttackPed_46DB60`, `ChooseAttackWeapon_46F490`, `GiveWeapon_46F650`).
- Gracz: `Player::AddWeaponWithAmmo_564960`, `SelectNextOrPrevWeapon_5649D0`.

**Zależności** — szacunek
- Kolizje sprite'ów i promieni z mapą (`BulletWall`), RNG (rozrzut, `Weapon_30.cpp` korzysta z `rng`), pojazdy, AI.

**Testy**
- Istniejące: brak — fakt (kod).
- Bez GPU (po implementacji): tor pocisku na syntetycznej mapie, zasięg wybuchu i obrażenia jako funkcje czyste — szacunek.

---

## 8. Gangi i system respektu

**Stan w OpenGTA2 — potwierdzone kodem**
- **Brak** — fakt (kod). Ślady:
  - `ZoneType.Gang` (strefy są parsowane do `Map.Zones` z nazwą, ale klient z nich nie korzysta),
  - `ScriptCommandType.CREATE_GANG_CAR018a..d` (tylko enum).

**Brakuje** — fakt (kod: brak)
- Modelu gangu (nazwa, kolory, ulubiony pojazd, broń), respektu per gracz, reakcji pieszych na respekt, zleceń z telefonów, wskaźnika respektu w HUD.

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- `Gang.cpp` (26/0/0; `Gang_144`, `GangPool_CA8`, `IncrementRespect_4BEE50`, `DecrementRespect_4BEEA0`, `ApplyKillRespectChange_4BEF70`, `GetRespectForPlayer_4BEEF0`, `IsHostileToGang_4BEDF0`, `alloc_gang_for_map_zone_4BF1E0`).
- `Ped::ApplyGangRespectForKill_46F680`, `Map_0x370::GetGangAtCoords_4DFB50`.
- Skrypty: `miss2_0x11C.hpp` (`SCR_SET_GANG_INFO`, `SCR_IS_CHAR_IN_GANG_ZONE`); telefony: `ScriptManager_C1EA8` (`AddPhoneThread_5129B0`, `AnswerPhone_5129F0`).

**Zależności** — szacunek
- Strefy (`ZONE`, już parsowane), piesi z przynależnością do gangu, broń i zabójstwa, skrypty (`SET_GANG_INFO` definiuje gangi na poziomie), HUD, zapis.

**Testy**
- Istniejące: brak — fakt (kod). `MapReaderTests.Read_should_read_zones` pośrednio pokrywa parsowanie stref (DataTests).
- Bez GPU (po implementacji): tabela „zabójstwo członka gangu X → zmiany respektu X/Y/Z” jako test danych — szacunek.

---

## 9. Misje i skrypty (SCR)

**Stan w OpenGTA2 — potwierdzone kodem**
- **Parser** (`GameData/Scripts/Parsing/ScriptParser.cs`): zaimplementowany. Typ napisu (`StringType`) to pusty enum — fakt (kod).
- **Interpreter** (`GameData/Scripts/Interpreting/ScriptInterpreter.cs`) — fakt (kod):
  - `Run(Script, IScriptRuntime)` przechodzi **liniowo jeden raz** po wszystkich 6000 wskaźnikach i dekoduje nagłówek polecenia (`ScriptCommand`: `PtrIndex`, `Type`, `NextPtrIndex`, `Flags`),
  - obsługuje tylko `PARKED_CAR_DATA` (→ `SpawnCar`) i `PLAYER_PED` (→ `SpawnPlayerPed`); reszta trafia do `UnknownCommand` z komentarzem „`// TODO: Handle other commands`” (linia 38),
  - nie ma wątków, skoków (`IF_JUMP`, `GOSUB`), liczników, zmiennych, oczekiwania ani wykonywania per tick; `NextPtrIndex` nie jest używany,
  - nawet `CAR_DATA` (0x0009), choć jest w enumie, nie jest obsługiwane.
- `IScriptRuntime` ma 3 metody, w tym `UnknownCommand` z komentarzem „`// TODO: This method should eventually disappear when all commands have been implemented.`” — fakt (kod).
- `ScriptCommandType` wylicza 39 kodów operacji (część z dopiskiem adresu zamiast nazwy, np. `GENERATOR001C`, `OBJ_DATA0014`) — fakt (kod).
- **Niepodpięte**: klient nie odwołuje się do `ScriptParser` ani `ScriptInterpreter` (grep). Jedyny konsument to zakomentowany kod w `DebugConsole/Program.cs` — fakt (kod).

**Brakuje** — fakt (kod: brak)
- Maszyny wątków skryptu, przepływu sterowania, liczników i porównań, timerów, wyzwalaczy, briefów i pagerów (GXT), telefonów, celów misji, nagród, zakończenia poziomu.
- Ponad 200 poleceń (zob. niżej), mostu między skryptem a encjami gry.

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- Interpreter: `miss2_0x11C.cpp` (262/1/0; 228 różnych metod `miss2_0x11C::SCRCMD_*`; rdzeń: `ExecOpCode_5061C0`, `PreExecOpCode_5108D0`, `Next_503620`, `Gosub_50CE50`, `SpawnThread_511960`; struktury parametrów `SCR_*` w `miss2_0x11C.hpp`).
- Menedżer: `ScriptManager_C1EA8.cpp` (35/0/0; `ExecuteScriptThreads_5127A0`, `LoadMissionScript_512400`, `LoadSubScripts_5125F0`, wątki aut i telefonów).
- Stos i ramki: `ScriptStackFramePool_25C.cpp`, `miss2_8.cpp`, `Miss2_25C.cpp`.
- Tryby i bonusy: `BonusTracker_1C0.cpp`, `TagGame_28.cpp`, `Taxi_4.cpp`.
- Wyświetlanie: `Hud.cpp` (`AllocBrief_5D33F0`, `DrawBrief_5D3B80`, strzałki).

**Zależności** — szacunek
- Rdzeń interpretera (wątki, skoki, liczniki) można zrobić niezależnie, z runtime'em-zaślepką i deterministycznym tickiem.
- Pełne polecenia wymagają niemal wszystkich systemów: encji (piesi, pojazdy, obiekty), GXT i HUD, policji, gangów, zapisu (`SCR_DO_SAVE_GAME`).

**Testy**
- Istniejące (DataTests) — fakt (kod):
  - `ScriptParserTests`: 4 pliki — liczba wskaźników, rozmiar danych, liczba napisów,
  - `ScriptInterpreterTests.Run_should_call_runtime_SpawnCar`: na `data/Industrial-2P.scr` oczekuje dokładnie 70 wywołań `SpawnCar` z `PARKED_CAR_DATA` (Moq).
- Bez danych gry: syntetyczny SCR z ręcznie złożonymi poleceniami (parser jest już tak testowany od `261f70f`: `Synthetic/ScriptParserSyntheticTests.cs`). Dla interpretera: test „tablica wskaźników → oczekiwana sekwencja wywołań runtime'u”, a po dodaniu wątków — ślad wykonania per tick — szacunek.

---

## 10. Dźwięk

**Stan w OpenGTA2 — potwierdzone kodem**
- `GameData/Audio/SdtReader.cs`: tablica `SoundEntry` (`Offset`, `Size`, `SampleRate`, `VariationSampleRate`, `LoopStart`, `LoopEnd`). `RawReader.cs` wczytuje cały RAW do pamięci. `SoundLibrary.GetSound(int)` składa nagłówek WAV (PCM16, mono) i zwraca strumień; częstotliwość jest losowo zmieniana o ±`VariationSampleRate` przez niezasiany `Random`. TODO „`// TODO: no allocation`” (linia 24) — fakt (kod).
- `Components/AudioTestComponent.cs` — **podpięte (test)**: w konstruktorze wczytuje na sztywno `data/Audio/bil.sdt` i `data/Audio/bil.raw`; `Tab` odtwarza losowo próbkę 309 albo 310 przez `SoundEffect.FromStream(...).Play()`, za każdym razem tworząc nowy `SoundEffect` bez zwalniania — fakt (kod).
- `LoopStart`/`LoopEnd` są parsowane, ale nieużywane — fakt (kod).
- Błąd odczytu w `SoundStream.Read` — zob. sekcja 2, punkt 2.
- Dźwięk intra: `IntroComponent` przez DirectSound (`Dsound.dll`, `DirectSoundCreate8`) i Bink — fakt (kod); niepodpięte.

**Brakuje** — fakt (kod: brak)
- Dźwięku pozycyjnego 3D, silników i poślizgów, kolizji, broni, głosów pieszych, radia w pojazdach (stacje, `RADIO_STATION` tylko w enumie), policyjnego radia, muzyki, zapętleń, miksowania i głośności, ustawień.

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- Logika: `sound_obj.cpp` (148/1/0; m.in. `HandleAICarEngineSound_418190`, `_HandleCarSkidSound_418940`, `CycleRadioStation_57EEE0`, `EnqueueRadioWord_4271B0`).
- Serwis: `root_sound.cpp` (22/0/0; `Service_40EFA0`, `DeclareRadioStation_40F030`, `PlayVoice_40F090`).
- Próbki i strumienie: `cSampleManager.cpp` (63/0/0; `LoadWavSdtData_58E980`, `OpenStream_58E320`, `PlayVocal_58E510`).
- Pozostałe: `SoundObject_10.cpp`, `CollisionSoundQueue_C88.cpp`, `voice_line.hpp`; backend Miles Sound System w `3rdParty/mss.h`.

**Zależności** — szacunek
- Encje generujące zdarzenia (pojazdy, broń, kolizje), kamera i słuchacz (pozycja), wydzielony RNG audio (żeby losowość dźwięku nie wpływała na determinizm symulacji).

**Testy**
- Istniejące: brak — fakt (kod).
- Bez GPU i bez urządzenia audio: syntetyczne SDT/RAW → sprawdzenie bajtów nagłówka WAV i zawartości danych; test odczytu strumienia porcjami (złapie błąd z sekcji 2, punkt 2). Z zasianym `Random` — deterministyczna częstotliwość próbki — szacunek.

---

## 11. Zapisywanie stanu gry

**Stan w OpenGTA2 — potwierdzone kodem**
- **Brak.** W `src/` nie ma zapisu plików (`File.Create`, `FileStream` do zapisu, `StreamWriter`: grep pusty), serializacji stanu ani formatu zapisu — fakt (kod).

**Brakuje** — fakt (kod: brak)
- Zapisu i odczytu gry (slot gracza, postęp etapów, wynik, mnożniki, życia, broń, zapisane auta, liczniki skryptu), tabel wyników, ustawień.
- **Z punktu widzenia frameworka porównawczego:** migawki pełnego stanu symulacji (jak w sekcji „Szwy”). To inny cel niż zapis gry, ale wymaga tej samej warstwy serializacji — szacunek.

**Odpowiednik w GTA2_RE** (`Source/`, M/W/S)
- Profil i wyniki: `PlyDat_2BC0.cpp` (29/0/0; `PlyDat_2BC0`, `PlySlot_A4`, `SavePlySlotDat_56BA60`, `LoadPlySlotDat_56B990`, `SaveHiScores_56BF20`, `UnlockStage_56BBD0`).
- Zapis gry: `ScriptManager_C1EA8.cpp` (`SaveGame_511E10`, `LoadSave_511F80`, `SaveScriptCounters_511B90`, `SaveMapInfo_511D40`; struktury `SaveData_748`, `SavedCarAndScriptData_500`, `SavedPlayerStats_90`).
- Gracz: `Player.cpp` (`CopyPlayerDataToSave_56A1A0`, `UpdateGameFromSave_56A310`, `RestoreCarsFromSave_56A0F0`, `RestorePowerUpsFromSave_5651F0`).
- Menu: `Frontend.cpp` (`LoadPlySlotSvgs_4B53C0`). Skrypty: `SCR_DO_SAVE_GAME`. Ustawienia: `registry.cpp` (rejestr Windows). Sesja: `GameSession_578.cpp` (45/0/0).

**Zależności** — szacunek
- Zapis gry ma sens dopiero przy istniejących systemach gracza, broni, pojazdów, skryptów (liczniki) i postępu poziomów.
- Migawki stanu do porównań warto zrobić od razu razem z wydzieloną symulacją.

**Testy**
- Istniejące: brak — fakt (kod).
- Bez GPU: round-trip „stan → bajty → stan” i stabilność skrótu stanu po N tickach dla tego samego wejścia i ziarna — szacunek.

---

## Zależności platformowe

Cel: macOS ARM64, Windows i Linux przez MonoGame DesktopGL. Lista obejmuje wszystko, co w kodzie przy `2b72b4d` wiąże projekt z Windows/x86/DirectX.

| # | Zależność | Miejsce | Wpływ na DesktopGL / macOS ARM64 | Pewność |
|---|---|---|---|---|
| 1 | `TargetFramework` `net10.0-windows`, `OutputType` `WinExe`, `UseWindowsForms=true`, `ApplicationHighDpiMode=PerMonitorV2`, `ApplicationManifest=app.manifest` | `src/OpenGta2.Client/OpenGta2.Client.csproj` | Projekt nie zbuduje się na macOS/Linux bez zmiany TFM i usunięcia WinForms | fakt (kod); skutek — szacunek |
| 2 | `PlatformTarget=x86` | j.w. | Wymusza proces 32-bitowy; na macOS ARM64 nie ma środowiska x86 .NET (Rosetta obsługuje tylko x64) | fakt (kod); skutek — szacunek |
| 3 | Pakiet `MonoGame.Framework.WindowsDX` 3.8.4.1 | j.w. | Trzeba go zamienić na `MonoGame.Framework.DesktopGL` | fakt (kod) |
| 4 | `using SharpDX.MediaFoundation;` (nieużywany import) | `src/OpenGta2.Client/GtaGame.cs:7` | Bez SharpDX (DesktopGL go nie ma) to błąd kompilacji | fakt (kod); błąd kompilacji — szacunek |
| 5 | P/Invoke `Kernel32.dll` (`LoadLibrary`), `binkw32.dll` (10 funkcji Bink), `Dsound.dll` (`DirectSoundCreate8`), delegat `StdCall`, struktura `Bink*` w kodzie `unsafe` | `src/OpenGta2.Client/Components/IntroComponent.cs:171–212` | `binkw32.dll` z instalacji gry to 32-bitowa biblioteka Windows; poza Windows x86 nie da się jej załadować. Wyjątek jest łapany (`_failure = true`), więc intro zostanie pominięte zamiast wysypać grę. Trzeba zamiennika (inny dekoder BIK) albo wyłączenia | fakt (kod); dekoder — szacunek |
| 6 | `/platform:Windows` w Content Pipeline | `src/OpenGta2.Client/Assets/Assets.mgcb` | Treści budowane dla DirectX. Pakiet `MonoGame.Content.Builder.Task` może nadpisać platformę wg właściwości `MonoGamePlatform` | fakt (kod); nadpisywanie — szacunek |
| 7 | Shadery: gałąź `#else` (DirectX) ma `vs_4_0`/`ps_4_0`, gałąź `#if OPENGL` ma `vs_3_0`/`ps_3_0` | `Assets/Effects/BlockFaceEffect.fx`, `ScreenspaceSpriteEffect.fx`, `DebugLineEffect.fx` | Szkielet na GL jest, ale zob. 8 i 9 | fakt (kod) |
| 8 | `Texture2DArray Tiles` + `Tiles.Sample(...)` z 3-składnikowym UV; po stronie C# `new Texture2D(..., arraySize: tileCount)` | `BlockFaceEffect.fx:15,71`; `Levels/StyleTextureSet.cs:92` | Shader Model 3 (ścieżka OpenGL w MonoGame) nie ma tablic tekstur, więc ten efekt prawdopodobnie nie skompiluje się dla DesktopGL. Rozwiązania: atlas 2D kafli albo inny backend. **To główny bloker renderu mapy na GL.** | fakt (kod); niekompatybilność — szacunek (wysokie prawdopodobieństwo) |
| 9 | Składnia obiektów DX10 (`Texture2D` + `.Sample`) w shaderze sprite'ów | `ScreenspaceSpriteEffect.fx:13,43` | Dokumentacja MonoGame dla OpenGL zaleca `tex2D` przez makro. `.Sample` przy `ps_3_0` może wymagać zmiany | szacunek |
| 10 | Kompilacja efektów (`mgfxc`) na macOS/Linux | narzędzia MonoGame | Wymaga Wine według dokumentacji MonoGame (Context7: „Setting up your OS for development” — macOS/Ubuntu, skrypt `mgfxc_wine_setup.sh`). Alternatywa: prekompilowane `.xnb` w repo albo budowanie treści w CI na Windows | fakt (dokumentacja MonoGame) |
| 11 | Czcionka `Consolas` | `Assets/Fonts/DebugFont.spritefont` | Czcionki Windows; na macOS/Linux zwykle jej nie ma, więc budowanie treści się nie powiedzie | fakt (kod); dostępność — szacunek |
| 12 | Wersje narzędzi: `dotnet-mgcb*` 3.8.1.303 w manifeście vs pakiety MonoGame 3.8.4.1 | `src/OpenGta2.Client/.config/dotnet-tools.json` vs `.csproj` | Niespójność wersji; warto je wyrównać przy migracji. Manifest zawiera już edytory dla mac/linux | fakt (kod) |
| 13 | `Environment.GetEnvironmentVariable("OPENGTA2_PATH", EnvironmentVariableTarget.User)` | `Client/TestGamePath.cs:8`, `GameData.UnitTests/TestGamePath.cs:6`, `DebugConsole/TestGamePath.cs:4` | Na Unixie cel `User` nie jest obsługiwany i zwraca `null`, przez co gra i testy nie znajdą danych. Commit `261f70f` poprawia 2 z 3 plików; `DebugConsole/TestGamePath.cs` przy `69109837` nadal ma stary odczyt | fakt (kod); zachowanie .NET — fakt (dokumentacja .NET), potwierdzone komentarzem w `261f70f` |
| 14 | Wielkość liter w ścieżkach: `data/Audio/bil.sdt`, `data/Movie/intro.bik`, `data/mike1m.SCR`, `data/wil.SCR`, `data/downtown-2p.SCR`, `data/MP2-2P.SCR`, `data/Industrial-2P.scr`, `data/bil.gmp`, `data/e.gxt`, `binkw32.dll` | `AudioTestComponent.cs:17,22`, `IntroComponent.cs:41,51,57`, `GtaGame.cs:44–49`, testy | Rozszerzenia mają różną wielkość liter (`.SCR` i `.scr`). Oryginalny exe odwołuje się do `data\audio\` małymi literami (łańcuch w `gta2_re/Source/*.cpp`), a OpenGTA2 do `data/Audio/`. Na systemie plików rozróżniającym wielkość liter (Linux; opcjonalnie macOS) ścieżki mogą nie trafić. Trzeba wyszukiwania plików bez względu na wielkość liter | fakt (kod) co do łańcuchów; zgodność z dyskiem — szacunek (brak danych gry) |
| 15 | Separatory ścieżek | cały `src/` | W kodzie C# są wyłącznie `/` (bezpieczne). Backslashe występują tylko w elementach MSBuild w `.csproj` (MSBuild je normalizuje) | fakt (kod) |
| 16 | Rozdzielczość 1920×1080 i `GraphicsProfile.HiDef` na sztywno, `MultiSampleCount = 4` ustawiane na `GraphicsDevice` w konstruktorze `GtaGame` przed `Run()` | `GtaGame.cs:22–33` | Na DesktopGL może się zachowywać inaczej (kolejność tworzenia urządzenia, HiDef a SM3) | fakt (kod); skutek — szacunek |
| 17 | Klawisze numeryczne (`NumPad0..9`) do kamery, `OemTilde`/`OemPlus`/`OemMinus` | `CameraComponent.cs`, `MapComponent.cs`, `PedManagerComponent.cs` | Na laptopach Mac nie ma klawiatury numerycznej, a klawisze `Oem*` zależą od układu | fakt (kod); wpływ — szacunek |
| 18 | `.editorconfig`: `end_of_line = crlf` | `.editorconfig` | Nie blokuje budowania; przy edycji na macOS/Linux może generować szum w diffach | fakt (kod) |
| 19 | `OpenGta2.GameData`, `DebugConsole`, `UnitTests` | `*.csproj` | Czyste `net10.0`, bez P/Invoke. Odczyt struktur przez `MemoryMarshal` zakłada little-endian, co spełniają x64 i ARM64 | fakt (kod) |
| 20 | Lokalne SDK | maszyna audytora | Zainstalowane jest tylko .NET SDK 8.0.301, a projekty wymagają .NET 10 (`global.json` z `261f70f` przypina 10.0.100) | fakt (środowisko) |

---

## Priorytety (rekomendacja)

Kolejność według zależności: najpierw to, co odblokowuje testy i determinizm, potem systemy, od których zależą kolejne. Całość to **rekomendacja (szacunek)**.

1. **Budowanie na macOS/Linux bez klienta.** Częściowo zrobione w `261f70f`/`6910983`: `global.json`, testy syntetyczne, `OpenGta2.CrossPlatform.slnf`. Do zrobienia: .NET 10 na maszynach deweloperskich, poprawka `TestGamePath` w `DebugConsole` oraz 4 otwarte błędy z sekcji 2 (remap `CodeObj`, `SoundStream.Read`, dekodowanie GXT, heurystyka `GetGroundZ`), każdy z testem syntetycznym.
2. **Wydzielenie biblioteki symulacji** (`net10.0`, bez `Microsoft.Xna.Framework.Graphics`): `CollisionMap`, `Ped`, `PedManager`, model wejścia, „świat” z listą encji i jedno `Step(TickInput)`. Bez tego żaden test logiki gry nie ruszy poza Windows. `LevelProvider` rozdzielić na część danych i część renderu.
3. **Fundament determinizmu.** Szkielet już jest w `src/OpenGta2.Conformance` (`FixedStepClock`, `DeterministicRandom`, `InputTimeline`, `ISimulationAdapter`), ale bez adaptera prawdziwej gry. Brakuje adaptera OpenGTA2 nad biblioteką z punktu 2, a w samej grze: stały tick (wzorem `Game_0x40::GetFrameDurationMs_4B8BB0`: stała długość kroku), wejście wstrzykiwane przez `Controls`/`IInputSource`, zasiany RNG jako usługa (osobny dla audio), migawka stanu po każdym ticku, usunięcie statycznego `DiagnosticHighlight` z toru symulacji. Decyzja `Fix16` czy `float`: przy porównaniach z oryginałem stałoprzecinkowy `Fix16` usunie rozbieżności typu x64/ARM64. Kolejność serwisów w ticku warto od razu wzorować na `UpdateGame_4B9410`, a format nagrań na `InputRecorder_67F8B0` (wejście + indeks RNG).
4. **Port klienta na DesktopGL:** zmiana pakietu i TFM, usunięcie `using SharpDX…`, `PlatformTarget` AnyCPU, przepisanie `BlockFaceEffect` bez `Texture2DArray` (atlas), poprawka `.Sample` w shaderach GL, czcionka dostępna na wszystkich systemach, wyszukiwanie plików bez względu na wielkość liter, intro BIK za flagą Windows albo z innym dekoderem. Ten krok można robić równolegle z 2–3, bo nie blokuje symulacji.
5. **Kolizje 3D z mapą:** wysokość podłoża i skosy (`SlopeType`, `GroundType`), ślizganie po ścianach, Z pieszego. Bez tego nie ma sensownego chodzenia, pojazdów ani AI.
6. **Rdzeń interpretera SCR:** wątki, skoki, liczniki, wykonywanie w ticku, przy runtime'ie-zaślepce. Testowalny bez GPU i bez reszty gry, a daje „szkielet” poziomu (spawn gracza z `PLAYER_PED`, zaparkowane auta).
7. **Pojazdy:** parser GCI i `CARI`, encja, fizyka, wsiadanie i wysiadanie, sprite'y pojazdów, kamera śledząca.
8. **Piesi i AI:** parser `RGEN`, strzałki ruchu, strefy, spawn, AI pojazdów.
9. **Broń i obrażenia**, potem **policja (wanted)**, potem **gangi i respekt**. Każde zależy od poprzednich.
10. **HUD i GXT w kliencie**, rozwijane razem z 6–9, bo skrypty i policja potrzebują briefów, strzałek i gwiazdek.
11. **Dźwięk pozycyjny i radio:** po encjach generujących zdarzenia.
12. **Zapis gry:** na końcu, z wykorzystaniem warstwy serializacji powstałej w punkcie 3.
