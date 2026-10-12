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
| Kolumna DMAP | `u8 height`, `u8 offset`, `u16 pad`, numery bloków | czytamy `height − offset` wpisów od `z = offset` | **interpretacja — do potwierdzenia** |
| CMAP | jak DMAP, 16-bit, bez `pad` | tak | brak testu na danych |
| UMAP | `block_info[8][256][256]` | tak, kolejność z, y, x | **kolejność osi do potwierdzenia** |
| ZONE | `u8` typ, x, y, w, h, długość nazwy, nazwa bez NUL | tak | round-trip syntetyczny |
| MOBJ, PSXM, ANIM, LGHT, RGEN | opisane w dokumencie | nie (pomijane) | — |

**Do potwierdzenia na oryginalnych plikach** (test `[gamedata] original GTA 2 map parses`
z `G2N_GAME_DIR`):
1. Liczba wpisów `blockd` w kolumnie. Dokument mówi „variable size – see height”; my
   przyjmujemy `height − offset`.
2. Kolejność osi w `base[256][256]` i w UMAP — przyjęto indeks `y·256 + x`.
3. Czy oryginalne poziomy używają DMAP (dokument: „PC version maps … will now use DMAP”).

Granica świata według dokumentu: gracz porusza się w `x, y ∈ 1..254`, `z ∈ 1..6`.
Uwzględnia to `sim::World::cell_passable`.
