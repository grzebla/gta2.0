# Styl GTA 2 (`.sty`)

Źródło: dokument DMA Design „GBH Style Format” v4.6 (1999), opublikowany na
[gtamp.com](https://gtamp.com/gta2/gta2-style-sty-graphics-file-format/). Implementacja:
`engine/assets/src/sty_style.cpp`; atlas tekstur: `engine/scene/src/draw_list.cpp`.
Weryfikacja na oryginalnych plikach PC 9.6 i 10.5 (identyczne bajtowo), 2026-10-12.

| Element | Dokument | Oryginalne pliki | Implementacja |
|---|---|---|---|
| Nagłówek | `"GBST"`, wersja 600 | **wersja 700** | akceptujemy 600 i 700 |
| Chunki | PALX, PPAL, PALB, TILE, SPRG, SPRX, SPRB, DELS, DELX, FONB, CARI, OBJI, PSXT, RECY | jak w dokumencie + **nieopisany `SPEC`**; `fstyle.sty` nie ma TILE ani CARI | PALX, PPAL, PALB, TILE, SPRX, SPRG, SPRB; reszta pomijana |
| PALX | `u16[16384]` wirtualna → fizyczna paleta | max 275 < 320 palet | walidowane |
| PPAL | strony 64 KiB, 64 palety „w kolumnach o szerokości dworda”, BGRA | 320 palet (5 stron) w miastach, 64 w `fstyle` | kolor `i` palety `p`: `strona(p/64)·64K + i·256 + (p%64)·4` — **potwierdzone wizualnie** |
| PALB | 8 × `u16` liczników → bazy skumulowane | `992, 2089, 37, 53, 6, 1, 15, 9` (suma 3202) | tak |
| TILE | 64×64, 8 bpp, strony 256×256, 992 kafle (63 strony) | **62 strony**, 992 kafle | kafel `n`: strona `n/16`, slot `n%16` w siatce 4×4 wierszami — **potwierdzone wizualnie** (brak szwów, napisy czytelne, bez odbić) |
| Paleta kafla | wirtualna = `PALB.tile + n` | — | tak |
| Kolor 0 | zawsze przezroczysty | 4,9 % pikseli kafli | alfa 0 |
| SPRX | `u32 ptr, u8 w, u8 h, u16 pad` | 2089 sprite'ów (miasta), 1766 (`fstyle`) | walidacja mieszczenia się w SPRG |
| SPRB | 6 × `u16` liczników | `77, 474, 552, 90, 173, 723` | suma = liczba wpisów SPRX (walidowane) |
| Kafle 992–1023 | zarezerwowane | mapy używają 1023 jako znacznika skosu (patrz gmp.md) | nie rysowane |

Nie zaimplementowano jeszcze: delty (DELS/DELX), definicje pojazdów (CARI), obiekty (OBJI),
czcionki (FONB), remapy palet (car/ped remap), `SPEC`.
