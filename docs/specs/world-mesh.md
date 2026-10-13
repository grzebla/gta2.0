# Siatka mapy bez skosów (T0014): kontrakt częściowego budowania

**Status: [I] + [A], kontrakt lokalny i tymczasowy; nie jest dowodem zachowania oryginału.**
Znaczniki statusów jak w [world-geometry.md](world-geometry.md), sekcja 0. Stan 2026-10-13.

`scene::build_unsloped_map_mesh(const assets::GmpMap&, u16 tile_count)`
(`engine/scene/include/gta20/scene/world_mesh_map.hpp`, `engine/scene/src/world_mesh_map.cpp`)
łączy sparsowaną mapę z indeksowaną siatką CPU `scene::WorldMesh` (T0013,
[world-geometry.md](world-geometry.md), sekcja 9). Zwraca `Result<MapMesh>`: siatkę i ograniczone
(stałego rozmiaru) statystyki `MapMeshStats`. Nie zależy od kamery, nie ma pamięci podręcznej i nie
jest nigdzie rysowana: renderer aplikacji, `DrawList`, `append_map_textured` i ścisły werdykt
inwentarza (`gta20-headless inventory`, `engine/assets/include/gta20/assets/map_inventory.hpp`) są
bez zmian.

## 1. Kolejność i położenie

* Odwiedzane są **wszystkie** 256 × 256 × 8 komórek: z = 0..7, w nim y = 0..255, w nim
  x = 0..255; ściany jednej komórki w kolejności `BlockFace` (lid, left, right, top, bottom).
  Te same dane wejściowe dają te same ciągi wierzchołków i indeksów oraz te same statystyki; wynik
  zależy tylko od zawartości komórek, nie od kolejności tablicy unikalnych bloków. [A]
* Komórka (x, y, z) to początek bloku (x, y, z) w kontrakcie T0001
  (`block_geometry.hpp`): blok zajmuje [x, x+1] × [y, y+1] × [z, z+1], pokrywa leży na wysokości
  z + 1, a cała siatka mieści się w x, y ∈ [0, 256], z ∈ [0, 8]. **To bieżąca konwencja lokalna
  ([I]/[H])**, nie pomiar: przeliczenie „poziom GMP → wysokość świata” pozostaje otwarte do
  dowodów z oryginału (WG-ELEV, [world-geometry.md](world-geometry.md), sekcje 3 i 6.2).

## 2. Tymczasowa reguła wyboru ścian — [H]

Dla każdej niepustej komórki (`BlockInfo::empty()` = fałsz):

| Kolejność | Warunek | Skutek | Licznik |
|---|---|---|---|
| 1 | `slope() != 0` | cały blok pominięty (żadna ściana, także pokrywa, nie jest spłaszczana) | `sloped_blocks`, `sloped_blocks_by_code[slope()]` (0..63) |
| 2 | słowo ściany ma bit `flat` (dowolny kafel, także 0 i ≥ 992) | ściana pominięta | `flat_faces`, `flat_by_face[BlockFace]` |
| 3 | kafel 0 | nic do narysowania | `blank_faces` |
| 4 | kafel 992..1023 (zarezerwowane; 1023 to znacznik mapy) | pominięta; **nie** jest rysowana białym slotem atlasu | `reserved_faces` |
| 5 | 1 ≤ kafel < 992 i kafel ≥ `tile_count` | pominięta (brak w stylu) | `out_of_style_faces` |
| 6 | pozostałe (`is_drawable_tile`) | ściana dodana przez `WorldMesh::append_face` | `emitted_faces`, `emitted_by_face[BlockFace]` |

Dla każdego bloku bez skosu każda z pięciu ścian trafia do dokładnie jednego z wierszy 2–6, więc
`5 · unsloped_blocks = emitted + flat + blank + reserved + out_of_style`, a
`empty_cells + sloped_blocks + unsloped_blocks = cells_visited = 524 288`.

Reguła jest **tymczasowa**:

* Bity `wall` i `bullet_wall` **nie** są bramką widoczności (sekcja 4 world-geometry.md: z flag
  kolizji nie wolno wnioskować o widoczności). Bit oświetlenia pokrywy też jest ignorowany.
* **Brak eliminacji ścian sąsiednich**: ściana między dwoma pełnymi blokami, pokrywa przykryta
  blokiem wyżej i dwie ściany w tej samej płaszczyźnie są dodawane. Czy oryginał je pomija, jest
  nierozstrzygnięte (przypadek 12 tabeli decyzji, WG-WALL-SEL).
* Orientacja tekstur ścian pochodzi z `face_texture_corners` (T0005) i pozostaje [H]; dla pokryw
  to regresja obecnego silnika [I].

## 3. Pokrycie: nigdy „kompletne M1”

`MapMeshStats::coverage()` zwraca:

* `MapMeshCoverage::partial`, gdy pominięto treść nieobsługiwaną: blok ze skosem, ścianę `flat`,
  kafel zarezerwowany lub spoza stylu (`has_unsupported()`). Siatka **na pewno** nie zawiera
  geometrii opisanej w mapie.
* `MapMeshCoverage::provisional` w przeciwnym razie (pominięto co najwyżej puste komórki i kafle 0).
  Nawet wtedy wynik nie jest geometrią M1: reguła wyboru, wysokość i orientacja ścian są
  tymczasowe.

Wartości „kompletne” nie ma. Brak obsługi skosów i ścian `flat` nigdy nie może być zgłoszony jako
pełna geometria M1. Pełna walidacja geometrii 17 map (T0010 sprawdza tylko instalację i
skojarzenia map ze stylami) następuje dopiero po obsłudze skosów.

## 4. Błędy

Kafel jest klasyfikowany przed `append_face`, więc dla sparsowanej mapy budowanie nie zawodzi;
gdyby `append_face` zwróciło błąd, wraca on jako `Result` z położeniem komórki.

## 5. Testy

`tests/unit/scene_tests.cpp`, przypadki `WorldMeshMap:` na **syntetycznych** mapach z
`GmpWriter` (oczekiwania z tego kontraktu, nie z oryginału): pusta mapa, jeden blok z pokrywą i
czterema ścianami (obrót/odbicie, dokładne współrzędne i UV), stos bloków z przerwą między poziomami
i stykający się sąsiedzi, narożniki mapy na poziomach 0 i 7 (zakres 0..256 × 0..256 × 0..8),
niezależność od bitów kolizji (siatki identyczne z bitami i bez), ściany `flat`, skosy według kodu
(w tym 1023 na skosie 49), kafle puste/zarezerwowane/spoza stylu, `tile_count = 0` oraz powtarzalność
(dwa budowania i mapa zapisana w odwrotnej kolejności). Każdy przypadek sprawdza dokładne liczby,
sumy kontrolne liczników, skończoność wierzchołków, zakres indeksów i to, że UV każdej ściany są
narożnikami jednego rysowalnego slotu atlasu (nigdy slotu 1023).

`tests/unit/gamedata_tests.cpp`, `[gamedata] WorldMeshMap: original cities are reported as partial
coverage`: dla `wil`, `ste`, `bil` sprawdza **tylko** werdykt `partial`, `sloped_blocks > 0`, brak
kafli spoza stylu, skończoność i zakres indeksów. Nie waliduje geometrii. Jednorazowy pomiar [D]
(2026-10-13, dane 9.6): wil — 103 454 ściany dodane, 17 977 bloków ze skosem, 10 642 ściany
`flat`, 0 zarezerwowanych; ste — 112 838 / 14 911 / 21 582 / 0; bil — 109 187 / 24 003 /
23 380 / 0.

## 6. Następne kroki (poza zakresem T0014)

Obsługa skosów i znaczenia `flat` (najpierw przepisać znaczenie bitu z dokumentu DMA), potem pełna
walidacja geometrii 17 map, integracja z rendererem i kamerą. Każdy z tych kroków wymaga dowodów z
oryginału opisanych w [world-geometry.md](world-geometry.md), sekcje 6–7.
