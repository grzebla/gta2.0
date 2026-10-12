# Geometria świata bez skosów: konwencje i luki w dowodach

Kontrakt dla następnej sesji implementującej ściany i wysokość bloków. Oddziela fakty od
założeń o zachowaniu oryginalnego GTA 2. Zakres: bloki **bez skosów** (`slope() == 0`), pięć
ścian bloku, flagi słów ścian, obrót/odbicie tekstur, projekcja i wysokość. Skosy, oświetlenie
pokryw, animacje kafli i kolizje są poza zakresem (wymienione tylko tam, gdzie dotykają tabeli
decyzji). Stan na rewizji `cb06317` (gałąź `agent/t0002`), 2026-10-12.

## 0. Statusy

Każde stwierdzenie o zachowaniu ma lokalizację źródła i jeden z czterech statusów
(zgodnie z [../reference/gta2re.md](../reference/gta2re.md), pkt 4):

| Znacznik | Status | Znaczenie |
|---|---|---|
| **[F]** | specyfikacja formatu | Dokument DMA Design w transkrypcji z `docs/formats/*.md`. Mówi, co zapisano w pliku, nie jak oryginał to rysuje. |
| **[A]** | istniejący dowód automatyczny | Asercja w teście CTest w repozytorium; zakres dokładnie taki, jak w ciele testu. |
| **[O]** | pomiar na oryginalnej grze | Zrzut lub ślad `provenance: originalGta2` z uruchomionego oryginału. **Obecnie brak jakiegokolwiek** (sekcja 1). |
| **[H]** | hipoteza | Wszystko inne, także propozycje konwencji w tym dokumencie. |

Dwa dodatkowe oznaczenia, które **nie** są dowodem zachowania oryginału:

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
| Dekodowanie bitów | `tests/unit/assets_tests.cpp:41-58` | [A] | `SideFace`/`LidFace::decode` wyciągają bity zgodnie z [F]. Test syntetyczny — nie mówi nic o znaczeniu flag. |
| Kontrakt T0001 | `engine/scene/include/gta20/scene/block_geometry.hpp:9-25`, `engine/scene/src/block_geometry.cpp:9-15` | [I] | położenie, normalne i kolejność narożników pięciu ścian bloku jednostkowego. |
| Testy T0001 | `tests/unit/scene_tests.cpp:135-255` | [A] | kod spełnia kontrakt T0001 (płaszczyzny, nawinięcie, translacja, wspólne krawędzie). Oczekiwania są **syntetyczne** (`scene_tests.cpp:39-40`), wyprowadzone z kontraktu, nie z oryginału. |
| Osie mapy | `tests/unit/gamedata_tests.cpp:141-174` | [A] | tylko `wil.gmp`, wszystkie poziomy z: ściana z bitem `wall` i kaflem ≠ 0 na `left`/`right`/`top`/`bottom` ma pustą komórkę w kierunku `−x`/`+x`/`−y`/`+y` w > 75 % przypadków, a przy zamienionych osiach w < 35 %. Liczby 84–86 % / ~20 % (`gmp.md:24-25`, komentarz `gamedata_tests.cpp:143`) to [D]. |
| Kafle zarezerwowane | `tests/unit/gamedata_tests.cpp:96-111`, `:214-233` | [A] | w `wil`/`ste`/`bil` żadna ściana boczna nie ma kafla ≥ 992 (`:101`); każdy kafel ścian i pokryw jest < `tile_count` stylu (= 992) albo = 1023 (`:227`); ≥ 95 % pokryw 1023 ma skos 49–52 (`:110`). Mapy `mike1*` i pozostałe nie są tym objęte. |
| 98,8 % / 992–1022 tylko w `mike1*` | `docs/formats/gmp.md:28-30` | [D] | brak testu, który to powtarza. |
| CMAP | `tests/unit/gamedata_tests.cpp:121-139` | [A] | tylko `wil.gmp` i `mike1m.gmp`: oba chunki się dekodują, a DMAP ma **więcej unikalnych bloków** niż CMAP. Test **nie** porównuje komórek; „inna, starsza wersja miasta” i „inne pokrywy” (`gmp.md:17`, `:27`) to [D]. |
| Układ kolumn DMAP 100 % | `docs/formats/gmp.md:16`, `:23` | [D] | parser przechodzi na 17 mapach (`gamedata_tests.cpp:77-90`), ale przyleganie kolumn nie jest asercją. |
| Orientacja pokryw, obrót, odbicie | `docs/formats/gmp.md:31-32`, `docs/reports/2026-10-12-original-assets.md` (sekcja 6–7) | [W] | zrzuty **naszego** silnika (lokalnie `~/Games/GTA2-work/review/*.png`, opisane w lokalnym `REPORT.md`): pasy ciągłe, napisy „SLOW” nieodbite. To nie są zrzuty oryginału ani porównanie automatyczne. |
| Zrzuty/ślady oryginału | `docs/aws/integration.md:11-18` | — | **brak**: na AWS nic nie wdrożono, `gfx-compat`/`gfx-e2e` nie zostały uruchomione. Zero dowodów [O] dla geometrii. |
| GTA2_RE | `docs/mechanics-matrix.md:17`, `:19` | — | tylko nazwy funkcji jako wskazówka, gdzie szukać (`DrawGradientSlope_4F6630`, `DrawDiagonalWall_4F02D0`, `Draw_4F6A20`, `Camera.cpp`). W tej sesji GTA2_RE nie był czytany (brak lokalnej kopii); nic tutaj nie pochodzi z jego kodu. |

## 2. Układ współrzędnych

| Stwierdzenie | Źródło | Status |
|---|---|---|
| Mapa ma 256 × 256 × 8 komórek (x, y, z). | `docs/formats/gmp.md:15`, `:18`; `gmp_map.hpp:16-18` | [F] |
| Indeks kolumny DMAP to `y·256 + x`; x rośnie wraz z kolumną mapy, y z wierszem. | `gmp.md:24`; pośrednio `gamedata_tests.cpp:141-174` | [F] + [A] (tylko kierunki ścian na `wil.gmp`) |
| Ściany `left`/`right`/`top`/`bottom` graniczą z sąsiadem w kierunku `−x`/`+x`/`−y`/`+y`. | `gamedata_tests.cpp:141-174` | [A] (statystycznie, `wil.gmp`) |
| `top`/`bottom` to **ściany pionowe** (granica z poprzednim/następnym wierszem), nie poziome powierzchnie. | `block_geometry.hpp:18-19` | [I]; wynika z [A] powyżej |
| Na ekranie +y mapy idzie w dół, +x w prawo. | `camera2d.hpp:14-15`, `camera2d.cpp:6-9` | [I] dla naszego silnika; [W] że oryginał wygląda tak samo (pasy, napisy) |
| z rośnie w górę; poziom z = 0 jest najniższy. | `block_geometry.hpp:11-13`; kolejność rysowania `draw_list.cpp:94` | [I]; dla oryginału [H] — kierunek osi z nie ma w repo ani dowodu [A], ani [O]. Kolejność „niższe najpierw” to [W]. |
| Gracz porusza się w `z ∈ 1..6`. | `gmp.md:46-47` | [F] — nie mówi, na jakiej wysokości świata leży pokrywa poziomu z. |

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
| 8 | Kafel 1 … `tile_count`−1 (zwykły) | kafel istnieje w stylu: [A] dla `wil`/`ste`/`bil` (`gamedata_tests.cpp:214-233`) | pokrywa rysowana [I]; ściany nie | pokrywa: [W] (tylko nasz render); ściany: **NIEROZSTRZYGNIĘTE** | WG-ELEV, WG-WALL-SEL |
| 9 | Kafel 992–1022 | zarezerwowane [F] (`sty.md:20`); brak w miastach [A] (`gamedata_tests.cpp:101`, `:227`); tylko w `mike1*` [D] (`gmp.md:29-30`); cel „animacje silnika” — bez źródła w repo, [H] | pominięty (≥ `tile_count`) [I] (`draw_list.cpp:101`) | **NIEROZSTRZYGNIĘTE** (chunk `ANIM` niezaimplementowany, `gmp.md:20`) | WG-RESERVED |
| 10 | Kafel 1023 (znacznik) | znacznik trójbocznych skosów: [A] ≥ 95 % na skosach 49–52 (`gamedata_tests.cpp:104-111`); 98,8 % [D]; na ścianach bocznych miast nie występuje [A] (`:101`) | pominięty [I] (`draw_list.cpp:99-101`); slot 1023 atlasu to biały kafel pomocniczy (`draw_list.hpp:30`) — **nie** tekstura 1023 | **NIEROZSTRZYGNIĘTE** (skosy poza zakresem) | WG-RESERVED (po skosach) |
| 11 | Kafel ≥ `tile_count` stylu, < 992 | dla miast nie występuje [A] (`:227`); `fstyle.sty` ma 0 kafli [A] (`gamedata_tests.cpp:189`); parowanie map bonusowych ze stylem nieustalone | pominięty [I] | **NIEROZSTRZYGNIĘTE** | WG-RESERVED |
| 12 | Ściana między dwoma pełnymi blokami (zasłonięta) | — | nie dotyczy (brak ścian) | **NIEROZSTRZYGNIĘTE**, czy oryginał ją pomija, czy rysuje i zasłania | WG-WALL-SEL |

## 5. Obrót i odbicie

### 5.1 Co robi dziś nasz silnik (tylko pokrywy) — [I]

Źródło: `draw_list.cpp:104-112` i `quad.vert.glsl:17-24` (odpowiedniki: `quad.hlsl:32-38`,
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
  `draw_list.cpp:111` powołuje się na „docs/formats/sty.md”, ale `sty.md` nie zawiera żadnej
  informacji o obrocie — ta referencja jest niedostępna/martwa; status pozostaje [H].
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

Bez zgadywania stałych. Każdy pomiar: oryginał (wersja 9.6 lub 10.5 — identyczne dane, `gmp.md:22`),
stała rozdzielczość (zapisana), pieszy stojący (bez oddalenia kamery przy jeździe), znana pozycja.

1. **Skala na poziom**: rozmiar w pikselach boku pokrywy na co najmniej trzech różnych poziomach z
   w tym samym kadrze. Stosunek skal wyznacza wysokość kamery w jednostkach poziomu (model
   `s(z) = f / (H − z)` jest [H]; trzy poziomy go weryfikują, a nie zakładają).
2. **Środek perspektywy**: przesunięcie na ekranie narożnika wysokiej pokrywy względem tego samego
   (x, y) na poziomie gruntu, dla bloków w różnych ćwiartkach ekranu → czy środek rzutu to środek
   ekranu, czy jest przesunięty.
3. **Proporcja wysokości**: wysokość widocznej ściany w pikselach vs różnica położeń dolnej i górnej
   krawędzi → czy wysokość poziomu = szerokość bloku.
4. **Poziom gruntu i pokrywy**: na jakim z GMP stoi pieszy w znanym miejscu i czy jego sprite pokrywa
   się z pokrywą poziomu z, czy z + 1 → przeliczenie „poziom GMP → wysokość świata”.
5. **Wybór ścian**: które z czterech ścian bloku są widoczne w każdej ćwiartce ekranu i czy ściany
   zasłonięte przez sąsiada są rysowane.
6. **Zależność od ruchu kamery**: jak skala z pkt 1 zmienia się przy jeździe (wysokość kamery
   zmienna) — tylko by wiedzieć, że pomiary 1–5 trzeba robić w spoczynku.
7. **Pole widzenia**: ile bloków gruntu mieści się w poziomie i w pionie przy danej rozdzielczości
   (i czy zależy od proporcji ekranu).
8. **Bloki ponad kamerą**: czy wysokie budynki blisko środka są przycinane (płaszczyzna bliska).

## 7. Przyszłe scenariusze porównawcze

Wszystkie: strona silnika = mapa **syntetyczna** z `GmpWriter` (`gmp_writer.hpp:11-27`), oznaczona
`provenance: synthetic`, id `synthetic-*`; strona referencyjna = zrzuty oryginału z EC2
(`provenance: originalGta2`, `docs/testing.md:26`, `:69`), trzymane poza repo (prywatny bucket);
do repo trafiają tylko współrzędne, numery kafli, parametry kamery i wyniki porównań. Tam, gdzie
nie da się uruchomić oryginału na mapie syntetycznej, referencja to miejsce w oryginalnej mapie
(`wil.gmp`) wskazane współrzędnymi. **Żaden scenariusz nie ma dziś dowodu [O]** (sekcja 1).

| Nazwa | Ustawienie | Obserwowany wynik | Pochodzenie referencji | Dowód dziś |
|---|---|---|---|---|
| **WG-ELEV** — wysokość pokryw | kolumna bloków z pokrywami na z = 1, 2, 3, 5 przy płaskim gruncie; pieszy stoi obok; stała rozdzielczość | rozmiar i przesunięcie pokryw w pikselach względem środka ekranu; poziom, na którym stoi pieszy | `originalGta2` (zrzut EC2), miejsce w `wil.gmp` z budynkiem ≥ 3 poziomów | brak [O]; kontrakt T0001 [I]/[A] tylko matematyczny |
| **WG-PROJ** — parametry rzutu | ten sam kadr co WG-ELEV, bloki w czterech ćwiartkach i w środku ekranu; pomiary 1–3, 7, 8 z sekcji 6.2 | skala na poziom, środek perspektywy, proporcja wysokości, pole widzenia | `originalGta2` | brak |
| **WG-WALL-SEL** — wybór ścian | wolnostojący blok 1 × 1 × 3 z różnymi kaflami na `left`/`right`/`top`/`bottom`; kamera kolejno w czterech ćwiartkach; drugi wariant z sąsiadem zasłaniającym jedną ścianę | które kafle ścian są widoczne w każdej pozycji; czy zasłonięta ściana jest rysowana | `originalGta2` | brak |
| **WG-WALL-BITS** — flagi kolizji a widoczność | rząd bloków: zwykły kafel z `wall` = 0, `wall` = 1, `bullet_wall` = 1, kafel 0 z `wall` = 1, słowo 0 | czy każda ściana jest widoczna (piksele) — osobno od tego, czy pieszy przechodzi | `originalGta2`; wcześniej [D]-pomiar występowania tych kombinacji w miastach | brak |
| **WG-FLAT** — flagi `flat` | ściana boczna z `flat` = 1 na każdej z czterech stron i pokrywa z `flat` = 1; kafel z przezroczystymi pikselami | położenie, przezroczystość, widoczność z obu stron | znaczenie z dokumentu [F] (do przepisania do `gmp.md`) + `originalGta2` | brak; dokument niedostępny w tej sesji |
| **WG-ROT-LID** — obrót pokrywy | cztery pokrywy z tym samym asymetrycznym kaflem (napis), kody R0–R270, bez odbicia | etykieta A–D w każdym narożniku na ekranie (tabela 5.1) | `originalGta2` | tylko [W] (nasz render Downtown) |
| **WG-FLIP-ORDER** — kolejność odbicia | jak WG-ROT-LID, z odbiciem: flip + R0 … flip + R270 | jw.; rozróżnia „odbicie przed/po obrocie” i oś odbicia (sekcja 5.2) | `originalGta2` | brak |
| **WG-ROT-WALL** — obrót/odbicie ścian | blok z asymetrycznym kaflem na każdej ścianie bocznej, 8 kombinacji na ścianę (32 bloki) | etykiety A–D w narożnikach widzianych z zewnątrz; czy `left`/`right` są lustrzane (sekcja 3) | `originalGta2` (wymaga najpierw WG-WALL-SEL i WG-PROJ) | brak |
| **WG-RESERVED** — kafle spoza stylu | pokrywy i ściany z kaflem 992, 1022, 1023 (bez skosu) i ≥ `tile_count` | co rysuje oryginał (nic, animacja, błąd) | `originalGta2` na mapie `mike1*` (kafle 992–1022 według [D]) | brak |

## 8. Rekomendacja na następne zadanie (1–2 h)

**Zadanie:** dodać w `engine/scene` czysto CPU-ową funkcję mapowania tekstury ściany, np.
`face_texture_corners(BlockFace, Rotation, bool flip) → std::array<TexCorner, 4>` (etykieta A–D
dla p0..p3 geometrii T0001), z testem syntetycznym `[world-geometry]` obejmującym **40**
przypadków z sekcji 5.4. Dla pokrywy oczekiwania mają odtwarzać obecne zachowanie
`append_map_textured` (tabela 5.1) — to test regresji [I], nie dowód zgodności; dla ścian kodują
propozycję [H] z sekcji 3 i 5.3, z komentarzem „hypothesis, see docs/specs/world-geometry.md”.
Bez zmian w rendererze, kamerze, shaderach i zachowaniu w czasie działania.

Dlaczego to: jest niezależne od nierozstrzygniętej projekcji i wysokości, przygotowuje dane do
ścian i od razu ustala nazwy/etykiety, którymi posłużą się testy WG-ROT-*.

**Nierozstrzygnięte zależności** (żadna nie blokuje tego zadania, wszystkie blokują renderowanie ścian):

1. Kierunek obrotu, oś i kolejność odbicia w oryginale (WG-ROT-LID, WG-FLIP-ORDER) — może zmienić
   oczekiwania dla pokrywy, a więc i obecny render.
2. Orientacja u na ścianach bocznych (lustrzana czy wzdłuż +x/+y) — WG-ROT-WALL.
3. Przeliczenie poziomu GMP na wysokość świata i parametry rzutu — WG-ELEV, WG-PROJ.
4. Widoczność ścian a bity `wall`/`bullet_wall`/`flat`, kafel 0 z flagami — WG-WALL-BITS, WG-FLAT;
   najpierw przepisać do `gmp.md` znaczenie `flat` z dokumentu DMA (niedostępnego w tej sesji).
5. Pierwsze zrzuty oryginału wymagają EC2 (`docs/aws/integration.md`) — decyzja i zgoda
   właściciela na koszty; agent nie może tego uruchomić.
