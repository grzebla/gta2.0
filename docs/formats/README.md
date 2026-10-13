# Formaty plików GTA 2

Źródła informacji, od najbardziej wiarygodnych:
1. Opublikowane dokumenty formatów DMA Design / Rockstar (np. „GTA2 map format” v8.2, 1999).
2. Pomiary na plikach z legalnej instalacji (testy `[gamedata]`).
3. GTA2_RE — wyłącznie do rozpoznania, które struktury i pola istnieją (bez kopiowania kodu).

| Plik | Format | Stan w silniku |
|---|---|---|
| `*.gmp` | mapa (kontener `GBMP`, wersja 500) | parser DMAP/CMAP/UMAP + ZONE — [gmp.md](gmp.md) |
| `*.sty` | style: tekstury kafli, sprite'y, palety, definicje pojazdów (`GBST`) | brak (następny krok renderu) |
| `*.scr` | skrypty misji | brak |
| `*.gxt` | teksty (`GBL`) | brak |
| `*.sdt` + `*.raw` | dźwięki | brak |
| `*.mmp`, `test1.seq` | opis poziomu (mapa, styl, skrypt) | tylko pary mapa → styl, sprawdzane przez `gamedata-strict` — [map-styles.md](map-styles.md) |
| `*.bik` | filmy Bink | poza zakresem (kodek zastrzeżony) |

Kontener chunków (`engine/assets/chunk_file.*`) obsługuje każdy plik o układzie:
`char[4]` typ pliku, `u16` wersja, potem chunki `char[4]` + `u32` rozmiar + dane.
