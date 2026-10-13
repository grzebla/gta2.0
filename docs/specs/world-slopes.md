# Skosy świata: kody 0–63, rodziny i nierozstrzygnięta semantyka ścian

Dokument badawczy T0019 (2026-10-13, baza `0348c5d`). Opisuje, co wiadomo o każdym z 64 kodów
`BlockInfo::slope()` (bity 2–7 bajtu `slope_type`), skąd to wiadomo i czego nie wiadomo, oraz
proponuje kolejność zadań implementacyjnych. **Nie** zawiera implementacji. Nic tutaj nie dowodzi
wierności wizualnej wobec oryginału: brak jakiegokolwiek zrzutu oryginału ze skosem ([O]).

Uzupełnia [world-geometry.md](world-geometry.md) (bloki bez skosów: osie, pięć ścian, obrót,
projekcja). Konwencje osi, ścian i etykiet z tamtego dokumentu obowiązują tutaj bez zmian.

## 0. Statusy

| Znacznik | Kategoria | Znaczenie w tym dokumencie |
|---|---|---|
| **[F]** | dokumentacja formatu | Dokument DMA Design „GTA2 map format” v8.2 w transkrypcji z [../formats/gmp.md](../formats/gmp.md). Dla skosów przepisany jest **tylko** układ bitów (`gmp.md:14`); tabeli znaczeń kodów w repo nie ma. |
| **[M]** | agregat z oryginalnych map | Liczby z lokalnych map 9.6 (`GTA20_GAME_DIR`), powtarzalne poleceniem z sekcji 1.1. Mówią o **zawartości plików** (które słowa są ustawione, kto z kim sąsiaduje), nie o tym, jak oryginał rysuje. Nie są asercjami CTest. |
| **[B]** | analiza zachowania (GTA2_RE) | Opis działania oryginału własnymi słowami na podstawie konkretnej rewizji GTA2_RE. **W tej sesji: brak** (sekcja 1). |
| **[O]** | pomiar na oryginalnej grze | Zrzut lub ślad `originalGta2`. **Brak** dla skosów. |
| **[T]** | strona trzecia | Archiwalny OpenGTA2 (licencja MIT, gałąź `integrate/opengta2-upstream`, porzucony 2026-10-12): nazwy wyliczenia `SlopeType` i jego własna interpretacja geometrii. To nie jest dokument DMA ani zachowanie oryginału — tylko nazwy do potwierdzenia i lista konkurencyjnych hipotez. Kodu nie przenosimy. |
| **[H]** | hipoteza | Wszystko inne, w tym wnioski z [M] i propozycje konwencji. |
| **[I]** / **[A]** | nasz silnik / test CTest | Jak w `world-geometry.md` sekcja 0. |

Hipotezy nazwane (używane w tabelach):

* **H-CONT** (ciągłość): koniec skosu, którego sąsiad na tym samym poziomie z jest pełnym blokiem z
  pokrywą (kod 0, pokrywa ≠ 0, czyli wierzch na wysokości z + 1), leży na wysokości 1 (w jednostkach
  poziomu ponad z); koniec, którego sąsiad na tym samym z jest pusty, leży na wysokości 0 (grunt to
  pokrywa bloku z − 1). Sąsiednie skosy mają na wspólnej krawędzi te same wysokości.
* **H-STEP** (równe stopnie): rodzina o n kawałkach na poziom (n = 2 dla „26°”, 8 dla „7°”, 1 dla
  „45°”) dzieli wzniesienie równo: kawałek k (1 … n) ma krawędź niską na (k − 1)/n i wysoką na k/n.
* **H-FOOT** (podstawa diagonalna): bryła kodów 45–52 zajmuje w rzucie z góry tylko jeden trójkąt
  komórki wyznaczony przekątną (sekcja 4.4).

## 1. Źródła

| Źródło | Lokalizacja / rewizja | Kat. | Co z niego wzięto |
|---|---|---|---|
| Dokument DMA „GTA2 map format” v8.2 | transkrypcja `docs/formats/gmp.md:11-14` | [F] | tylko: `slope_type` = bity 0–1 podłoże, 2–7 skos 0–63. **Oryginalna strona (gtamp.com) nie była dostępna w tej sesji** — próba pobrania została odrzucona przez piaskownicę (brak dostępu sieciowego). Znaczenia kodów, opis 3-/4-bocznych skosów, rozmiary bloków częściowych i znaczenie `flat` pozostają nieprzepisane. |
| Dokument DMA „GBH Style Format” v4.6 | `docs/formats/sty.md:20` | [F] | kafle 992–1023 zarezerwowane. |
| Twierdzenie „1023 = znacznik skosu trójbocznego” | `docs/formats/gmp.md:28-30`, komentarz `engine/scene/src/draw_list.cpp:99-100` | [F] niepotwierdzone | repo przypisuje je dokumentom formatu, ale nie przytacza ich tekstu. Dane [M] (sekcja 5) są z nim zgodne. |
| Inwentaryzacja T0010 | `gta20-headless inventory` (`engine/assets/src/map_inventory.cpp`), uruchomiona 2026-10-13 na `~/Games/GTA2` | [M] | liczba komórek każdego kodu na każdej z 17 map, kafle 992–1022 i 1023 na ścianach i pokrywach. Wynik `PASS`, 17 map, raport 8102 B. |
| Agregaty sąsiedztwa i słów ścian | `tools/map-analysis/slope_aggregates.py` (ta gałąź) | [M] | dla każdego kodu: udział niezerowych słów ścian, bitów `flat`, pokrywy 1023, klasy sąsiadów w ±x, ±y, z − 1, z + 1; dla 49–52 osobno z pokrywą 1023 i bez. Tylko liczby i procenty. |
| Testy `[gamedata]` | `tests/unit/gamedata_tests.cpp:91-111` | [A] | w `wil`/`ste`/`bil` ≥ 95 % **unikalnych rekordów** z pokrywą 1023 ma skos 49–52; żadna ściana boczna nie ma kafla ≥ 992. |
| OpenGTA2 `SlopeType` | `integrate/opengta2-upstream:src/OpenGta2.GameData/Map/Models/SlopeType.cs` (ostatnia zmiana `ccca39c`, 2023-06-22) | [T] | 64 nazwy kodów (kolumna „Nazwa” w sekcji 3). |
| OpenGTA2 `SlopeGenerator` | `integrate/opengta2-upstream:src/OpenGta2.Client/Levels/SlopeGenerator.cs` (`ccca39c`) | [T] | tylko jako lista konkurencyjnych interpretacji (sekcje 4.5–4.6): 3-boczny = trójkątna podstawa z wierzchołkiem w górnym narożniku, 4-boczny = trójkątna pokrywa ze ścianą ukośną w dół; bloki częściowe 24/64, środkowy 16/64 szerokości. Żadna z tych liczb nie jest tu przyjęta. |
| GTA2_RE | — | [B] | **Nie przeglądano.** Brak lokalnej kopii i brak dostępu sieciowego w tej sesji; **sprawdzona rewizja: żadna**. Jedyne wskazówki to nazwy funkcji z `docs/mechanics-matrix.md:17` (`DrawGradientSlope_4F6630`, `DrawDiagonalWall_4F02D0`, `FindGroundZForCoord_4E5B60`). Nic w tym dokumencie nie pochodzi z kodu GTA2_RE. |
| Zrzuty oryginału | `docs/reports/`, `tests/gfx/scenarios/` | [O] | brak. Jedyny wykonany przebieg (T0012) nie dotarł do świata. |
| Nasz silnik | `engine/scene/src/draw_list.cpp:94-115` | [I] | każda pokrywa jest dziś rysowana jako płaski kwadrat komórki **niezależnie od kodu skosu**; pokrywy z kaflem 0 lub ≥ `tile_count` (w tym 1023) są pomijane; ścian bocznych nie ma. |

### 1.1 Powtórzenie agregatów [M]

```sh
GTA20_GAME_DIR=~/Games/GTA2 ./build/debug/bin/gta20-headless inventory > "$TMPDIR/inventory.txt"
python3 -I tools/map-analysis/slope_aggregates.py "$GTA20_GAME_DIR" --inventory "$TMPDIR/inventory.txt"
```

Skrypt sam czyta DMAP (układ z `gmp.md:15-16`) i porównuje swoje liczby komórek z raportem T0010
**dla każdej mapy i każdego kodu** oraz liczbę pokryw 1023; przy niezgodności kończy się kodem 1.
Wynik 2026-10-13: `inventory cross-check: 17 maps, 1385106 cells, 63 slope codes present, 6415
lid-1023 cells: PASS` (≈ 6 s). Wydruk to wyłącznie liczby i procenty (bez rekordów, współrzędnych i
pikseli); nie jest commitowany.

Zastrzeżenia do [M]:

* „Komórka” = niepusty rekord w siatce 256 × 256 × 8 (ta sama definicja co w T0010). Sumy po 17
  mapach **nie są niezależnymi próbami**: `wil-multi`/`bil-multi` to prawie kopie `wil`/`bil`
  (różnice < 0,3 % komórek), a poziomy bonusowe powtarzają fragmenty miast.
* Klasy sąsiadów: `empty` (pusty rekord), `0+lid` (kod 0 z kaflem pokrywy ≠ 0), `0-nolid` (kod 0 bez
  pokrywy), liczba = kod skosu sąsiada. Kierunki: `left`/`right`/`top`/`bottom` = −x/+x/−y/+y
  (`world-geometry.md` sekcja 2, [A] tylko statystycznie na `wil.gmp`).
* Procenty dotyczą komórek danego kodu; przy sąsiadach podawane są tylko trzy najczęstsze klasy.

## 2. Konwencje

* Komórka (x, y, z) zajmuje [x, x+1] × [y, y+1] × [z, z+1] (kontrakt T0001, [I]). Narożniki w rzucie
  z góry: **NW** = (x, y), **NE** = (x+1, y), **SE** = (x+1, y+1), **SW** = (x, y+1); −y to góra
  ekranu (dla naszego silnika [I], dla oryginału [H]).
* Wysokość h ∈ [0, 1] mierzona w jednostkach **poziomu** ponad z. Proporcja wysokość poziomu /
  szerokość bloku jest nierozstrzygnięta (`world-geometry.md` sekcja 6.2), więc kąty i normalne
  podane niżej zakładają h/w = 1 i są tylko orientacyjne; wysokości narożników od tego nie zależą.
* „Wznosi się ku −y” = wysokość rośnie przy malejącym y.

## 3. Tabela kodów 0–63

Kolumny: **Nazwa** — identyfikator z [T] (do potwierdzenia [F]); **Komórki** — suma po 17 mapach z
T0010 [M] (w nawiasie liczba map, w których kod występuje); **wil/ste/bil** — komórki w trzech
miastach [M]; **Status**:

* **IMPL** — zaimplementowane jako blok jednostkowy [I];
* **GOTOWE-H** — geometria wyznaczona przez [M] przy H-CONT i H-STEP; można implementować z
  syntetycznymi oczekiwaniami (sekcja 6), zgodność z oryginałem nieudowodniona;
* **CZĘŚĆ** — część geometrii GOTOWE-H, reszta zablokowana;
* **BLOK** — zablokowane: dane dopuszczają więcej niż jedną geometrię albo brakuje wymiaru;
* **BRAK** — kod nie występuje w żadnej mapie;
* **ZNACZNIK** — kod opisuje relację z innym blokiem, nie własny kształt.

| Kod | Nazwa [T] | Rodzina | Komórki (map) | wil/ste/bil | Status | Nierozstrzygnięte |
|---|---|---|---|---|---|---|
| 0 | None | brak skosu | 1 273 055 (17) | 95 206/110 477/101 281 | IMPL | wszystko z `world-geometry.md` sekcja 4; pokrywa 1023 w 3 komórkach |
| 1 | Up26_1 | 26°, ku −y, k = 1 z 2 | 2 068 (16) | 399/265/379 | GOTOWE-H | UV ścian bocznych, orientacja tekstury pokrywy, `flat` na pokrywie (23,6 %) |
| 2 | Up26_2 | 26°, ku −y, k = 2 z 2 | 2 068 (16) | 391/305/356 | GOTOWE-H | jak 1 |
| 3 | Down26_1 | 26°, ku +y, k = 1 z 2 | 2 032 (17) | 383/176/375 | GOTOWE-H | jak 1 |
| 4 | Down26_2 | 26°, ku +y, k = 2 z 2 | 2 038 (17) | 385/216/353 | GOTOWE-H | jak 1 |
| 5 | Left26_1 | 26°, ku −x, k = 1 z 2 | 1 871 (16) | 321/309/321 | GOTOWE-H | jak 1 |
| 6 | Left26_2 | 26°, ku −x, k = 2 z 2 | 2 050 (16) | 341/408/316 | GOTOWE-H | jak 1 |
| 7 | Right26_1 | 26°, ku +x, k = 1 z 2 | 1 854 (16) | 283/330/326 | GOTOWE-H | jak 1 |
| 8 | Right26_2 | 26°, ku +x, k = 2 z 2 | 2 009 (16) | 314/430/311 | GOTOWE-H | jak 1 |
| 9 | Up7_1 | 7°, ku −y, k = 1 z 8 | 456 (7) | 85/36/80 | GOTOWE-H | jak 1 |
| 10 | Up7_2 | 7°, ku −y, k = 2 z 8 | 437 (7) | 81/36/73 | GOTOWE-H | jak 1 |
| 11 | Up7_3 | 7°, ku −y, k = 3 z 8 | 445 (7) | 85/36/73 | GOTOWE-H | jak 1 |
| 12 | Up7_4 | 7°, ku −y, k = 4 z 8 | 393 (7) | 64/36/72 | GOTOWE-H | jak 1 |
| 13 | Up7_5 | 7°, ku −y, k = 5 z 8 | 393 (7) | 67/36/69 | GOTOWE-H | jak 1 |
| 14 | Up7_6 | 7°, ku −y, k = 6 z 8 | 377 (6) | 59/36/68 | GOTOWE-H | jak 1 |
| 15 | Up7_7 | 7°, ku −y, k = 7 z 8 | 339 (6) | 47/36/61 | GOTOWE-H | jak 1 |
| 16 | Up7_8 | 7°, ku −y, k = 8 z 8 | 318 (6) | 47/36/60 | GOTOWE-H | jak 1 |
| 17 | Down7_1 | 7°, ku +y, k = 1 z 8 | 338 (9) | 94/21/55 | GOTOWE-H | jak 1 |
| 18 | Down7_2 | 7°, ku +y, k = 2 z 8 | 320 (9) | 92/21/48 | GOTOWE-H | jak 1 |
| 19 | Down7_3 | 7°, ku +y, k = 3 z 8 | 320 (9) | 92/21/48 | GOTOWE-H | jak 1 |
| 20 | Down7_4 | 7°, ku +y, k = 4 z 8 | 301 (9) | 81/21/49 | GOTOWE-H | jak 1 |
| 21 | Down7_5 | 7°, ku +y, k = 5 z 8 | 267 (9) | 67/21/46 | GOTOWE-H | jak 1 |
| 22 | Down7_6 | 7°, ku +y, k = 6 z 8 | 236 (8) | 55/21/43 | GOTOWE-H | jak 1 |
| 23 | Down7_7 | 7°, ku +y, k = 7 z 8 | 206 (8) | 46/21/37 | GOTOWE-H | jak 1 |
| 24 | Down7_8 | 7°, ku +y, k = 8 z 8 | 205 (8) | 46/21/37 | GOTOWE-H | jak 1 |
| 25 | Left7_1 | 7°, ku −x, k = 1 z 8 | 395 (9) | 94/39/76 | GOTOWE-H | jak 1 |
| 26 | Left7_2 | 7°, ku −x, k = 2 z 8 | 379 (9) | 93/39/69 | GOTOWE-H | jak 1 |
| 27 | Left7_3 | 7°, ku −x, k = 3 z 8 | 305 (9) | 88/39/38 | GOTOWE-H | jak 1 |
| 28 | Left7_4 | 7°, ku −x, k = 4 z 8 | 299 (9) | 84/39/38 | GOTOWE-H | jak 1 |
| 29 | Left7_5 | 7°, ku −x, k = 5 z 8 | 240 (9) | 62/39/33 | GOTOWE-H | jak 1 |
| 30 | Left7_6 | 7°, ku −x, k = 6 z 8 | 214 (7) | 53/39/32 | GOTOWE-H | jak 1 |
| 31 | Left7_7 | 7°, ku −x, k = 7 z 8 | 194 (7) | 49/39/26 | GOTOWE-H | jak 1 |
| 32 | Left7_8 | 7°, ku −x, k = 8 z 8 | 197 (7) | 51/38/26 | GOTOWE-H | jak 1 |
| 33 | Right7_1 | 7°, ku +x, k = 1 z 8 | 408 (8) | 89/43/87 | GOTOWE-H | jak 1 |
| 34 | Right7_2 | 7°, ku +x, k = 2 z 8 | 390 (8) | 87/43/80 | GOTOWE-H | jak 1 |
| 35 | Right7_3 | 7°, ku +x, k = 3 z 8 | 326 (7) | 90/43/49 | GOTOWE-H | jak 1 |
| 36 | Right7_4 | 7°, ku +x, k = 4 z 8 | 306 (7) | 82/43/46 | GOTOWE-H | jak 1 |
| 37 | Right7_5 | 7°, ku +x, k = 5 z 8 | 237 (6) | 51/43/45 | GOTOWE-H | jak 1 |
| 38 | Right7_6 | 7°, ku +x, k = 6 z 8 | 227 (5) | 48/43/44 | GOTOWE-H | jak 1 |
| 39 | Right7_7 | 7°, ku +x, k = 7 z 8 | 213 (5) | 45/43/40 | GOTOWE-H | jak 1 |
| 40 | Right7_8 | 7°, ku +x, k = 8 z 8 | 207 (5) | 44/43/38 | GOTOWE-H | jak 1 |
| 41 | Up45 | 45°, ku −y, k = 1 z 1 | 4 941 (15) | 821/509/1 173 | GOTOWE-H | jak 1; `flat` na pokrywie 33,1 % |
| 42 | Down45 | 45°, ku +y | 4 881 (15) | 798/541/1 151 | GOTOWE-H | jak 41 (`flat` 36,0 %) |
| 43 | Left45 | 45°, ku −x | 4 270 (15) | 523/389/1 307 | GOTOWE-H | jak 41 (`flat` 46,8 %) |
| 44 | Right45 | 45°, ku +x | 3 809 (15) | 461/510/1 070 | GOTOWE-H | jak 41 (`flat` 40,0 %) |
| 45 | DiagonalFacingUpLeft | ściana diagonalna, otwarta ku NW | 3 000 (13) | 377/391/764 | CZĘŚĆ | kształt i UV pokrywy (H-FOOT), UV ściany ukośnej, pokrywa 1023 w 4 komórkach |
| 46 | DiagonalFacingUpRight | ściana diagonalna, otwarta ku NE | 2 940 (13) | 382/364/721 | CZĘŚĆ | jak 45 (pokrywa 1023: 4) |
| 47 | DiagonalFacingDownLeft | ściana diagonalna, otwarta ku SW | 2 944 (13) | 409/502/653 | CZĘŚĆ | jak 45 (pokrywa 1023: 2) |
| 48 | DiagonalFacingDownRight | ściana diagonalna, otwarta ku SE | 2 748 (13) | 411/421/591 | CZĘŚĆ | jak 45 (pokrywa 1023: 0) |
| 49 | DiagonalSlopeFacingUpLeft | skos diagonalny 3-/4-boczny, ku NW | 1 800 (13) | 110/433/428 | BLOK | H-TET vs H-HIP (pokrywa 1023, 88,2 %), H-COMP vs H-VALLEY (pozostałe); UV; `flat` na ścianie ukośnej |
| 50 | DiagonalSlopeFacingUpRight | skos diagonalny, ku NE | 1 749 (12) | 117/400/412 | BLOK | jak 49 (1023: 88,5 %) |
| 51 | DiagonalSlopeFacingDownLeft | skos diagonalny, ku SW | 1 942 (13) | 128/500/447 | BLOK | jak 49 (1023: 84,9 %) |
| 52 | DiagonalSlopeFacingDownRight | skos diagonalny, ku SE | 1 914 (13) | 119/420/482 | BLOK | jak 49 (1023: 84,5 %) |
| 53 | PartialBlockLeft | blok częściowy przy krawędzi −x | 986 (11) | 123/114/209 | BLOK | grubość pasa; częsty `flat` (tu 62–77 % słów bocznych); widoczność stron |
| 54 | PartialBlockRight | blok częściowy przy krawędzi +x | 1 141 (8) | 196/80/213 | BLOK | jak 53 |
| 55 | PartialBlockTop | blok częściowy przy krawędzi −y | 1 296 (9) | 179/117/289 | BLOK | jak 53 |
| 56 | PartialBlockBottom | blok częściowy przy krawędzi +y | 1 029 (9) | 101/138/232 | BLOK | jak 53 |
| 57 | PartialBlockTopLeftCorner | słupek w narożniku NW | 299 (9) | 15/81/60 | BLOK | rozmiar kwadratu; jak 53 |
| 58 | PartialBlockTopRightCorner | słupek w narożniku NE | 295 (9) | 10/78/69 | BLOK | jak 57 |
| 59 | PartialBlockBottomRightCorner | słupek w narożniku SE | 232 (8) | 16/60/47 | BLOK | jak 57 |
| 60 | PartialBlockBottomLeftCorner | słupek w narożniku SW | 270 (9) | 15/69/55 | BLOK | jak 57 |
| 61 | PartialBlockCentre | słupek na środku komórki | 801 (12) | 76/3/295 | BLOK | rozmiar; jak 53 |
| 62 | Reserved | zarezerwowany | 0 (0) | 0/0/0 | BRAK | znaczenie tylko z [T]; brak zadania |
| 63 | SlopeAbove | znacznik „skos w bloku powyżej” | 42 886 (17) | 7 555/5 241/8 912 | ZNACZNIK | czy ma własny kształt (sekcja 4.8); wpływ na kolizje poza zakresem |

Kontrola pokrycia: tabela ma dokładnie 64 wiersze, kody 0–63, każdy raz; liczby komórek i map są
sprawdzane względem raportu T0010 (sekcja 8).

## 4. Rodziny

### 4.1 Skosy gradientowe 1–44: wysokości narożników

**[M] — łańcuchy.** Kawałki tej samej rodziny leżą obok siebie w jednej osi, w kolejności numerów:

| Rodzina | Kody | Następny kawałek w kierunku | Udział następnika (kolejne pary) | Koniec „niski”: sąsiad pusty | Koniec „wysoki”: sąsiad `0+lid` |
|---|---|---|---|---|---|
| 26° Up | 1→2 | −y | 95,6 % | +y od 1: 89,1 % | −y od 2: 41,2 % (dalej: kod 4 31,3 %, 63 12,9 %) |
| 26° Down | 3→4 | +y | 95,3 % | −y od 3: 91,0 % | +y od 4: 38,7 % (kod 2 31,8 %) |
| 26° Left | 5→6 | −x | 98,5 % | +x od 5: 85,2 % | −x od 6: 39,0 % (kod 8 32,5 %) |
| 26° Right | 7→8 | +x | 96,1 % | −x od 7: 87,6 % | +x od 8: 43,8 % (kod 6 33,2 %) |
| 7° Up | 9→…→16 | −y | 87,9–100 % | +y od 9: 94,7 % | −y od 16: 74,8 % |
| 7° Down | 17→…→24 | +y | 85,8–99,4 % | −y od 17: 92,0 % | +y od 24: 91,2 % |
| 7° Left | 25→…→32 | −x | 75,6–99,5 % | +x od 25: 78,5 % | −x od 32: 90,9 % |
| 7° Right | 33→…→40 | +x | 77,5–97,2 % | −x od 33: 89,7 % | +x od 40: 95,2 % |
| 45° | 41 / 42 / 43 / 44 | — | — | +y / −y / +x / −x: 86,7 / 85,4 / 80,1 / 84,7 % | −y / +y / −x / +x: 43,6 / 44,5 / 46,2 / 51,0 % |

Gdy następnik nie jest kolejnym kawałkiem, najczęściej jest nim kawałek **o tym samym numerze k z
rodziny przeciwnej** (grzbiet): 2↔4 i 6↔8 (~31–33 %), 11→19 (9,0 %), 19→11 (12,5 %), 12→20, 13→21,
14→22 (4,6–5,3 %), 28→36 (20,4 %), 36→28 (19,9 %), 27→35, 29→37, 35→27, 37→29 (~5 %), 41→42
(19,9 %), 44→43 (13,2 %); rzadziej kawałek innej rodziny o tej samej wysokości krawędzi (32→44
6,1 %, 40→6 1,9 %). Przy H-STEP każda z tych par styka się krawędziami o tej samej wysokości; wśród
trzech najczęstszych klas sąsiada w osi wzniesienia żadnego kodu 1–44 nie ma pary o różnych
wysokościach krawędzi. Niezgodne sąsiedztwa istnieją w mniejszości (po niskiej stronie: kod 2 przy
kodzie 1 — 2,1 %, kod 6 przy kodzie 5 — 2,6 %, `0+lid` przy kodach 7 i 17 — 2,7 % i 1,8 %), a po
wysokiej stronie bywa pusto (16: 23,0 %, 41–44: 20,7–30,4 %) albo kod 63 (2, 4, 6, 8: 12,5–15,3 %;
możliwe [H]: rampa ciągnie się poziom wyżej). H-CONT opisuje więc typowy układ, nie każdą komórkę.

**Wniosek [H] (H-CONT + H-STEP, zgodny z [M]):** nazwa rodziny = kierunek **wznoszenia się**
(„Up” = ku −y, czyli ku górze ekranu); łańcuch n kawałków wznosi się o jeden poziom (od pustej
komórki z gruntem na wysokości z do pełnego bloku z wierzchem na z + 1). Nazwy „26°”, „7°” i „45°”
zgadzają się z atan(1/2) = 26,6°, atan(1/8) = 7,1° i atan(1) = 45° tylko przy równych stopniach i
h/w = 1 — to poszlaka dla H-STEP, nie pomiar.

**Wysokości narożników (h ponad z), kawałek k z n, L = (k − 1)/n, U = k/n:**

| Kierunek | Kody (k) | NW | NE | SE | SW | Normalna (h/w = 1, nienormowana) |
|---|---|---|---|---|---|---|
| Up (ku −y) | 1–2 (k = 1–2, n = 2); 9–16 (k = 1–8, n = 8); 41 (n = 1) | U | U | L | L | (0, +1/n, 1) |
| Down (ku +y) | 3–4; 17–24; 42 | L | L | U | U | (0, −1/n, 1) |
| Left (ku −x) | 5–6; 25–32; 43 | U | L | L | U | (+1/n, 0, 1) |
| Right (ku +x) | 7–8; 33–40; 44 | L | U | U | L | (−1/n, 0, 1) |

Przykład: kod 12 (Up7_4) ma NW = NE = 4/8, SE = SW = 3/8; kod 6 (Left26_2) ma NW = SW = 1,
NE = SE = 1/2.

Rzut z góry (Up; pozostałe kierunki to obrót tego rysunku):

```
            −y (góra ekranu)
     NW  U ────────── U  NE
         │  ↑   ↑   ↑ │      strzałki: kierunek wznoszenia
         │  ↑   ↑   ↑ │
     SW  L ────────── L  SE
            +y
```

**Triangulacja.** Pokrywa kawałka gradientowego jest płaska (dwie przeciwległe krawędzie poziome),
więc obie przekątne dają tę samą powierzchnię; przy mapowaniu tekstury liniowym w płaszczyźnie
wybór przekątnej też nie zmienia obrazu. Proponowany wybór dla deterministycznych testów [H]:
narożniki p0..p3 = NW, NE, SE, SW i trójkąty {0,1,2}, {0,2,3}, jak dla pokrywy T0001.

### 4.2 Skosy gradientowe: ściany boczne

**[M] — które słowa są ustawione** (udział komórek z kaflem ≠ 0):

| Kody | Słowa wzdłuż osi wzniesienia (boki) | Słowo po stronie niskiej | Słowo po stronie wysokiej |
|---|---|---|---|
| 1–8 (26°) | 25,4–30,3 % | 0,0 % we wszystkich ośmiu | k = 1: 0,0 %; k = 2: 6,8–10,8 % |
| 9–40 (7°) | 6,2–19,1 % | 0,0 % we wszystkich | k < 8: 0,0 %; k = 8: 2,5–27,7 % |
| 41–44 (45°) | 12,0–14,4 % | 0,0 % | 2,5–8,7 % |

Przykład: kod 2 (Up26_2) — `left` 27,3 %, `right` 26,9 %, `top` (−y, strona wysoka) 10,1 %,
`bottom` 0,0 %; kod 1 — `top` i `bottom` 0,0 %.

**Wniosek [H]:** każda ściana boczna kawałka leży w płaszczyźnie swojej strony komórki (jak w
T0001) i bierze kafel ze słowa tej strony:

* dwie ściany wzdłuż osi wzniesienia (dla Up/Down: `left` i `right`; dla Left/Right: `top` i
  `bottom`) to wielokąty od h = 0 do krawędzi pokrywy: trapez o wysokościach L i U, a dla k = 1
  trójkąt (L = 0);
* ściana po stronie wysokiej to prostokąt o wysokości U; po stronie niskiej prostokąt o wysokości L,
  zdegenerowany dla k = 1. Dane pokazują kafel po stronie wysokiej tylko dla ostatniego kawałka
  (k = n), a po stronie niskiej nigdy — zgodnie z tym, że pozostałe stykają się z sąsiednim
  kawałkiem łańcucha.

Nierozstrzygnięte: jaka część kafla 64 × 64 trafia na trapez/trójkąt (przycięcie do kształtu czy
ściśnięcie całego kafla), orientacja i obrót tekstury na ścianach i na pochyłej pokrywie (czy
obowiązuje tabela `world-geometry.md` 5.1 bez względu na kierunek wznoszenia), widoczność ścian
(WG-WALL-SEL, WG-WALL-BITS), znaczenie `flat` na pokrywach skosów (20,0–32,0 % dla 1–8, 3,1–23,8 %
dla 9–40, 33,1–46,8 % dla 41–44), oświetlenie. Do implementacji te punkty muszą być jawnie
oznaczone jako [H] albo pozostać poza zakresem (sekcja 6).

### 4.3 Kod 63 i bloki pod skosami

**[M]:** bezpośrednio pod komórką skosu gradientowego leży kod 63 w 98,2–99,9 % (1–8),
96,6–100 % (9–40) i 94,4–97,0 % (41–44). Odwrotnie: nad 42 886 komórkami kodu 63 leży skos
gradientowy w 99,9 % (41–44: 40,0 %, 1–8: 37,0 %, 9–40: 22,9 %). Pod skosami 45–61 kodu 63 **nie**
ma wśród trzech najczęstszych klas. Kod 63: podłoże `air` 94,9 %, pokrywa ≠ 0 tylko 9,6 %, ściany
boczne 15,0–18,4 %; pod nim pusto (52,7 %) albo blok kodu 0.

**Wniosek [H]:** 63 oznacza „pod skosem gradientowym”, zgodnie z nazwą [T]. Pokrywa bloku 63 leży
pod powierzchnią skosu, więc zwykle nie ma kafla; ściany boczne to np. boki nasypu lub podpory
mostu. Czy oryginał traktuje 63 jako zwykły blok jednostkowy przy rysowaniu, jest nierozstrzygnięte
(brak [F]/[B]/[O]); do tego czasu 63 nie wymaga osobnej geometrii — obecna ścieżka bloku
jednostkowego obsługuje jego słowa jak dla kodu 0.

### 4.4 Ściany diagonalne 45–48

**[M]:**

| Kod | Puste sąsiedztwo (−x / +x / −y / +y) | Słowa ≠ 0: left / right / top / bottom | Pokrywa ≠ 0 | Pod spodem ten sam kod |
|---|---|---|---|---|
| 45 | **91,6** / — / **90,7** / — | **99,8** / 9,3 / 0,0 / 3,9 | 39,5 % | 51,6 % |
| 46 | — / **91,6** / **91,3** / — | 6,9 / **96,8** / 0,0 / 11,5 | 44,4 % | 51,0 % |
| 47 | **92,7** / — / — / **90,7** | **98,0** / 10,4 / 7,1 / 0,0 | 39,7 % | 52,9 % |
| 48 | — / **91,6** / — / **89,3** | 8,1 / **99,0** / 11,1 / 0,0 | 42,4 % | 52,0 % |

(„—”: pusty sąsiad nie jest najczęstszą klasą; w tych kierunkach dominują bloki kodu 0 z pokrywą
lub bez, ~29–38 % każda.)

**Wniosek [H] (zgodny z nazwą [T] „facing …”):** ściana diagonalna to pionowa płaszczyzna pełnej
wysokości poziomu wzdłuż przekątnej komórki; „facing” wskazuje stronę otwartą (normalna ściany
ukośnej). Bryła zajmuje trójkąt po przeciwnej stronie (H-FOOT). Kafel ściany ukośnej pochodzi ze
słowa `left` (45, 47) albo `right` (46, 48) — to jedyne słowa ustawione w ≥ 96,8 % komórek. Z
pozostałych słów ustawiane są tylko te dwie strony, które zostają z bryły trójkątnej; trzecia
„zbędna” strona ma 0,0 %.

| Kod | Otwarta strona | Przekątna | Bryła (trójkąt) | Słowo ściany ukośnej | Pozostałe ściany |
|---|---|---|---|---|---|
| 45 | NW (−x, −y) | NE–SW | SE: NE, SE, SW | `left` | `right` (x+1), `bottom` (y+1) |
| 46 | NE (+x, −y) | NW–SE | SW: NW, SE, SW | `right` | `left` (x), `bottom` (y+1) |
| 47 | SW (−x, +y) | NW–SE | NE: NW, NE, SE | `left` | `right` (x+1), `top` (y) |
| 48 | SE (+x, +y) | NE–SW | NW: NW, NE, SW | `right` | `left` (x), `top` (y) |

Rzut z góry (wiersz górny = −y, kolumna lewa = −x; `#` bryła, `·` otwarte):

```
   45            46            47            48
· · · · /     \ · · · ·     \ # # # #     # # # # /
· · · / #     # \ · · ·     · \ # # #     # # # / ·
· · / # #     # # \ · ·     · · \ # #     # # / · ·
· / # # #     # # # \ ·     · · · \ #     # / · · ·
/ # # # #     # # # # \     · · · · \     / · · · ·
```

Ściana ukośna to prostokąt (przekątna × wysokość poziomu), więc triangulacja nie wpływa na
geometrię. Nierozstrzygnięte: **pokrywa** (39,5–44,4 % komórek ma kafel; czy jest przycięta do
trójkąta bryły, H-FOOT, czy pokrywa całą komórkę), UV i orientacja tekstury na ścianie ukośnej
(długość przekątnej √2 bloku przy h/w = 1), pokrywy 1023 w 10 komórkach (45: 4, 46: 4, 47: 2).

### 4.5 Skosy diagonalne 49–52 z pokrywą 1023 („trójboczne”)

**[M] — interakcja z kaflem 1023:** 6 415 komórek w 17 mapach ma pokrywę 1023; 6 402 z nich
(99,80 %) to kody 49–52, pozostałe: kod 0 (3), 45 (4), 46 (4), 47 (2). Kafel 1023 nie występuje na
żadnej ścianie bocznej (T0010: `side 1023=0` na każdej mapie). Wśród 7 405 komórek 49–52 pokrywę 1023
ma 86,46 % (49: 88,2 %, 50: 88,5 %, 51: 84,9 %, 52: 84,5 %). Wcześniejsze „98,8 %” (`gmp.md:28`,
trzy miasta, mianownik nieznany) i „≥ 95 % unikalnych rekordów” ([A]) liczono inaczej; po
komórkach 17 map jest 99,80 %.

**[M] — komórki 49–52 z pokrywą 1023:**

| Kod | Puste sąsiedztwo | Sąsiad w głąb (najczęstszy) | Słowo ≠ 0 w ≥ 99,9 % | Pod spodem | Nad nim pusto |
|---|---|---|---|---|---|
| 49 | −x 84,2 %, −y 86,1 % | +x: 50 71,8 % / 42 19,7 %; +y: 51 72,8 % / 44 19,8 % | `left` (`top` 0,0 %) | `0+lid` 69,1 %, 45 29,0 % | 98,7 % |
| 50 | +x 84,6 %, −y 87,1 % | −x: 49 73,6 % / 42 19,6 %; +y: 52 74,7 % / 43 18,1 % | `right` (`top` 0,0 %) | `0+lid` 70,0 %, 46 28,0 % | 98,9 % |
| 51 | −x 83,9 %, +y 86,2 % | +x: 52 69,7 % / 41 20,6 %; −y: 49 70,1 % / 44 19,5 % | `left` (`bottom` 0,0 %) | `0+lid` 69,6 %, 47 27,3 % | 98,7 % |
| 52 | +x 85,7 %, +y 84,2 % | −x: 51 71,0 % / 41 20,5 %; −y: 50 71,4 % / 43 17,8 % | `right` (`bottom` 0,0 %) | `0+lid` 70,3 %, 48 26,9 % | 98,6 % |

Odczyt [H]: 49, 50, 51, 52 tworzą najczęściej kwadrat 2 × 2 (49 w NW, 50 w NE, 51 w SW, 52 w SE),
każdy otwarty na zewnątrz; albo narożnik pasa skosów 45° wokół dachu (np. 49 z 42 na wschodzie i 44
na południu). Słowo z kaflem ma ten sam układ co ściana diagonalna o tym samym kierunku (45 → 49,
46 → 50, 47 → 51, 48 → 52), a ok. 27–29 % komórek stoi na takiej właśnie ścianie diagonalnej.

Z H-CONT i sąsiadów 41–44 wynikają **wysokości narożników** (dla obu hipotez niżej takie same):
49: NW 0, NE 0, SE 1, SW 0; 50: SW = 1, reszta 0; 51: NE = 1, reszta 0; 52: NW = 1, reszta 0.
Sprawdzenie ciągłości (H-STEP, n = 1): wschodnia krawędź 49 (NE 0, SE 1) = zachodnia krawędź 42
(NW 0, SW 1); południowa krawędź 49 (SW 0, SE 1) = północna krawędź 44 (NW 0, NE 1).

Cztery narożniki o wysokościach 0, 0, 0, 1 nie leżą w jednej płaszczyźnie, więc potrzebna jest
decyzja o kształcie. Dwie hipotezy zgodne z krawędziami:

* **H-TET** (bryła na trójkącie podstawy): dla 49 tylko trójkąt SE (H-FOOT, ten sam co ściana 45);
  jedna pochyła ściana od przekątnej NE–SW (h = 0) do narożnika SE (h = 1), plus dwie pionowe
  trójkątne ściany po stronach `right` i `bottom`; w trójkącie NW nie ma nic (widać pokrywę bloku
  poniżej). Tak to interpretuje również [T].
* **H-HIP** (narożnik dachu czterospadowego): cała komórka, h = min(u, v) dla 49 (u, v ∈ [0, 1] od
  NW), dwie pochyłe ściany ze zgięciem na przekątnej NW–SE.

Co przemawia za H-TET (poszlaki [M], nie dowód): pokrywa jest atrapą (1023), a kafel ma tylko jedno
słowo — jedna teksturowana powierzchnia; ~28 % komórek stoi na ścianie diagonalnej, której bryła
zajmuje dokładnie trójkąt podstawy H-TET (przy H-HIP połowa komórki wisiałaby nad pustym miejscem);
pod pozostałymi jest pełny blok z pokrywą (69–70 %), który dostarcza widocznego „gruntu” w pustym
trójkącie. Nazwa „trójboczny” pasuje do trójkątnej powierzchni H-TET, ale to tylko nazwa (repo).
**Status: BLOK** — przyjęcie H-TET wymaga [F], [B] albo [O] (sekcja 7).

Triangulacja przy H-TET: jedna ściana trójkątna, bez wyboru. UV pochyłego trójkąta (który fragment
kafla, orientacja) — nierozstrzygnięte. `flat` na słowie ściany ukośnej: 57,4–60,9 % (znaczenie
nieznane).

### 4.6 Skosy diagonalne 49–52 bez pokrywy 1023 („czteroboczne”)

**[M]** (1 003 komórki: 49: 213, 50: 201, 51: 293, 52: 296):

| Kod | Sąsiedzi „na zewnątrz” | Sąsiedzi „w głąb” (`0+lid`) | Pokrywa ≠ 0 | Słowo ukośne ≠ 0 | Pod spodem pusto | Nad nim (ten sam kod / ściana diag.) |
|---|---|---|---|---|---|---|
| 49 | −x: 42 46,5 % / 49 46,0 %; −y: 49 46,0 % / 44 44,6 % | +x 64,8 %, +y 66,7 % | 83,1 % | `left` 99,1 % | 60,6 % | 22,1 % / 45: 20,2 % |
| 50 | +x: 42 47,3 % / 50 44,8 %; −y: 50 45,8 % / 43 43,3 % | −x 58,7 %, +y 63,7 % | 82,1 % | `right` 99,0 % | 63,2 % | 25,4 % / 46: 24,4 % |
| 51 | −x: 51 47,4 % / 41 45,7 %; +y: 51 53,2 % / 44 33,8 % | +x 54,6 %, −y 56,0 % | 80,2 % | `left` 99,3 % | 38,6 % | 27,6 % / 47: 27,6 % |
| 52 | +x: 52 47,0 % / 41 45,6 %; +y: 52 53,4 % / 43 33,1 % | −x 55,1 %, −y 59,1 % | 85,1 % | `right` 99,3 % | 40,9 % | 28,0 % / 48: 24,0 % |

Odczyt [H]: to **wklęsły** narożnik — pełne bloki leżą w głąb (dla 49 na wschodzie i południu),
a na zewnątrz ciągną się skosy 45° wznoszące się ku temu narożnikowi. Wysokości narożników z H-CONT:
49: NW 0, pozostałe 1; 50: NE 0; 51: SW 0; 52: SE 0. Ciągłość: zachodnia krawędź 49 (NW 0, SW 1)
= wschodnia krawędź 42 (NE 0, SE 1); północna krawędź 49 (NW 0, NE 1) = południowa krawędź 44
(SW 0, SE 1).

* **H-COMP** (kształt odwrotny do H-TET): płaska trójkątna pokrywa na h = 1 w trójkącie SE (dla 49) i
  pochyły trójkąt od narożnika NW (h = 0) do przekątnej NE–SW (h = 1). Zgięcie na przekątnej
  NE–SW (49, 52) albo NW–SE (50, 51). Tak interpretuje to również [T].
* **H-VALLEY**: h = max(u, v) dla 49, dwa pochyłe trójkąty ze zgięciem na przekątnej NW–SE.

Poszlaka [M] za H-COMP: ustawione są jednocześnie pokrywa (80–85 %) i jedno słowo ukośne
(99,0–99,3 %) — dwie różnie teksturowane powierzchnie, z których jedna jest pokrywą. Ściany
pionowe są w kontekście zasłonięte (sąsiedzi w głąb to pełne bloki, na zewnątrz skosy o tej samej
krawędzi), więc dane nie mówią, które słowo teksturuje które z nich. **Status: BLOK.**

### 4.7 Bloki częściowe 53–61

**[M] — strona otwarta** (udział pustego sąsiada):

| Kod | −x | +x | −y | +y | Wniosek [H]: bryła przy |
|---|---|---|---|---|---|
| 53 Left | 24,6 % | **78,3 %** | — | — | krawędzi −x (pas wzdłuż y) |
| 54 Right | **74,8 %** | 29,6 % | — | — | krawędzi +x |
| 55 Top | — | — | 33,4 % | **79,0 %** | krawędzi −y (pas wzdłuż x) |
| 56 Bottom | — | — | **75,7 %** | 23,5 % | krawędzi +y |
| 57 TopLeft | 55 53,2 % | **72,6 %** | 53 46,5 % | **76,6 %** | narożniku NW |
| 58 TopRight | **71,9 %** | 55 52,2 % | 54 44,4 % | **76,3 %** | narożniku NE |
| 59 BottomRight | **71,6 %** | 56 46,1 % | **74,6 %** | 54 59,1 % | narożniku SE |
| 60 BottomLeft | 56 46,7 % | **72,2 %** | **80,4 %** | 53 52,2 % | narożniku SW |
| 61 Centre | 85,4 % | 87,0 % | 79,7 % | 78,4 % | środku komórki |

(„—”: dominują sąsiedzi tego samego kodu, 58,4–65,6 %, czyli pas ciągnie się wzdłuż swojej krawędzi.)
Narożniki łączą pasy: np. 57 ma od zachodu pas 55 (przy krawędzi −y), a od północy pas 53 (przy
krawędzi −x) — oba kończą się w narożniku NW komórki 57. Bloki częściowe stoją często jeden na
drugim (pod spodem ten sam kod: 53–56 22,8–27,4 %, 57–60 33,2–38,9 %, 61 33,8 %) i często mają `flat`
(np. 53: `left` 62,0 %, `right` 77,0 %, pokrywa 69,5 %) — wygląda to na płoty i słupy.

**Status: BLOK.** Mapa nie mówi nic o grubości pasa ani rozmiarze słupka. Wartości [T] (24/64 i
16/64) to wybór strony trzeciej. Nierozstrzygnięte także: pełna wysokość (wsparcie tylko
pośrednie — układanie w stosy), które strony są widoczne i czy `flat` oznacza przezroczystą
dwustronną ścianę (WG-FLAT).

### 4.8 Kody 62 i 0

* **62:** 0 komórek w 17 mapach [M]; nazwa „Reserved” tylko z [T]. Brak zadania; kod spoza
  przyszłej specyfikacji powinien być zgłaszany diagnostyką, a nie zgadywany.
* **0:** bryła jednostkowa (T0001, [I]); nierozstrzygnięte punkty bez skosów — `world-geometry.md`
  sekcja 4 (widoczność ścian, `flat`, kafle zarezerwowane). Pokrywa 1023 na kodzie 0: 3 komórki.

## 5. Rodziny diagonalne, trójboczne i kafel 1023 — podsumowanie

| Rodzina | Kody | Pokrywa 1023 | Słowo powierzchni ukośnej | Status |
|---|---|---|---|---|
| Ściana diagonalna | 45–48 | 10 komórek (45: 4, 46: 4, 47: 2) — znaczenie nieznane | `left` (45, 47) / `right` (46, 48) | CZĘŚĆ (pokrywa BLOK) |
| Skos diagonalny trójboczny | 49–52 z pokrywą 1023 | 6 402 komórki; atrapa: tekstura nie z pokrywy [H] | `left` (49, 51) / `right` (50, 52) | BLOK (H-TET vs H-HIP) |
| Skos diagonalny czteroboczny | 49–52 bez 1023 | — (pokrywa = zwykły kafel, 80–85 % ≠ 0) | jw. | BLOK (H-COMP vs H-VALLEY) |
| Pozostałe | 0 (3), reszta 0 | kod 0: nieznane (WG-RESERVED) | — | — |

Nasz silnik [I] pomija dziś każdą pokrywę 1023 (`draw_list.cpp:101`); slot 1023 atlasu to biały kafel
pomocniczy, nie tekstura 1023 (`world-geometry.md` sekcja 4, wiersz 10).

## 6. Proponowany podział na zadania

Zasady: jedno zadanie = najwyżej jedna rodzina; CPU-owy kod w `engine/scene` (jak T0005), bez zmian
renderera, dopóki projekcja z wysokością nie jest gotowa. Oczekiwania testów są **syntetyczne**
(`provenance: synthetic`, identyfikatory `synthetic-slope-*`, komentarz „SYNTHETIC, hypothesis, see
docs/specs/world-slopes.md”) i wpisywane ręcznie z tabel tego dokumentu, **niezależnie od kodu** i
osobno dla każdej rodziny (żadna rodzina nie wyprowadza oczekiwań z innej). Przejście takich testów
nie jest dowodem zgodności z oryginałem.

| # | Zadanie | Kody | Wymagane dowody przed startem | Oczekiwania syntetyczne | Poza zakresem |
|---|---|---|---|---|---|
| S1 | Skosy 45° | 41–44 | brak dodatkowych: sekcje 4.1–4.2 ([M] + H-CONT; H-STEP trywialne dla n = 1) | dla każdego kodu: wysokości NW/NE/SE/SW; które słowo daje którą ścianę; kształt ścian (2 trójkąty wzdłuż osi, prostokąt po stronie wysokiej, brak po niskiej); nawinięcie na zewnątrz; ściana z kaflem 0 nie powstaje ([H], `world-geometry.md` sekcja 4, wiersz 6) | UV ścian bocznych, `flat`, widoczność ścian, renderer |
| S2 | Skosy 26° | 1–8 | jw. + H-STEP (n = 2) | jw. dla 8 kodów; ciągłość krawędzi w parach 1–2, 3–4, 5–6, 7–8 i na grzbietach 2/4 i 6/8 | jw. |
| S3 | Skosy 7° | 9–40 | jw. + H-STEP (n = 8) | jw. dla 32 kodów; ciągłość wzdłuż łańcuchów i grzbietów o równym k | jw. |
| S4 | Ściany diagonalne (bez pokrywy) | 45–48 | brak dodatkowych dla ścian (sekcja 4.4) | płaszczyzna i narożniki ściany ukośnej, wybór słowa `left`/`right`, dwie pozostałe ściany, brak trzeciej | **pokrywa** (BLOK do decyzji H-FOOT), UV ściany ukośnej |
| S5 | Skosy trójboczne | 49–52 + pokrywa 1023 | **ZABLOKOWANE**: rozstrzygnięcie H-TET/H-HIP przez E1, E2 lub E3 | dopiero po rozstrzygnięciu | — |
| S6 | Skosy czteroboczne | 49–52 bez 1023 | **ZABLOKOWANE**: H-COMP/H-VALLEY przez E1, E2 lub E3 | dopiero po rozstrzygnięciu | — |
| S7 | Pasy częściowe | 53–56 | **ZABLOKOWANE**: grubość pasa (E1, E2 lub E3) | — | — |
| S8 | Słupki narożne | 57–60 | **ZABLOKOWANE**: rozmiar (E1, E2 lub E3) | — | — |
| S9 | Słupek środkowy | 61 | **ZABLOKOWANE**: rozmiar (E1, E2 lub E3) | — | — |
| — | Kod 63 | 63 | brak zadania geometrii; E1 potwierdza, że 63 nie ma własnego kształtu | — | kolizje |
| — | Kod 62 | 62 | brak zadania (nie występuje) | — | — |

Kolejność S1 → S2 → S3 → S4: od jednego kawałka na poziom do ośmiu, potem rodzina bez pochyłej
pokrywy. Pokrywa 45–48 dołącza do S4 albo do osobnego zadania dopiero po decyzji H-FOOT (pytanie do
Astry). Wspólny dla S1–S3 nierozstrzygnięty punkt — UV ścian bocznych i orientacja tekstury pochyłej
pokrywy — powinien trafić do jednego zadania dowodowego (E3), zanim cokolwiek zacznie to rysować.

## 7. Zadania dowodowe (prerekwizyty)

| # | Dowód | Co rozstrzyga | Warunek wykonania |
|---|---|---|---|
| E1 | Przepisanie (własnymi słowami) sekcji o skosach, 3-/4-bocznych skosach, kaflu 1023, blokach częściowych i `flat` z dokumentu DMA v8.2 do `docs/formats/gmp.md` | nazwy kodów [T] → [F]; S5–S9 o ile dokument podaje kształty i wymiary; kod 63 | dostęp do dokumentu: w tej sesji brak sieci; potrzebna lokalna kopia od właściciela albo sesja z dostępem do gtamp.com |
| E2 | Analiza GTA2_RE ze **wskazaną rewizją** (SHA), opis własnymi słowami, bez kodu (`docs/reference/gta2re.md`) | kształt 49–52 (H-TET/H-HIP, H-COMP/H-VALLEY), wymiary 53–61, czy 63 rysuje się jak blok | lokalna kopia GTA2_RE lub dostęp sieciowy; wynik to [B], nie [O] |
| E3 | Przechwycenie oryginału (tryb M, `world-geometry.md` sekcja 7) w miejscach wybranych lokalnym zapytaniem do danych: rampa 26°/7° z ustawionymi ścianami bocznymi, grupa 2 × 2 kodów 49–52 z pokrywą 1023, wklęsły narożnik 49–52 bez 1023, płot z 53–60, słupek 61 | UV ścian, kształty i wymiary — jedyny dowód [O] | przebieg dociera do świata (T0012: dotąd nie), ustawienie kamery klawiszami; prośba wyłącznie przez `gfx_requests` w osobnym zadaniu |

## 8. Weryfikacja tego dokumentu

* Pokrycie tabeli z sekcji 3 i jej liczby względem T0010:
  `python3 -I tools/map-analysis/check_slope_table.py docs/specs/world-slopes.md "$TMPDIR/inventory.txt"`
  — 2026-10-13: `slope table: 64 rows, PASS` (każdy kod 0–63 dokładnie raz; suma komórek, liczba
  map i liczby dla `wil`/`ste`/`bil` równe raportowi T0010). Bez drugiego argumentu sprawdza samo
  pokrycie kodów.
* Skrypt z 1.1 potwierdził identyczne liczby komórek na każdej mapie i dla każdego kodu oraz 6 415
  pokryw 1023.
* Do repo trafiają tylko agregaty (liczby, procenty) — bez rekordów bloków, współrzędnych,
  kafli i pikseli. Nie skopiowano kodu GTA2_RE (nieprzeglądany) ani OpenGTA2 (opisano tylko jego
  interpretację).
