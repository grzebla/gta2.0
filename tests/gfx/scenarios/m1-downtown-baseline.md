# `m1-downtown-baseline` — referencyjny zrzut oryginalnej gry (przebieg odkrywczy)

**Rodzaj: referencyjny zrzut ORYGINALNEGO GTA 2** (`provenance: originalGta2` dla wszystkiego, co z
niego powstanie). To nie są dane syntetyczne ani render naszego silnika. Pole `provenance` celowo
**nie** występuje w [m1-downtown-baseline.json](m1-downtown-baseline.json): host EC2 przyjmuje
tylko pola `id`, `wrapper`, `screen`, `durationSeconds`, `screenshotsAtSeconds`, `inputs`, więc
oznaczenie jest w tym dokumencie. Zrzuty zostają w lokalnym katalogu wyników orkiestratora — nigdy
w repozytorium, w artefaktach CI ani w miejscach publicznych.

Statusy jak w [world-geometry.md](../../../docs/specs/world-geometry.md) (sekcja 0): **[D]** pomiar
lokalny na plikach danych, **[H]** hipoteza, **[I]** fakt o naszym kodzie, **[O]** obserwacja z
przebiegu oryginału. Przebieg odkrywczy `local-20261013-084136` się odbył — jego wynik jest w
sekcji 8; sekcje 1–7 opisują plan sprzed przebiegu.

## 1. Cel i zakres

Pierwszy, ograniczony przebieg **odkrywczy i kalibracyjny** (milestone M1, geometria świata), nie
pomiar w zadanym miejscu mapy. Ma odpowiedzieć na pytania:

1. Czy z ekranu startowego da się dojść do świata Downtown samymi naciśnięciami ENTER i po ilu.
2. Ile trwa start gry i ładowanie poziomu na hoście (DDrawCompat, 640×480).
3. Jak wyglądają klatki świata, gdy gracz stoi bez ruchu (materiał do WG-PROJ: `K`, `r`, `c`,
   [world-geometry.md](../../../docs/specs/world-geometry.md) sekcja 6.2) i czy kamera w spoczynku
   jest stabilna (tamże, pkt 4).
4. Gdzie w mapie `wil.gmp` jest kamera — ustalane **ze zrzutu** przez dopasowanie punktów
   orientacyjnych (sekcja 6), a nie zakładane.

Scenariusz nie prowadzi pieszego do żadnych współrzędnych i nie twierdzi, gdzie pieszy startuje.

## 2. Plik scenariusza i walidacja

| Pole | Wartość | Uzasadnienie |
|---|---|---|
| `id` | `m1-downtown-baseline` | = nazwa pliku |
| `wrapper` | `ddrawcompat` | klucz `wrappers` w `tests/gfx/compat-matrix.json`; host ma według zadania GTA 2 + DDrawCompat |
| `screen` | `{"start_mode": 1}` | pełny ekran; nieudane okno i tak po cichu przełącza grę na pełny ekran ([gpu-testing.md](../../../docs/ci/gpu-testing.md)) |
| `durationSeconds` | `120` | start + menu + ~30 s spoczynku + zapas na wolniejszy start |
| `screenshotsAtSeconds` | 20 chwil, 6…118 | sekcja 4 |
| `inputs` | 3 × `ENTER` (150 ms) w 15, 25, 65 s | sekcja 3 |

Walidacja offline (nie kontaktuje hosta, nie wymaga danych gry):

```bash
cmake -DSCENARIO=tests/gfx/scenarios/m1-downtown-baseline.json -P cmake/ValidateGfxScenario.cmake
ctest --preset headless -R '^gfx\.'   # gfx.scenario.* + syntetyczne gfx.scenario-check.*
```

`cmake/ValidateGfxScenario.cmake` sprawdza reguły hosta (pola, `id` = plik, `wrapper` z macierzy,
`durationSeconds` 10..300, 1..20 różnych chwil zrzutów, ≤ 100 wejść, dozwolone klawisze,
`holdMs` ≤ 5000) oraz kilka reguł **ostrzejszych niż host**, żeby przebieg był czytelny: chwile
zrzutów rosnące i w 1..`durationSeconds`−1, `screen` tylko z kluczami użytymi w macierzy
(`baseScreen` + `cases`), `start_mode` ∈ {0, 1}, wejścia w kolejności czasu, klawisz puszczony
przed końcem, brak zrzutu w trakcie trzymania klawisza, brak powtórzonych kluczy JSON i tekstu
poza obiektem. Przykłady nieprawidłowe są syntetyczne (`synthetic-*`,
`tests/gfx/scenario-check/ScenarioCheckTest.cmake`).

## 3. Założenia startowe (wszystkie [H], do potwierdzenia tym przebiegiem)

* **Wersja gry.** Opis hosta mówi o GTA 2 9.6, macierz wskazuje archiwum `game/gta2-10.5.zip`.
  Mapy i style obu wersji są identyczne ([gmp.md](../../../docs/formats/gmp.md), „9.6 = 10.5”),
  frontend może się różnić. Wersję trzeba odczytać z wyników przebiegu.
* **Ekran.** Zakładamy, że host łączy `baseScreen` z macierzy (`do_play_movie=0` — bez filmów
  Bink, 640×480) z `screen` scenariusza tak jak dla przypadków `compat`, oraz że pełny ekran
  działa w 16 bpp.
* **Czas.** Sekunda 0 = uruchomienie `gta2.exe`; wejścia trafiają jako kody skanowania do okna gry,
  które ma fokus.
* **Profil.** Świeży profil gracza (brak zapisanych graczy i postępu), więc dostępny jest tylko
  pierwszy poziom.
* **Frontend.** Lokalne nazwy plików w `data/frontend/` [D] sugerują ekran 1 z pozycjami
  Play/Options/Quit i ekran 2 z polem imienia, poziomami 1–3, bonusami, ligą i restartem. Nie
  wiadomo, która pozycja jest domyślnie podświetlona ani czy pole imienia wymaga wpisania tekstu.
* **Oczekiwany przebieg:**
  * ENTER @ 15 s — menu główne gotowe (≈ 12 s rozgrzewki jak `observe.warmupSeconds` w macierzy);
    aktywuje domyślną pozycję (oczekiwane „Play”) → ekran 2.
  * ENTER @ 25 s — aktywuje domyślną pozycję ekranu 2. Jeśli to poziom 1, ładuje się Downtown; jeśli
    pole imienia, gra przejdzie do edycji imienia i świat się nie pojawi (wynik też użyteczny: v2
    scenariusza doda `DOWN`/litery).
  * 28–64 s — **okno spoczynku**: żadnych wejść. Jeśli świat się załadował, pieszy stoi, a kamera
    powinna być nieruchoma.
  * ENTER @ 65 s — celowo **po** oknie spoczynku. W menu przesuwa przebieg dalej (np. zatwierdza
    imię); w świecie ENTER jest prawdopodobnie klawiszem „wsiądź do pojazdu” — wtedy zrzuty 68–118
    pokażą jego skutek (np. ruch pieszego i kamery), ale nie zepsują klatek spoczynku sprzed 65 s.
* **Dzielnica.** Oczekiwany pierwszy poziom = Downtown (`data/wil.gmp` + `data/wil.sty`). Nie
  potwierdzamy tego napisem w menu, tylko dopasowaniem do mapy (sekcja 6); brak jednoznacznego
  dopasowania do `wil.gmp` przy udanym dopasowaniu do `ste`/`bil` oznacza inną dzielnicę.

## 4. Harmonogram zrzutów

| Sekundy | Spodziewany stan [H] | Po co |
|---|---|---|
| 6, 12 | ekran startowy / menu główne | czas startu, czy menu jest gotowe przed pierwszym ENTER |
| 18, 22 | ekran 2 frontendu | skutek ENTER @ 15 |
| 28, 32, 36 | ładowanie albo pierwsze klatki świata | skutek ENTER @ 25, czas ładowania |
| 40, 45, 50, 55, 60, 64 | świat w spoczynku | stabilność kamery, `K`/`r`/`c`, lokalizacja (sekcja 6) |
| 68, 72 | skutek ENTER @ 65 | rozróżnienie menu / świata po trzecim ENTER |
| 80, 90, 100, 110, 118 | świat (spoczynek albo po wejściu do pojazdu) | druga seria do porównania stabilności, wykrycie zawieszenia gry |

Żaden zrzut nie wypada w trakcie trzymania klawisza (sprawdza walidator).

## 5. Klasyfikacja klatek: menu czy użyteczny świat

Każdy zrzut dostaje jedną etykietę; tylko `WORLD` służy do pomiarów geometrii.

| Etykieta | Rozpoznanie |
|---|---|
| `DESKTOP` | obraz jak zrzut pulpitu sprzed startu (`baseline-desktop.png`) albo proces nie żyje — gra nie wystartowała lub padła |
| `BLACK` | ≥ 97 % czarnych pikseli (próg `maxBlackRatio` z macierzy) — start, ładowanie, przełączanie trybu |
| `MENU` | statyczna, pełnoekranowa grafika frontendu: między kolejnymi zrzutami bez wejścia zmienia się najwyżej podświetlenie/animacja tekstu; **brak** siatki kafli 64×64 dopasowującej się do `wil.sty`; lokalnie można porównać z obrazami `data/frontend/*.tga` (tylko lokalnie, bez zapisywania wyników w repo) |
| `WORLD` | dopasowanie punktów orientacyjnych z sekcji 6 daje **jednoznaczne** położenie w `wil.gmp` (jedno okno mapy, wyraźnie lepsze od drugiego) |
| `WORLD_OCCLUDED` | rozpoznana siatka kafli, ale dopasowanie niejednoznaczne lub zasłonięte (tekst misji, pager, okno dialogowe, pauza) — nie do pomiarów |
| `UNKNOWN` | wszystko inne; opisać ręcznie |

Rozstrzyga kryterium obiektywne (jednoznaczne dopasowanie do mapy), a nie ocena „na oko”.
**Klatki spoczynku** = co najmniej dwie kolejne klatki `WORLD` bez wejścia pomiędzy (np. 40…64 s).
Kamera jest „stabilna”, gdy wyznaczony środek i skala (sekcja 6) różnią się między nimi o mniej niż
1 px; różnice samych pikseli mogą pochodzić z animowanych kafli, pieszych i pojazdów — maskować je
przed porównaniem. HUD (wynik, mnożnik, życie, pager — lista [H]) maskować stałymi prostokątami ustalonymi na
pierwszej klatce `WORLD`.

## 6. Lokalizacja kamery z lokalnych punktów orientacyjnych mapy

Zasada: **nie zakładamy, że kamera jest wycentrowana na pieszym** (w [world-geometry.md](../../../docs/specs/world-geometry.md)
sekcja 7 to [H]). Położenie kamery wyznacza się z samego obrazu świata, a pieszy jest maskowany jak
inne obiekty ruchome. Jego położenie na ekranie zapisuje się osobno i dopiero porównanie obu daje
zmierzone przesunięcie kamery względem pieszego.

**Dlaczego to działa [D].** Lokalny pomiar na `wil.gmp` (jednorazowy skrypt poza repo, tylko liczby
zbiorcze, DMAP, komórki x, y ∈ 1..254): dla każdej kolumny wzięto słowo najwyższej pokrywy z
kaflem ≠ 0 i policzono, jaka część okien k×k ma w całej mapie wzór występujący dokładnie raz.

| k | unikalne okna — sam numer kafla | unikalne okna — kafel + flat/flip/obrót |
|---|---|---|
| 3 | 52,0 % | 61,0 % |
| 4 | 74,8 % | 81,2 % |
| 5 | 86,9 % | 91,0 % |
| 6 | 93,1 % | 95,3 % |
| 8 | 97,6 % | 98,0 % |

Okna nieunikalne to prawdopodobnie obszary jednolite (woda, powtarzalne dachy) [H]. Najwyższe
pokrywy leżą na poziomach 0–6 (najczęściej 1: 28 362 z 65 536 kolumn) [D]. Kadr 640×480 obejmuje według modelu
roboczego wyraźnie więcej niż 8×8 bloków na poziomie gruntu [H], więc okno 6×6–8×8 kafli z
okolicy środka kadru zwykle wystarczy.

**Procedura (lokalnie, na zrzutach z katalogu wyników; narzędzie do napisania w zadaniu
następczym):**

1. **Katalog kafli** — lokalnie z `GTA20_GAME_DIR`: obrazy pokryw z `wil.sty` (nasz parser stylu,
   [I]) w 4 obrotach × odbicie, oraz dla każdej kolumny `wil.gmp` słowo najwyższej pokrywy i jej
   poziom (`GmpMap`, [I]). Nic z tego nie trafia do repo.
2. **Skala lokalna** — w środkowej części kadru (poza maską HUD i obiektów) wyznaczyć okres i fazę
   siatki krawędzi pokryw: rozmiar bloku `s` w pikselach i przesunięcie siatki. Wartości `s` nie
   zakładamy (zależy od `K`, `r`, poziomu — WG-PROJ).
3. **Identyfikacja kafli** — każdą komórkę siatki przeskalować do 64×64 i porównać (np. NCC po
   normalizacji jasności, bo oświetlenie pokryw nie jest znane) z katalogiem; wynik: macierz
   (kafel, obrót, odbicie) albo „nieznany” przy małym marginesie.
4. **Wyszukanie w mapie** — przesuwać okno po `wil.gmp` i liczyć zgodne komórki. Wynik ważny
   tylko, gdy najlepsze położenie jest jedyne (np. ≥ 90 % zgodnych komórek przy ≥ 6×6 znanych i
   drugie położenie co najmniej o 20 punktów procentowych gorsze). W przeciwnym razie powiększyć
   okno; gdy nadal niejednoznacznie — klatka `WORLD_OCCLUDED`.
5. **Perspektywa** — dopasowanie zaczyna się od komórek najbliższych środka kadru i jednego
   poziomu pokryw; komórki innych poziomów służą jako sprawdzenie modelu `p − c = K·x/(r − ℓ)`
   (różne `s` na różnych poziomach, rozbieżność ku krawędziom kadru). Z kilku poziomów wyznacza
   się `c`, `K`, `r` (WG-PROJ, sekcja 6.2 pkt 1–2).
6. **Wynik** — współrzędne mapy (x, y z częścią ułamkową) punktu pod punktem głównym `c` na
   wybranym poziomie, `s` dla każdego poziomu, `c` w pikselach, osobno położenie pieszego na
   ekranie. To są jedyne liczby, które wolno zapisać w repo (bez obrazów).
7. **Kontrola krzyżowa** — nasz render tego miejsca lokalnie, poza repo:
   `gta2 --level data/wil.gmp --camera X,Y --zoom s --offscreen 640x480 --screenshot <katalog poza repo>`.
   Nasz renderer jest dziś ortograficzny i rysuje pokrywy bez wysokości (spec sekcja 6.1, [I]), więc
   zgodność jest oczekiwana tylko dla pokryw jednego poziomu w pobliżu `c`.

Dodatkowa, słabsza wskazówka [H]: `wil.gmp` ma chunk `ZONE` z nazwanymi strefami (175 stref, 12
różnych bajtów typu, [D]). Jeśli oryginał wyświetla nazwę obszaru po wejściu do świata, ogranicza ona położenie
do prostokąta strefy — tylko jako kontrola, nie jako dowód położenia.

## 7. Co po przebiegu

* Do repo trafiają: etykiety klatek (sekcja 5), czasy przejść menu → świat, wyniki lokalizacji
  (sekcja 6, liczby) i ewentualna wersja v2 scenariusza. Bez zrzutów, bez danych gry.
* Jeśli świat nie pojawi się przed 64 s: poprawić `inputs` na podstawie zrzutów `MENU` (nowy
  commit tego samego `id`) i dopiero wtedy prosić o kolejny przebieg. **Po przebiegu z sekcji 8:**
  zrzuty `MENU` nie pokazały żadnej reakcji na ENTER, więc nie dają podstawy do innej sekwencji —
  najpierw potrzebna jest diagnostyka wejścia po stronie właściciela (sekcja 8).
* Dopiero po potwierdzeniu stabilnej kamery w spoczynku można planować scenariusze w zadanych
  miejscach mapy.

## 8. Przebieg odkrywczy `local-20261013-084136` (2026-10-13)

Pełna analiza: [m1-downtown-discovery.md](../../../docs/reports/m1-downtown-discovery.md).
Zrzuty zostają lokalnie; tutaj tylko wnioski.

* **Klatki:** wszystkie 20 zaplanowanych istnieje i jest czytelnych; **wszystkie 20 = `MENU`**
  (menu główne, podświetlone „PLAY”). Żadna klatka `WORLD` — **brak użytecznej referencji `WORLD`**;
  sekcja 6 (lokalizacja) i WG-PROJ nie mają materiału. [O]
* **Wejścia:** powierzchnia gry jest identyczna co do piksela od 6 s do 118 s, także po ENTER w 15,
  25 i 65 s; zmienia się tylko zegar pulpitu. Runner zgłasza po 2 zdarzenia `SendInput` na ENTER,
  ale czy gra je odebrała — nieustalone. `foreground: true` runner mierzy tylko w chwilach zrzutów,
  więc fokus w chwili wstrzyknięcia też jest nieustalony. [O] / [R]
* **Założenia z sekcji 3:** wersja gry na hoście to **9.6** (napis w menu) [O]; menu główne jest
  widoczne przed 6 s [O] (czy frontend był już gotowy przyjmować klawisze — nieustalone); domyślnie podświetlone „PLAY” [O]. Pełny ekran działa (DDrawCompat), ale
  zrzut ma 2304×800 z dwiema powierzchniami: gra zajmuje x 0–1279 (1280×800, obraz 4:3 ze
  skalowaniem ≈ 1,667), pulpit x 1280–2303 [O]. Przebieg ekranu 2 frontendu i dalsze punkty sekcji
  3 pozostają [H].
* **Werdykt hosta** `FAIL_GAME_DIALOG` nie jest dowodem okna dialogowego (`dialogs=null`, brak okna na
  zrzutach, okno gry na pierwszym planie w chwilach zrzutów [R]); prawdopodobny defekt werdyktu w
  runnerze — raport, sekcja 5, D1.
* **Następny warunek wstępny (po stronie właściciela):** przegląd podejrzewanego defektu werdyktu D1
  i przygotowanie ograniczonej diagnostyki wejścia w runnerze, identyfikowanej rewizją (SHA-256
  wdrożonych skryptów): wynik `SetForegroundWindow` i stan pierwszego planu tuż przed i po każdym
  wstrzyknięciu, błędy `SendInput`, pomocniczo stan procesu gry. To dane diagnostyczne, **nie dowód**
  dostarczenia ani obsłużenia klawisza — konsumpcja wejścia może pozostać nierozstrzygnięta. Zmiana
  dotyczy chronionej ścieżki `tests/gfx/windows/` — **do przekazania właścicielowi przez Astrę**
  (raport, sekcja 7). O kolejnym przebiegu decyduje się dopiero po przejrzeniu tej zmiany i
  zebranych danych. Harmonogram `inputs` tego scenariusza pozostaje bez zmian.
