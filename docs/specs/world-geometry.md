# Geometria świata bez skosów: konwencje i luki w dowodach

Kontrakt dla następnej sesji implementującej ściany i wysokość bloków. Oddziela fakty od
założeń o zachowaniu oryginalnego GTA 2. Zakres: bloki **bez skosów** (`slope() == 0`), pięć
ścian bloku, flagi słów ścian, obrót/odbicie tekstur, projekcja i wysokość. Skosy, oświetlenie
pokryw, animacje kafli i kolizje są poza zakresem (wymienione tylko tam, gdzie dotykają tabeli
decyzji). Stan na rewizji `cb06317` (gałąź `agent/t0002`), 2026-10-12.

Aktualizacja 2026-10-13 (T0008): zadanie z sekcji 8 (T0005) jest zakończone; poprawiony przykład
przemianowania poziomów w sekcji 6.2 (zachowuje `r − ℓ`); sekcje 1, 7 i 8 opisują referencje z
oryginału według obecnej polityki właściciela (przechwytywanie wyłącznie przez orkiestrator).

## 0. Statusy

Każde stwierdzenie o zachowaniu ma lokalizację źródła i jeden z czterech statusów
(zgodnie z [../reference/gta2re.md](../reference/gta2re.md), pkt 4):

| Znacznik | Status | Znaczenie |
|---|---|---|
| **[F]** | specyfikacja formatu | Dokument DMA Design w transkrypcji z `docs/formats/*.md`. Mówi, co zapisano w pliku, nie jak oryginał to rysuje. |
| **[A]** | istniejący dowód automatyczny | Asercja w teście CTest w repozytorium; zakres dokładnie taki, jak w ciele testu. |
| **[O]** | pomiar na oryginalnej grze | Zrzut lub ślad `provenance: originalGta2` z uruchomionego oryginału. **W przejrzanych materiałach nie znaleziono żadnego** (sekcja 1). |
| **[H]** | hipoteza | Wszystko inne, także propozycje konwencji w tym dokumencie. |

Trzy dodatkowe oznaczenia, które **nie** są dowodem zachowania oryginału:

* **[W]** — historyczna obserwacja wizualna: ktoś obejrzał zrzut **naszego silnika** i ocenił
  go „na oko”. Nie jest to porównanie z oryginałem ani test automatyczny.
* **[D]** — historyczny, jednorazowy pomiar na plikach danych (bez testu, który by go powtarzał).
  Mówi o zawartości `.gmp`/`.sty`, nie o renderowaniu.
* **[I]** — fakt o obecnym kodzie GTA2.0 (co robi nasz silnik). Nie mówi nic o oryginale.

## 1. Inwentarz dowodów

| Źródło | Lokalizacja | Status | Co faktycznie pokazuje |
|---|---|---|---|
| Dokument „GTA2 map format” v8.2 | transkrypcja: `docs/formats/gmp.md:3-20` | [F] | układ bitów słów ścian i pokrywy, `slope_type`, chunki. Oryginalna strona (gtamp.com) **nie była dostępna w tej sesji** (brak dostępu sieciowego); znaczenie flag poza ich nazwami nie jest w repo przepisane. |
| Dokument „GBH Style Format” v4.6 | `docs/formats/sty.md:3-20` | [F] | układ kafli 64×64, kolor 0 przezroczysty, kafle 992–1023 zarezerwowane. |
| Dekodowanie bitów | `tests/unit/assets_tests.cpp:41-58` | [A] | Trzy wybrane wzorce syntetyczne: `SideFace::decode(0xFFFF)` (wszystkie bity ustawione → kafel 1023, wszystkie flagi, R270; `:43-49`), `LidFace::decode(5 \| 2<<10 \| 1<<14)` (sprawdzane tylko kafel 5, oświetlenie 2, R90 — `flat`/`flip` pokrywy nie są asertowane; `:50-53`) i `slope_type = (41<<2) \| 2` (podłoże Pavement, skos 41; `:54-57`). Przy wszystkich flagach ustawionych naraz zamiana dwóch bitów flag nie zostałaby wykryta. Nic o znaczeniu flag. |
| Kontrakt T0001 | `engine/scene/include/gta20/scene/block_geometry.hpp:9-25`, `engine/scene/src/block_geometry.cpp:9-15` | [I] | położenie, normalne i kolejność narożników pięciu ścian bloku jednostkowego. |
| Testy T0001 | `tests/unit/scene_tests.cpp:135-255` | [A] | kod spełnia kontrakt T0001 (płaszczyzny, nawinięcie, translacja, wspólne krawędzie). Oczekiwania są **syntetyczne** (`scene_tests.cpp:39-40`), wyprowadzone z kontraktu, nie z oryginału. |
| Osie mapy | `tests/unit/gamedata_tests.cpp:141-174` | [A] | Tylko `wil.gmp`, tylko komórki wewnętrzne x, y ∈ 1..254 (`:157-158`), wszystkie z = 0..7 (`:156`). Liczone są słowa `left`/`right`/`top`/`bottom` z bitem `wall` i kaflem ≠ 0 (`:162`). Sąsiad na tym samym z w kierunku `−x`/`+x`/`−y`/`+y` jest „otwarty”, gdy `!solid(...)`, gdzie `solid` = pokrywa ≠ 0 **lub** dowolna ściana boczna z bitem `wall` (`:58-60`) — „otwarty” nie znaczy „pusty” (np. blok bez pokrywy z kaflami ścian bez bitu `wall` jest otwarty, a blok drogi z pokrywą nie). Asercje (`:170-172`): > 1000 takich ścian na stronę, otwarty sąsiad w kierunku dokumentu w > 75 %, w kierunku zamienionych osi (`left`→`y−1`, `right`→`y+1`, `top`→`x−1`, `bottom`→`x+1`, `:153`) w < 35 %. Liczby 84–86 % / ~20 % (`gmp.md:24-25`, komentarz `gamedata_tests.cpp:143`) to [D]. |
| Kafle zarezerwowane | `tests/unit/gamedata_tests.cpp:96-111`, `:214-233` | [A] | Wszystko liczone po **unikalnych rekordach bloków** (`unique_blocks()`, `:98`, `:224`), nie po komórkach mapy — bez ważenia liczbą użyć. W `wil`/`ste`/`bil` żaden rekord nie ma ściany bocznej z kaflem ≥ 992 (`:101`); każdy kafel ścian i pokryw jest < `tile_count` stylu (= 992) albo = 1023 (`:227`); wśród rekordów z pokrywą 1023 co najmniej 95 % ma skos 49–52 (`:104-110`, osobno dla każdego miasta). Mapy `mike1*` i pozostałe nie są tym objęte. |
| 98,8 % / 992–1022 tylko w `mike1*` | `docs/formats/gmp.md:28-30` | [D] | Brak testu, który to powtarza. **Mianownik 98,8 % jest nieznany**: `gmp.md` go nie zapisuje, a metoda obecnego testu (unikalne rekordy) nie mówi, jak liczono wcześniejszy pomiar (rekordy czy komórki mapy). Zweryfikowane jest tylko ≥ 95 % po unikalnych rekordach z wiersza wyżej. |
| CMAP | `tests/unit/gamedata_tests.cpp:121-139` | [A] | tylko `wil.gmp` i `mike1m.gmp`: oba chunki się dekodują, a DMAP ma **więcej unikalnych bloków** niż CMAP. Test **nie** porównuje komórek; „inna, starsza wersja miasta” i „inne pokrywy” (`gmp.md:17`, `:27`) to [D]. |
| Układ kolumn DMAP 100 % | `docs/formats/gmp.md:16`, `:23` | [D] | parser przechodzi na 17 mapach (`gamedata_tests.cpp:77-90`), ale przyleganie kolumn nie jest asercją. |
| Orientacja pokryw, obrót, odbicie | `docs/formats/gmp.md:31-32`, `docs/reports/2026-10-12-original-assets.md` (sekcja 6–7) | [W] | zrzuty **naszego** silnika (lokalnie `~/Games/GTA2-work/review/*.png`, opisane w lokalnym `REPORT.md`): pasy ciągłe, napisy „SLOW” nieodbite. To nie są zrzuty oryginału ani porównanie automatyczne. |
| Zrzuty/ślady oryginału | repozytorium (`docs/`, `tests/`), `~/Games/GTA2-work/REPORT.md` i nazwy plików graficznych w `~/Games/GTA2-work/` (`review/`, `Disk1/`) | — | **W przejrzanych materiałach nie znaleziono żadnej referencji z oryginału.** Pliki w `review/` to według `REPORT.md` zrzuty naszego silnika i podglądy atlasu (`render-*`, `wil-*`, `tiles-*`); `Disk1/setup.bmp` to grafika instalatora. Nie przeglądano innych miejsc na dysku, więc to stwierdzenie nie wyklucza, że lokalne referencje istnieją gdzie indziej. Stan 2026-10-13: jest pierwszy scenariusz przechwycenia `tests/gfx/scenarios/m1-downtown-baseline.json` (T0009), ale orkiestrator nie wykonał jeszcze żadnego przebiegu — nadal brak [O]. |
| Dokumentacja AWS i hosta EC2 | `docs/aws/integration.md`, `docs/testing.md` („Dane referencyjne z oryginału”), `docs/ci/gpu-testing.md` | — | Opisuje host EC2 z oryginalnym GTA 2 i prywatny bucket artefaktów. Według obecnej polityki właściciela agenci **nie** uruchamiają poleceń AWS ani hosta; przebiegi na hoście zleca się wyłącznie przez orkiestrator (sekcja 7). Opisy ręcznej obsługi hosta w tych plikach nie są ścieżką dla agentów. |
| GTA2_RE | `docs/mechanics-matrix.md:17`, `:19` | — | tylko nazwy funkcji jako wskazówka, gdzie szukać (`DrawGradientSlope_4F6630`, `DrawDiagonalWall_4F02D0`, `Draw_4F6A20`, `Camera.cpp`). W tej sesji GTA2_RE nie był czytany (brak lokalnej kopii); nic tutaj nie pochodzi z jego kodu. |

## 2. Układ współrzędnych

| Stwierdzenie | Źródło | Status |
|---|---|---|
| Mapa ma 256 × 256 × 8 komórek (x, y, z). | `docs/formats/gmp.md:15`, `:18`; `gmp_map.hpp:16-18` | [F] |
| Indeks kolumny DMAP to `y·256 + x`; x rośnie wraz z kolumną mapy, y z wierszem. | `gmp.md:24`; pośrednio `gamedata_tests.cpp:141-174` | [F] + [A] (tylko kierunki ścian na `wil.gmp`) |
| Ściany `left`/`right`/`top`/`bottom` graniczą z sąsiadem w kierunku `−x`/`+x`/`−y`/`+y`. | `gamedata_tests.cpp:141-174` | [A] (statystycznie, `wil.gmp`) |
| `top`/`bottom` to **ściany pionowe** (granica z poprzednim/następnym wierszem), nie poziome powierzchnie. | `block_geometry.hpp:18-19` | [I]; wynika z [A] powyżej |
| Na ekranie +y mapy idzie w dół, +x w prawo. | `camera2d.hpp:14-15`, `camera2d.cpp:6-9` | [I] dla naszego silnika. Dla oryginału **[H]**: obserwacja [W] (ciągłe pasy, czytelne napisy, `gmp.md:31-32`) dotyczy tylko naszego renderu i nie wyklucza np. obrazu odbitego względem oryginału. |
| z rośnie w górę; poziom z = 0 jest najniższy. | `block_geometry.hpp:11-13`; kolejność rysowania `draw_list.cpp:94` | [I]; dla oryginału [H] — kierunek osi z nie ma w repo ani dowodu [A], ani [O]. Kolejność „niższe najpierw” to [W]. |
| Gracz porusza się w `z ∈ 1..6`. | `gmp.md:48-49` | [F] — nie mówi, na jakiej wysokości świata leży pokrywa poziomu z. |

## 3. Pięć ścian bloku

Kontrakt T0001 (`block_geometry.hpp:11-24`, [I], sprawdzany [A] w `scene_tests.cpp:135-255`):
blok o początku (x, y, z) zajmuje [x, x+1] × [y, y+1] × [z, z+1]. Kolejność narożników p0..p3 jest
tą z `block_geometry.cpp:9-15`; trójkąty {0,1,2} i {0,2,3}.

Etykiety tekstury: **A** = lewy górny róg obrazka kafla (u = 0, v = 0, pierwszy wiersz w atlasie,
`draw_list.hpp:37`), **B** = prawy górny, **C** = prawy dolny, **D** = lewy dolny. Kolumna
„proponowane osie” to [H]: obraz oglądany z zewnątrz ściany przez widza stojącego pionowo
(głowa w +z), przy orientacji świata jak na ekranie (x = wschód, y = południe). Dla pokrywy jest to
dokładnie to, co dziś robi `append_map_textured` dla obrotu 0 bez odbicia ([I], sekcja 5).

| Ściana | Słowo GMP | Sąsiad (oś mapy) | Płaszczyzna | Normalna | p0, p1, p2, p3 (offset od początku) | Proponowane u (A→B), v (A→D) | Proponowane A, B, C, D |
|---|---|---|---|---|---|---|---|
| lid | `lid` | z + 1 (w górę) | z = z₀+1 | +Z | (0,0,1) (1,0,1) (1,1,1) (0,1,1) | u = +x, v = +y | p0, p1, p2, p3 |
| left | `left` | x − 1 | x = x₀ | −X | (0,1,0) (0,0,0) (0,0,1) (0,1,1) | u = +y, v = −z | p2, p3, p0, p1 |
| right | `right` | x + 1 | x = x₀+1 | +X | (1,0,0) (1,1,0) (1,1,1) (1,0,1) | u = −y, v = −z | p2, p3, p0, p1 |
| top | `top` | y − 1 | y = y₀ | −Y | (0,0,0) (1,0,0) (1,0,1) (0,0,1) | u = −x, v = −z | p2, p3, p0, p1 |
| bottom | `bottom` | y + 1 | y = y₀+1 | +Y | (1,1,0) (0,1,0) (0,1,1) (1,1,1) | u = +x, v = −z | p2, p3, p0, p1 |

Uwagi:

* Przy tej propozycji każda ściana pionowa ma ten sam wzór: p0 = C, p1 = D, p2 = A, p3 = B
  (dolne narożniki pierwsze, `block_geometry.hpp:24`). Ściany `left`/`right` i `top`/`bottom`
  są względem siebie lustrzane w u (każda „czytana z zewnątrz”). Alternatywa — u zawsze wzdłuż
  +x/+y niezależnie od strony — jest równie możliwa ([H]); rozstrzyga ją scenariusz WG-ROT-WALL
  z napisem na ścianie.
* Nawinięcie: (p1−p0)×(p2−p0) wskazuje na zewnątrz w matematycznym, prawoskrętnym (x, y, z)
  ([A], `scene_tests.cpp:152-186`). Ponieważ ekran pokazuje +y w dół, ściany zwrócone do widza
  wyglądają na ekranie jak trójkąty **zgodne z ruchem wskazówek zegara**. Standardowa kamera
  „look-at” z +z pokazałaby +y w górę, więc projekcja musi zawierać odbicie osi y (albo
  równoważne), a wybór `cull_mode` musi to uwzględnić. Dziś culling jest wyłączony
  (`renderer.cpp:180`, [I]).
* **Pokrywa na `origin.z + 1` to kontrakt matematyczny.** T0001 ustala tylko, że pokrywa jest
  górną ścianą sześcianu jednostkowego. Nie dowodzi, że poziom GMP z odpowiada wysokości świata
  [z, z+1], że jednostka wysokości równa się szerokości bloku, ani że pokrywa poziomu z jest tam,
  gdzie stoi pieszy na poziomie z + 1. Ani repo, ani dokumenty w transkrypcji nie podają
  przeliczenia „poziom GMP → wysokość świata”; to [H] do zmierzenia (WG-ELEV, sekcja 6).
* Bloki nie mają podstawy (`block_info` ma 5 słów, `gmp.md:11`, [F]): dolnej ściany sześcianu nie
  ma w formacie i kontrakt jej nie definiuje.
* Nazwy „wschód/południe” to tylko etykiety kierunków ekranu (+x w prawo, +y w dół), nie
  stwierdzenie o oryginale.

## 4. Tabela decyzji

„Kolizja” to tylko znaczenie nazwy flagi z dokumentu ([F], `gmp.md:12`, `gmp_map.hpp:26-28`);
symulacja używa jej prototypowo (`engine/sim/src/world.cpp:35-37`, [I]). **Z flag kolizji nie
wolno wnioskować o widoczności.** Kolumna „Oryginał: renderowanie” jest wszędzie, gdzie nie ma
dowodu [O], oznaczona jako nierozstrzygnięta.

| # | Przypadek (słowo ściany) | Dane: co wiemy | Nasz silnik dziś | Oryginał: renderowanie | Wymagany dowód |
|---|---|---|---|---|---|
| 1 | Ściana boczna, kafel zwykły, `wall` = 0 | bity [F] (`gmp.md:12`), dekodowanie [A] (`assets_tests.cpp:41-58`). Że takie słowa występują w miastach: niezmierzone. | nie rysowana (ścian nie ma w ogóle, `draw_list.hpp:70-73`) [I] | **NIEROZSTRZYGNIĘTE** | WG-WALL-BITS |
| 2 | Ściana boczna, kafel zwykły, `wall` = 1 | jak 1; statystyka osi [A] dotyczy tylko tej grupy (`gamedata_tests.cpp:162`) | nie rysowana [I]; w `append_map` zmienia kolor kolumny (`draw_list.cpp:128-135`) | **NIEROZSTRZYGNIĘTE** (że jest widoczna — [H]) | WG-WALL-BITS |
| 3 | `bullet_wall` = 1 (z `wall` = 0 lub 1) | nazwa flagi [F]; znaczenie dla pocisków poza zakresem | ignorowana [I] | **NIEROZSTRZYGNIĘTE** (brak podstaw, by sądzić, że wpływa na obraz — [H]) | WG-WALL-BITS |
| 4 | `flat` = 1 na ścianie bocznej | nazwa bitu [F]; znaczenie nieprzepisane do repo | ignorowana [I] | **NIEROZSTRZYGNIĘTE** (np. płaska przezroczysta/dwustronna ściana, inne położenie — wszystko [H]) | przeczytać dokument (sekcja „flat”) + WG-FLAT |
| 5 | `flat` = 1 na pokrywie | nazwa bitu [F] (`gmp.md:13`) | ignorowana; pokrywa rysowana normalnie [I] (`draw_list.cpp:97-113`) | **NIEROZSTRZYGNIĘTE** | WG-FLAT |
| 6 | Słowo = 0 (kafel 0, bez flag) | blok 0 jest pusty: [A] (`gamedata_tests.cpp:91`, z komentarzem „format document”) | pokrywa z kaflem 0 pominięta [I] (`draw_list.cpp:99-103`) | brak rysowania — [H] (sensowne, ale bez [O]) | WG-WALL-BITS (komórka kontrolna) |
| 7 | Kafel 0, ale z flagami (`wall`/`flat`/obrót) | możliwe w formacie; występowanie niezmierzone | jak 6 [I] | **NIEROZSTRZYGNIĘTE** (np. niewidzialna ściana) | WG-WALL-BITS |
| 8 | Kafel 1 … `tile_count`−1 (zwykły) | kafel istnieje w stylu: [A] dla `wil`/`ste`/`bil` (`gamedata_tests.cpp:214-233`) | pokrywa rysowana [I]; ściany nie. Obserwacja [W] naszego renderu: pasy ciągłe, napisy czytelne (`gmp.md:31-32`) | **NIEROZSTRZYGNIĘTE** (pokrywy i ściany) | WG-ELEV, WG-ROT-LID, WG-WALL-SEL |
| 9 | Kafel 992–1022 | zarezerwowane [F] (`sty.md:20`); brak w miastach [A] (`gamedata_tests.cpp:101`, `:227`); tylko w `mike1*` [D] (`gmp.md:29-30`); cel „animacje silnika” — bez źródła w repo, [H] | pominięty (≥ `tile_count`) [I] (`draw_list.cpp:101`) | **NIEROZSTRZYGNIĘTE** (chunk `ANIM` niezaimplementowany, `gmp.md:20`) | WG-RESERVED |
| 10 | Kafel 1023 (znacznik) | znacznik trójbocznych skosów: [A] ≥ 95 % na skosach 49–52 (`gamedata_tests.cpp:104-111`); 98,8 % [D]; na ścianach bocznych miast nie występuje [A] (`:101`) | pominięty [I] (`draw_list.cpp:99-101`); slot 1023 atlasu to biały kafel pomocniczy (`draw_list.hpp:30`) — **nie** tekstura 1023 | **NIEROZSTRZYGNIĘTE** (skosy poza zakresem) | WG-RESERVED (po skosach) |
| 11 | Kafel ≥ `tile_count` stylu, < 992 | dla miast nie występuje [A] (`:227`); `fstyle.sty` ma 0 kafli [A] (`gamedata_tests.cpp:189`); parowanie map bonusowych ze stylem nieustalone | pominięty [I] | **NIEROZSTRZYGNIĘTE** | WG-RESERVED |
| 12 | Ściana między dwoma pełnymi blokami (zasłonięta) | — | nie dotyczy (brak ścian) | **NIEROZSTRZYGNIĘTE**. Na zrzucie widać tylko, że jej pikseli nie ma; czy oryginał ją pomija, czy rysuje i zasłania, jest **nieobserwowalne** bez osobnego śladu wywołań rysowania | WG-WALL-SEL (piksele); ślad wywołań — brak źródła |

## 5. Obrót i odbicie

### 5.1 Co robi dziś nasz silnik (tylko pokrywy) — [I]

Źródło: `draw_list.cpp:104-113` i `quad.vert.glsl:17-24` (odpowiedniki: `quad.hlsl:32-38`,
`quad.metal:30`). Bez obrotu narożnik `a_corner = (−½, −½)` dostaje (u0, v0) = **A** i leży w
(x, y) — lewy górny róg komórki na ekranie. Odbicie zamienia u0 ↔ u1 **w przestrzeni tekstury,
przed obrotem**; obrót o `kod · 90°` jest **zgodny z ruchem wskazówek zegara na ekranie** (dla
90° narożnik (−½, −½) trafia do (+½, −½)). Wynikowe położenie etykiet (NW = (x, y), NE = (x+1, y),
SE = (x+1, y+1), SW = (x, y+1)):

| Kombinacja | NW | NE | SE | SW | Opis |
|---|---|---|---|---|---|
| R0 | A | B | C | D | tożsamość |
| R90 | D | A | B | C | obrót 90° w prawo |
| R180 | C | D | A | B | obrót 180° |
| R270 | B | C | D | A | obrót 90° w lewo |
| flip + R0 | B | A | D | C | lustro poziome |
| flip + R90 | C | B | A | D | lustro względem przekątnej NE–SW |
| flip + R180 | D | C | B | A | lustro pionowe |
| flip + R270 | A | D | C | B | lustro względem przekątnej NW–SE (transpozycja) |

### 5.2 Co wiemy o oryginale

* Bity: obrót 14–15, odbicie 13 dla ścian i pokryw — [F] (`gmp.md:12-13`), dekodowanie [A]
  (`assets_tests.cpp:41-58`).
* Kierunek obrotu, oś odbicia i kolejność złożenia: **niezweryfikowane**. Jedyne wsparcie to [W]
  (`gmp.md:31-32`: ciągłe pasy, nieodbite napisy na Downtown w naszym renderze). Komentarz
  `draw_list.cpp:111-112` powoływał się na „docs/formats/sty.md”, który nie zawiera żadnej
  informacji o obrocie; od T0005 wskazuje na tę sekcję (`docs/specs/world-geometry.md`, sekcja 5).
  Status pozostaje [H].
* Dlaczego obserwacja [W] nie wystarcza: (a) „odbicie przed obrotem w prawo” i „obrót w lewo, potem
  odbicie” dają **identyczne** obrazy dla wszystkich kombinacji z odbiciem; różnią się tylko
  R90/R270 bez odbicia. (b) „Odbicie, potem obrót” vs „obrót, potem odbicie” (oba w prawo) różnią
  się tylko dla flip + R90 i flip + R270 (zamieniają się miejscami). (c) Lustro pionowe zamiast
  poziomego = obecna tabela z kodem obrotu + 2. Ciągłość pasów na kaflach symetrycznych nie
  rozróżnia żadnej z tych możliwości; potrzebny jest kafel bez symetrii (napis) we **wszystkich
  ośmiu** kombinacjach.

### 5.3 Propozycja dla ścian — [H]

Ten sam algorytm w lokalnych osiach (u, v) z sekcji 3: odbicie zamienia A↔B i D↔C w przestrzeni
tekstury, potem obrót o `kod · 90°` zgodnie z ruchem wskazówek zegara, **patrząc na ścianę z
zewnątrz**. Zamiast NW/NE/SE/SW: lewy górny / prawy górny / prawy dolny / lewy dolny narożnik
widziany z zewnątrz (dla R0 bez odbicia = A, B, C, D z tabeli w sekcji 3).

### 5.4 Wymagane pokrycie testami

Dla **każdej** z pięciu ścian osiem kombinacji: R0, R90, R180, R270, flip + R0, flip + R90,
flip + R180, flip + R270 — razem 40 przypadków. Test silnika (syntetyczny, CPU) sprawdza, który
narożnik geometrii (p0..p3) dostaje którą etykietę A–D. Test porównawczy z oryginałem (WG-ROT-LID,
WG-FLIP-ORDER, WG-ROT-WALL) dostarcza oczekiwane etykiety; dopóki go nie ma, oczekiwania dla
pokrywy kodują tylko obecne zachowanie [I], a dla ścian propozycję [H], i tak muszą być opisane.

## 6. Projekcja i wysokość

### 6.1 Dlaczego Camera2D + z = 0 nie pokaże budynków

* `Camera2D::view_projection` (`camera2d.cpp:5-18`) to macierz ortograficzna: x i y ekranu zależą
  tylko od x i y świata, `m[10] = 1`, `m[15] = 1`, a wiersz w nie zależy od z ([I]).
* Wszystkie shadery wpisują stałe z = 0: `quad.vert.glsl:22`, `quad.hlsl:36`, `quad.metal:30`; wejście
  wierzchołka ma tylko 2D (`a_corner` vec2, `i_center` vec2; `renderer.cpp:150-161`) ([I]).
* Skutek: (1) pokrywy wszystkich poziomów z o tych samych (x, y) trafiają w ten sam piksel —
  wysokość budynku nie zmienia ani rozmiaru, ani położenia; (2) ściana pionowa (stałe x lub y,
  zmienne z) rzutuje się na odcinek o zerowej powierzchni, więc żadna ściana nie może być widoczna;
  (3) nawet dodanie z do wierzchołka nic nie zmieni, bo macierz ortograficzna z góry ignoruje z dla
  x/y. Potrzebna jest projekcja, w której położenie na ekranie zależy od wysokości (perspektywa z
  podziałem przez w albo skala/przesunięcie zależne od z), plus rozstrzygnięcie kolejności rysowania
  (dziś: kolejność listy, bez bufora głębi — `renderer.cpp:172-183` nie ustawia celu głębi; [I]).
* Że oryginał używa rzutu perspektywicznego z kamerą nad środkiem ekranu i pokazuje ściany zwrócone
  ku środkowi ekranu, to powszechne wrażenie z gry — w repo **[H]**, bez [O].

### 6.2 Pomiary na oryginale potrzebne do wyboru parametrów

Bez zgadywania stałych. Warunki każdego pomiaru: oryginał (9.6 lub 10.5 — identyczne dane mapy i
stylu, `gmp.md:22`), zapisana rozdzielczość, pieszy stojący w spoczynku, zapisana pozycja.

**Model roboczy [H]:** kamera patrzy pionowo w dół (rzut środkowy). Punkt w odległości poziomej
`x` bloków od osi kamery, na pokrywie o indeksie poziomu `ℓ`, trafia w piksel

    p − c = K · x / (r − ℓ),        K = f · w / h,   r = H / h

gdzie `c` to punkt główny (piksele), `f` ogniskowa (piksele), `w` szerokość bloku i `h` wysokość
poziomu w jednostkach świata, `H` wysokość kamery w tych samych jednostkach.

**Co jest obserwowalne, a co nie.** Obraz zależy tylko od `K`, `r` i `c`. Wielkości `f`, `w`, `h` i
`H` osobno **nie są identyfikowalne** ze zrzutów, bo istnieją przekształcenia zachowujące zarówno
`K`, jak i `r`: (a) `w → λw`, `f → f/λ` (przy stałych `h`, `H`); (b) `h → λh`, `H → λH`, `f → λf`
(przy stałym `w`). Oba dają identyczne piksele. (Samo przeskalowanie `h` i `H` bez `f` zmienia `K`,
a więc obraz — nie jest niezmiennikiem.) Przekształcenie (b) zmienia `h / w`, więc **proporcja
`h / w` (czy blok jest sześcianem) pozostaje nierozstrzygnięta** — zrzuty z góry jej nie mierzą, a
przy tym modelu do odtworzenia obrazu nie jest potrzebna. Rozstrzygnęłaby ją tylko niezależna
kalibracja: wielkość znana jednocześnie w jednostkach poziomych i pionowych świata (np. wewnętrzne
współrzędne obiektu odczytane ze śladu oryginału). W przejrzanych materiałach takiego źródła nie
ma, więc `h / w` zostaje otwarte. Podobnie przemianowanie wszystkich poziomów o stałą
(`ℓ` → `ℓ + 1`) **razem z** `r` → `r + 1` zachowuje różnicę `r − ℓ` każdej pokrywy, a więc każdy
piksel: model zależy od `ℓ` i `r` tylko przez `r − ℓ`. (Para `ℓ` → `ℓ + 1` z `r` → `r − 1` zmienia
`r − ℓ` o 2 i **nie** jest niezmiennikiem.) Zrzuty wyznaczają więc `r` tylko względem przyjętego
oznaczenia poziomów pokryw, a nie „bezwzględną” wysokość świata pokrywy poziomu GMP z.

Pomiary niezależne (każdy wymaga zrzutu oryginału, [O]):

1. **Skala pokryw na poziom**: długość boku w pikselach górnych pokryw na co najmniej **trzech**
   różnych poziomach GMP, każda w pełni widoczna (bez zasłonięcia przez wyższego sąsiada).
   `s(ℓ) = K / (r − ℓ)`: dwa poziomy wyznaczają `r` i `K`, trzeci sprawdza model.
2. **Punkt główny `c`**: punkt zbieżności rzutów pionowych krawędzi budynków (narożników tych samych
   (x, y) na różnych poziomach) w co najmniej trzech ćwiartkach ekranu. Brak wspólnego punktu albo
   różne skale w osi x i y obalają model (kamera pochylona, piksele nie kwadratowe).
3. **Izotropia**: skala z pkt 1 mierzona osobno wzdłuż x i y mapy.
4. **Stabilność kamery**: powtórzenie pkt 1 po chwili postoju i po ponownym zatrzymaniu się — czy
   `r` i `K` w spoczynku są stałe (wpływ jazdy na kamerę jest [H] i poza zakresem).
5. **Wybór ścian**: które ściany mają **widoczne piksele** w zależności od ćwiartki względem `c`.
   Czy ściana zasłonięta jest wysyłana do rysowania, jest ze zrzutu nieobserwowalne.
6. **Bloki blisko kamery**: czy pokrywy przy dużym `ℓ` blisko `c` są przycinane.

Sprawdzenia spójności (nie są niezależnymi pomiarami): wysokość ściany w pikselach i położenie jej
dolnej/górnej krawędzi wynikają z rzutu pokryw poziomów, między którymi ściana leży — dają tylko
test, że krawędzie ścian pokrywają się z krawędziami tych pokryw przy `K`, `r`, `c` z pkt 1–2.
Tak samo zasięg widocznego gruntu wynika z `K`, `r` i rozdzielczości.

Nieobserwowalne ze zrzutów i bez źródła: indeks poziomu GMP, na którym „stoi” pieszy (wymagałby
śladu współrzędnych z oryginału), `h / w`, `f` i `H` osobno.

## 7. Przyszłe scenariusze porównawcze

**Referencje i ich przechowywanie (polityka właściciela, stan 2026-10-13).** Referencja = zrzut
**oryginalnego GTA 2** (`provenance: originalGta2`, [testing.md](../testing.md)) wykonany przez
**orkiestrator** na hoście testowym EC2 (GTA 2 9.6 + DDrawCompat, NVIDIA T4, Windows Server 2022).
Przebiegi przechwycenia wykonuje wyłącznie orkiestrator:

1. Agent opisuje przebieg scenariuszem `tests/gfx/scenarios/<id>.json` (tylko pola przyjmowane
   przez hosta: `id`, `wrapper`, `screen`, `durationSeconds`, `screenshotsAtSeconds`, `inputs`) i
   dokumentem `<id>.md`, który oznacza go jako referencyjne przechwycenie oryginału. Scenariusz
   musi przejść walidację offline (`cmake/ValidateGfxScenario.cmake`, CTest `gfx.scenario.<id>`) i
   być zacommitowany na gałęzi zadania.
2. Agent **prosi** o przebieg tylko polem `gfx_requests` w raporcie (`mode: capture`,
   `target: <id>`, uzasadnienie). Agenci nie uruchamiają poleceń AWS, nie włączają ani nie
   zatrzymują hosta i nie mają dostępu do bucketu.
3. Orkiestrator sprawdza prośbę, egzekwuje limity właściciela (czas przebiegu, liczba przebiegów
   na dobę, miesięczny budżet; prośby ponad limit są kolejkowane albo odrzucane) i wykonuje
   przebieg. Werdykt i lokalne ścieżki wyników wracają do agenta w kolejnych poleceniach.
4. Zrzuty zostają w **lokalnym katalogu wyników orkiestratora**, poza repozytorium — nigdy w repo,
   w artefaktach CI ani w miejscach publicznych. Prywatny bucket artefaktów właściciela obsługuje
   wyłącznie orkiestrator.

Do repo trafiają tylko: opis scenariusza, współrzędne mapy, numery kafli i słowa ścian, warunki
kamery oraz wyciągnięte liczby (piksele etykietowanych punktów, `K`, `r`, `c`). Wejścia scenariusza
to wyłącznie naciśnięcia klawiszy w czasie, więc ustawienie pieszego w zadanym miejscu (tryb M
poniżej) wymaga sekwencji klawiszy, a nie teleportu. Pierwszy scenariusz to przebieg odkrywczy
[m1-downtown-baseline](../../tests/gfx/scenarios/m1-downtown-baseline.md) (T0009), jeszcze nie
wykonany. **Żaden scenariusz nie ma dziś dowodu [O].**

**Odtwarzalność — dwa tryby ustawienia, ta sama scena w obu środowiskach:**

* **Tryb M (miejsce w oryginalnej mapie, domyślny):** obie strony renderują ten sam plik mapy i stylu
  z `GTA20_GAME_DIR` (np. `wil.gmp` + `wil.sty`). Scena = współrzędne (x, y, z) bloków. Że kamera
  oryginału w spoczynku stoi dokładnie nad pieszym, jest **[H]**. Zanim pozycja pieszego posłuży do
  ustawienia naszej kamery, trzeba zmierzyć poziome przesunięcie kamery względem pieszego (np. `c`
  z WG-PROJ wobec rzutu pozycji pieszego); dopiero wtedy nasz silnik ustawia środek kamery w
  odpowiednim punkcie mapy, przy tej samej rozdzielczości co zrzut. Miejsca wybiera lokalne zapytanie
  do danych mapy (zwraca tylko współrzędne i słowa ścian, nie piksele), np. „trzy wolnostojące
  kolumny o różnych poziomach górnej pokrywy w promieniu kadru”. Zależność: sposób doprowadzenia
  pieszego oryginału do zadanego miejsca i odczytania jego pozycji — nieustalony.
* **Tryb S (ta sama mapa syntetyczna w obu):** mapa z `GmpWriter` (`gmp_writer.hpp:11-27`),
  `provenance: synthetic`, id `synthetic-*`, wczytana **także przez oryginał**. Czy oryginał wczyta
  własną mapę (np. przez plik `.mmp` trybu wieloosobowego), jest [H] i niesprawdzone; do tego czasu
  tryb S jest tylko zapasowy. Scenariusz hosta (pola w pkt 1 wyżej) nie pozwala dziś dostarczyć
  własnej mapy, więc przez orkiestrator tryb S jest niewykonalny bez zmiany hosta przez właściciela.

**Porównania między różnymi scenami** (np. gdy referencja i nasz render nie dają się ustawić
identycznie). Bez warunków dodatkowych porównywalne są tylko wielkości dyskretne: reguła
widoczności ścian jako funkcja znaku przesunięcia bloku względem `c` w osi x i y oraz tabela
etykiet A–D dla każdej kombinacji obrót/odbicie; odpowiedniość ustala się po kluczach (ćwiartka,
kombinacja), nie po pikselach. Wielkości ciągłe — `r`, `K` podzielone przez wysokość okna, `c` jako
ułamek wymiarów okna, izotropia — wolno przenosić między zrzutami **tylko warunkowo**:

* że stan kamery w spoczynku jest stały, to [H] — najpierw musi to potwierdzić pomiar stabilności
  (sekcja 6.2, pkt 4) w kilku miejscach;
* że `K` skaluje się z wysokością okna, a `c` z jego wymiarami (normalizacja rozdzielczości), to
  [H] — najpierw trzeba zmierzyć `K` i `c` w co najmniej dwóch rozdzielczościach oryginału.

Dopóki któryś warunek nie jest potwierdzony, porównuje się tylko zrzuty w dopasowanych warunkach
(ta sama scena, rozdzielczość i stan kamery) albo kalibruje `K`, `r`, `c` osobno dla każdego zrzutu
na podstawie jego własnych pokryw. Surowe piksele — tylko w tej samej scenie i rozdzielczości.

| Nazwa | Ustawienie (tryb) | Obserwowany wynik | Pochodzenie referencji | Dowód dziś |
|---|---|---|---|---|
| **WG-ELEV** — wysokość pokryw | M (S zapasowo): co najmniej trzy **osobne** kolumny z górnymi pokrywami na różnych poziomach GMP plus pokrywa gruntu, z odstępami, tak by żadna górna pokrywa nie była zasłonięta przez wyższego sąsiada i każda mieściła się w kadrze; pieszy stoi w spoczynku | długość boku każdej górnej pokrywy w pikselach → `r`, `K` (sekcja 6.2, pkt 1) i sprawdzenie modelu trzecim poziomem | `originalGta2`, zrzut przez orkiestrator (EC2) | brak [O]; kontrakt T0001 [I]/[A] tylko matematyczny |
| **WG-PROJ** — parametry rzutu | M: kadr jak WG-ELEV, z pionowymi narożnikami budynków w co najmniej trzech ćwiartkach; dwa zrzuty w spoczynku | `c`, izotropia, stabilność (pkt 2–4, 6); sprawdzenia spójności krawędzi ścian | `originalGta2`, orkiestrator (EC2) | brak |
| **WG-WALL-SEL** — wybór ścian | M: bloki o różnych kaflach na `left`/`right`/`top`/`bottom`, położone w czterech ćwiartkach względem `c`; dodatkowo blok, którego jedną ścianę zasłania sąsiad | **tylko widoczne piksele**: które kafle ścian są widoczne w każdej ćwiartce. Czy zasłonięta ściana jest wysyłana do rysowania — nieobserwowalne bez osobnego śladu wywołań (brak źródła) | `originalGta2`, orkiestrator (EC2) | brak |
| **WG-WALL-BITS** — flagi kolizji a widoczność | M: miejsca (zapytanie do danych) ze ścianą boczną o zwykłym kaflu przy `wall` = 0, `wall` = 1, `bullet_wall` = 1, z kaflem 0 i flagami, oraz słowo 0, każda zwrócona ku otwartej komórce; S, jeśli któraś kombinacja nie występuje | widoczne piksele każdej z tych ścian; przechodzenie pieszego to osobny test kolizji | `originalGta2`, orkiestrator (EC2) | brak; występowanie kombinacji w miastach niezmierzone |
| **WG-FLAT** — flagi `flat` | M: ściany boczne i pokrywy z `flat` = 1 (zapytanie do danych), najlepiej z kaflem o przezroczystych pikselach; S, jeśli brak | położenie, przezroczystość, widoczność z obu stron | znaczenie z dokumentu [F] (do przepisania do `gmp.md`) + `originalGta2`, orkiestrator (EC2) | brak; dokument niedostępny w tej sesji |
| **WG-ROT-LID** — obrót pokrywy | M: pokrywy z asymetrycznym kaflem (np. napis) w kodach R0–R270 bez odbicia; ten sam blok renderowany przez oba środowiska | dla każdej pokrywy: który narożnik obrazka kafla (A–D) jest w którym narożniku na ekranie (tabela 5.1) | `originalGta2`, orkiestrator (EC2) | tylko [W] (nasz render Downtown), brak [O] |
| **WG-FLIP-ORDER** — kolejność odbicia | jak WG-ROT-LID, z odbiciem: flip + R0 … flip + R270 | jw.; rozróżnia „odbicie przed/po obrocie”, kierunek obrotu i oś odbicia (sekcja 5.2) | `originalGta2`, orkiestrator (EC2) | brak |
| **WG-ROT-WALL** — obrót/odbicie ścian | M (S zapasowo): ściany boczne z asymetrycznym kaflem, 8 kombinacji na każdą z czterech stron, widoczne według WG-WALL-SEL | etykiety A–D w narożnikach widocznej ściany; czy `left`/`right` są lustrzane (sekcja 3) | `originalGta2`, orkiestrator (EC2) (po WG-WALL-SEL i WG-PROJ) | brak |
| **WG-RESERVED** — kafle spoza stylu | M na mapie `mike1*` (rekordy z kaflami 992–1022 według [D]) oraz pokrywy 1023 bez skosu; S dla kafli ≥ `tile_count` | co widać w miejscu tych ścian/pokryw (nic, animacja, inny kafel) | `originalGta2`, orkiestrator (EC2) | brak |

## 8. Zadanie T0005 (mapowanie tekstury ścian) — zakończone

**Status: zakończone.** T0005 zatwierdzone 2026-10-13, commit `e41b6ae`:
`scene::face_texture_corners(BlockFace, assets::Rotation, bool flip)` w
`engine/scene/include/gta20/scene/face_texture.hpp` (`engine/scene/src/face_texture.cpp`), test
syntetyczny `[world-geometry]` w `tests/unit/scene_tests.cpp` z 40 przypadkami z sekcji 5.4 i 8
regresjami względem obecnego rysowania pokryw. Zachowanie w czasie działania bez zmian; nic jeszcze
nie rysuje z tą funkcją. Status merytoryczny bez zmian: dla pokrywy [I] (regresja obecnego silnika),
dla ścian [H]; to nie jest dowód zgodności z oryginałem.

Pierwotna rekomendacja (zachowana jako opis zakresu T0005):

**Zadanie:** dodać w `engine/scene` czysto CPU-ową funkcję mapowania tekstury ściany, np.
`face_texture_corners(BlockFace, Rotation, bool flip) → std::array<TexCorner, 4>` (etykieta A–D
dla p0..p3 geometrii T0001), z testem syntetycznym `[world-geometry]` obejmującym **40**
przypadków z sekcji 5.4. Dla pokrywy oczekiwania mają odtwarzać obecne zachowanie
`append_map_textured` (tabela 5.1) — to test regresji [I], nie dowód zgodności; dla ścian kodują
propozycję [H] z sekcji 3 i 5.3, z komentarzem „hypothesis, see docs/specs/world-geometry.md”.
Bez zmian w rendererze, kamerze, shaderach i zachowaniu w czasie działania.

Dlaczego to: jest niezależne od nierozstrzygniętej projekcji i wysokości, przygotowuje dane do
ścian i od razu ustala nazwy/etykiety, którymi posłużą się testy WG-ROT-*.

**Nierozstrzygnięte zależności** (żadna nie blokowała T0005, wszystkie nadal blokują renderowanie ścian):

1. Kierunek obrotu, oś i kolejność odbicia w oryginale (WG-ROT-LID, WG-FLIP-ORDER) — może zmienić
   oczekiwania dla pokrywy, a więc i obecny render.
2. Orientacja u na ścianach bocznych (lustrzana czy wzdłuż +x/+y) — WG-ROT-WALL.
3. Parametry rzutu `K`, `r`, `c` — WG-ELEV, WG-PROJ. Proporcja `h / w` i „bezwzględna” wysokość
   pokrywy poziomu GMP z zostają otwarte (sekcja 6.2); renderer nie powinien od nich zależeć.
4. Widoczność ścian a bity `wall`/`bullet_wall`/`flat`, kafel 0 z flagami — WG-WALL-BITS, WG-FLAT;
   najpierw przepisać do `gmp.md` znaczenie `flat` z dokumentu DMA (niedostępnego w tej sesji).
5. Uruchomienie oryginału. `gta2.exe` to program Windows (PE;
   `docs/reports/2026-10-12-original-assets.md`, sekcja 1) z własnymi DLL-ami renderera (sekcja 4
   raportu). Według obecnej polityki właściciela oryginał uruchamia **tylko orkiestrator** na hoście
   EC2, na prośbę agenta (`gfx_requests`) i w limitach właściciela (sekcja 7); agenci nie
   uruchamiają AWS ani hosta. Otwarte: czy pierwszy przebieg (`m1-downtown-baseline`) dochodzi do
   świata, doprowadzenie pieszego do miejsc z trybu M samymi klawiszami i odczyt jego pozycji.

## 9. Zadanie T0013 (składanie ścian w siatkę CPU) — kontrakt syntetyczny

**Status: [I] + [A], kontrakt syntetyczny; nie jest dowodem zachowania oryginału.**
`scene::WorldMesh` (`engine/scene/include/gta20/scene/world_mesh.hpp`,
`engine/scene/src/world_mesh.cpp`) to indeksowana siatka trójkątów bez GPU: wierzchołek
`WorldVertex` = pozycja (x, y, z) w jednostkach bloków + UV atlasu (v w dół atlasu), indeksy `u32`.

* `WorldMesh::append_face(BlockFace, Position3f origin, FaceTile{tile, rotation, flip}, tile_count)`
  dodaje **dokładnie jedną, wskazaną przez wywołującego** ścianę: cztery wierzchołki w kolejności
  p0..p3 z `block_face_geometry` (sekcja 3, T0001), etykiety A–D z `face_texture_corners` (sekcja 5,
  T0005) i prostokąt slotu z `TileAtlas::slot_uv` (wcięcie o pół teksela), oraz sześć indeksów
  `{0, 1, 2, 0, 2, 3}` przesuniętych o liczbę wierzchołków sprzed wywołania.
* Rysowalny jest tylko kafel `1 <= tile < tile_count` i `tile < 992` (`scene::is_drawable_tile`).
  Odrzucane są: 0 (pusty), `>= tile_count` (brak w stylu), 992–1022 (zarezerwowane), 1023 (znacznik
  mapy dla trzystronnych skosów — ten sam numer co biały slot atlasu `TileAtlas::kWhiteSlot`, ale
  siatka nigdy nie odwołuje się do białego slotu), nieznana ściana i pozycje nieskończone/NaN. Błąd
  wraca jako `Result` i nie zmienia siatki.
* Siatka **nie** wybiera ścian z mapy, nie ustala wysokości pokryw względem poziomów GMP, orientacji
  tekstur ścian (nadal [H], sekcja 5.3) ani widoczności ścian (sekcje 4 i 6). Nic jej jeszcze nie
  rysuje; `DrawList` i `append_map_textured` są bez zmian.
* Testy `WorldMesh:` w `tests/unit/scene_tests.cpp`: 40 kombinacji ściana × obrót × odbicie z
  oczekiwaniami spisanymi ręcznie (narożniki, tabela etykiet z sekcji 5.4, współrzędne slotów — bez
  wywoływania funkcji pomocniczych), kilka ścian w różnych miejscach (w tym 255,255,7) z kontrolą
  indeksów, nawinięcia na zewnątrz, skończoności, zakresu UV i niezmienności wcześniejszej zawartości,
  odrzucanie kafli oraz paczka 16 400 ścian (65 600 wierzchołków, indeksy > 65535).
