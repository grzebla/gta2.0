# Mapa GTA 2 (`.gmp`)

Źródło: dokument DMA Design „GTA2 map format”, wersja 8.2 z 18.08.1999, opublikowany na
[gtamp.com](https://gtamp.com/gta2/gta2-gmp-map-file-format/). Implementacja:
`engine/assets/src/gmp_map.cpp` (parser), `gmp_writer.cpp` (zapis DMAP dla danych syntetycznych).

| Element | Specyfikacja | Implementacja | Weryfikacja |
|---|---|---|---|
| Nagłówek | `"GBMP"`, `u16` wersja 500 | sprawdzane | testy syntetyczne |
| Chunki | `char[4]` + `u32` rozmiar, dowolna kolejność, nieznane pomijane | tak | testy syntetyczne |
| `block_info` | 12 B: `u16` left/right/top/bottom/lid, `u8` arrows, `u8` slope_type | tak | testy syntetyczne |
| Słowo ściany | bity 0–9 kafel, 10 wall, 11 bullet wall, 12 flat, 13 flip, 14–15 obrót | `SideFace::decode` | test bitów |
| Słowo pokrywy | 0–9 kafel, 10–11 oświetlenie, 12 flat, 13 flip, 14–15 obrót | `LidFace::decode` | test bitów |
| `slope_type` | bity 0–1 typ podłoża (air/road/pavement/field), 2–7 skos (0–63) | tak | test bitów |
| DMAP | `u32 base[256][256]`, `u32 column_words`, kolumny, `u32 num_blocks`, bloki | tak | round-trip syntetyczny |
| Kolumna DMAP | `u8 height`, `u8 offset`, `u16 pad`, numery bloków | `height − offset` wpisów od `z = offset` | **potwierdzone**: na 17 oryginalnych mapach kolejne kolumny przylegają w 100 % przy tej interpretacji (przy `height` wpisów: 0–30 %) |
| CMAP | jak DMAP, 16-bit, bez `pad` | tak | parsuje się na oryginałach, ale zawiera **inną (starszą) wersję miasta** niż DMAP — silnik używa DMAP |
| UMAP | `block_info[8][256][256]` | tak, kolejność z, y, x | brak w oryginalnych mapach PC |
| ZONE | `u8` typ, x, y, w, h, długość nazwy, nazwa bez NUL | tak | round-trip syntetyczny |
| MOBJ, PSXM, ANIM, LGHT, RGEN | opisane w dokumencie | nie (pomijane) | — |

**Zweryfikowane na oryginalnych plikach (9.6 = 10.5, 2026-10-12)** — testy `[gamedata]`:
1. Kolumna DMAP ma `height − offset` wpisów (100 % przylegania kolumn na 17 mapach).
2. Indeks kolumny to `y·256 + x`; ściana `left`/`right`/`top`/`bottom` graniczy z `−x`/`+x`/`−y`/`+y`
   (84–86 % ścian ma pustą komórkę po tej stronie, przy zamienionych osiach ~20 %).
3. Każda mapa ma chunki `CMAP` i `DMAP` (+ `ZONE`, `PSXM`, `LGHT`, `EDIT` (nieopisany), `RGEN`,
   opcjonalnie `ANIM`, `MOBJ`). CMAP opisuje inną wersję miasta — używamy DMAP.
4. Kafel pokrywy 1023 to znacznik trójbocznych skosów diagonalnych: 98,8 % takich pokryw w trzech
   miastach ma skos 49–52 (wyjątki: pojedyncze bloki ze skosem 0, 45–47). Kafle 992–1022 występują
   tylko w poziomach bonusowych `mike1*` (zarezerwowane dla animacji silnika).
5. Kolejność rysowania i obrót/odbicie pokryw zostały zweryfikowane wizualnie na Downtown:
   linie pasów i łuki są ciągłe między kaflami, napisy nie są odbite.

Oświetlenie pokrywy (bity 10–11): dokument mówi tylko „0 normalnie, 1–3 coraz ciemniej”.
Renderer na razie go **nie stosuje** — zgadywane współczynniki wyraźnie zniekształcały dachy;
wartości trzeba zmierzyć na oryginale.

Granica świata według dokumentu: gracz porusza się w `x, y ∈ 1..254`, `z ∈ 1..6`.
Uwzględnia to `sim::World::cell_passable`.
