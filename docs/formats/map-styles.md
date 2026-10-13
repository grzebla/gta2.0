# Mapy i ich style

Oryginalne wydanie PC (9.6, identyczne dane w 10.5) zawiera 17 map `.gmp` i 4 style `.sty`
(`wil`, `ste`, `bil`, `fstyle` — ten ostatni nie ma kafli i żadna mapa go nie używa).
Mapa nie zapisuje swojego stylu; parę mapa → styl deklarują pliki konfiguracyjne gry:

* `data/test1.seq` — 12 poziomów jednoosobowych, rekordy `MainOrBonus`, `GMPFile`, `STYFile`,
  `SCRFile`, `Description`;
* `data/*.mmp` — mapy wieloosobowe, sekcja `[MapFiles]` z `GMPFile`, `STYFile`, `SCRFile`,
  `Description`, `PlayerCount`. Warianty 2p–6p tej samej mapy podają ten sam styl.

Zapis w kodzie: `original_map_styles()` w `engine/assets/src/map_inventory.cpp`. Ścisła bramka
(`gamedata-strict`, [../testing.md](../testing.md#ścisła-inwentaryzacja-map-gamedata-strict))
sprawdza przy każdym uruchomieniu, że wskazane źródło deklaruje tę parę i że żaden inny
`test1.seq`/`*.mmp` jej nie przeczy. Aplikacja `gta2` dobiera według tej tabeli domyślny styl
(np. `lorne2e.gmp` → `wil.sty`); dla map spoza tabeli nadal używa pliku `.sty` o tej samej nazwie.

Wszystkie 17 przypisań jest rozstrzygniętych (pomiar 2026-10-13 na 9.6). Przypisanie
nierozstrzygnięte miałoby pusty styl i opis w polu źródła; bramka traktuje je jako błąd.

| Mapa | Styl | Źródło | Rola |
|---|---|---|---|
| `data/wil.gmp` | `data/wil.sty` | `data/test1.seq` | miasto 1 (Downtown) |
| `data/lorne2e.gmp` | `data/wil.sty` | `data/test1.seq` | miasto 1, bonus 1 |
| `data/lorne2m.gmp` | `data/wil.sty` | `data/test1.seq` | miasto 1, bonus 2 |
| `data/lorne2h.gmp` | `data/wil.sty` | `data/test1.seq` | miasto 1, bonus 3 |
| `data/ste.gmp` | `data/ste.sty` | `data/test1.seq` | miasto 2 (także `res-2p…6p.mmp`) |
| `data/mike1e.gmp` | `data/ste.sty` | `data/test1.seq` | miasto 2, bonus 1 |
| `data/mike1m.gmp` | `data/ste.sty` | `data/test1.seq` | miasto 2, bonus 2 |
| `data/mike1h.gmp` | `data/ste.sty` | `data/test1.seq` | miasto 2, bonus 3 |
| `data/bil.gmp` | `data/bil.sty` | `data/test1.seq` | miasto 3 |
| `data/mike2e.gmp` | `data/bil.sty` | `data/test1.seq` | miasto 3, bonus 1 |
| `data/mike2m.gmp` | `data/bil.sty` | `data/test1.seq` | miasto 3, bonus 2 |
| `data/mike2h.gmp` | `data/bil.sty` | `data/test1.seq` | miasto 3, bonus 3 |
| `data/wil-multi.gmp` | `data/wil.sty` | `data/downtown-2p.mmp` (też 3p–6p) | multiplayer |
| `data/bil-multi.gmp` | `data/bil.sty` | `data/Industrial-2P.mmp` (też 3P–6P) | multiplayer |
| `data/MP1-comp.gmp` | `data/bil.sty` | `data/mp1-2p.mmp` (też 3p–6p) | multiplayer |
| `data/MP2-comp.gmp` | `data/wil.sty` | `data/mp2-2p.mmp` (też 3p–6p) | multiplayer |
| `data/MP5-comp.gmp` | `data/ste.sty` | `data/mp5-2p.mmp` (też 3p–6p) | multiplayer |

Nazwy plików podano tak, jak występują w wydaniu 9.6; porównania są niewrażliwe na wielkość liter.
