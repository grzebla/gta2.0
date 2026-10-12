# Raport: oryginalne zasoby GTA 2 i pierwszy render prawdziwej mapy (2026-10-12)

PR #3 (`feature/original-gta2-assets` → `migrate/native-cpp-engine`). Pliki gry są wyłącznie
lokalne (`~/Games/...`); w repozytorium są tylko kod, testy, narzędzia i ten raport.
Zrzuty ekranu z prawdziwymi teksturami są w lokalnym raporcie `~/Games/GTA2-work/REPORT.md`.

## 1. Instalatory i wersje

| Plik | Rozpoznanie | Wersja gry |
|---|---|---|
| `~/Downloads/GTA2INSTALLER/GTA2.exe` (362 331 961 B, SHA-256 `0cd38049…b083a0`) | InstallShield „Setup Player” (InstallShield 7, „Setup Player 2K2”); osadzone `data1.hdr/.cab`, `data2.cab` | `gta2.exe` **9.6.0.0**, PE 2004-04-27 — wydanie Rockstar Classics |
| `~/Downloads/gta2freeloader/gta2freeloader.exe` | **sam plik wykonywalny gry** (nie instalator) | **10.5.0.0**, PE 2001-01-29 (wersja GTA2_RE) |
| `35_53.exe`, `35_54.exe`, `35_55.exe` | DynaZIP SFX → instalator „Active Delivery” + Microsoft CAB (`ic31/ic49/ic3B.cab`) | 10.5: 341 plików, bez muzyki i `intro.bik` |

## 2. Ekstrakcja (bez uruchamiania żadnego EXE, bez Wine, bez sudo)

* 9.6: własny skrypt wycina 15 osadzonych plików (`tools/gta2-install/carve_isplayer.py`, pokrycie
  do ostatniego bajtu) → `unshield 1.6.2` (Homebrew) rozpakowuje grupę „App Executables” (344 pliki).
* 10.5: `unzip` z SFX → `7z` z plików CAB.
* Procedura: `tools/gta2-install/README.md`.

## 3. Lokalizacje

| Katalog | Zawartość |
|---|---|
| `~/Games/GTA2` | 9.6, 344 pliki, 384,5 MiB — **źródło danych silnika** (`G2N_GAME_DIR`) |
| `~/Games/GTA2-10.5` | 10.5, 341 plików — binarka referencyjna |
| `~/Games/GTA2-work` | katalog roboczy, manifesty SHA-256 (`manifest.json`, `manifest-10.5.json`), zrzuty, lokalny raport |

Instalator oryginalny nie był modyfikowany.

## 4. Manifest (skrót)

9.6: 17 × `.gmp` (11,3 MiB), 4 × `.sty` (19,3 MiB), 109 × `.scr`, 30 × `.mmp`, 4 × `.gxt`, 4 × `.sdt` + 4 × `.raw`,
119 × `.wav` (293,8 MiB, radio), 26 × `.tga`, `intro.bik`, `gta2.exe`, 8 × `.dll`. Definicje pojazdów
i obiektów są w `.sty` (chunki CARI, OBJI). Kluczowe hashe: `gta2.exe` `420af867…972e089f`,
`wil.gmp` `3b03c5eb…3fc5fabfb`, `wil.sty` `db72f986…da733b746c`. Pełne listy: manifesty lokalne.

**Zgodność 9.6 ↔ 10.5:** wszystkie mapy i style, efekty dźwiękowe, `e.gxt` i 104 skrypty są
identyczne bajtowo. Różnią się DLL-e renderera, `gta2 manager.exe` i pliki trybu wieloosobowego
(`.mmp` tylko opisem, `res-*.scr` treścią) — szczegóły w `tools/gta2-install/README.md`.

## 5. Parsery na prawdziwych danych

| Parser | Wynik |
|---|---|
| `.gmp` | 17/17 map; DMAP potwierdzone (układ kolumn 100 %, osie 84–86 % vs ~20 %), CMAP to inna, starsza wersja miasta, kafel 1023 = znacznik skosu (98,8 %), 992–1022 tylko w `mike1*` |
| `.sty` | 4/4 style; wersja **700** (dokument: 600) + nieopisany chunk `SPEC`; 992 kafle / 320 palet / 2089 sprite'ów w miastach; układ palet i kafli potwierdzony wizualnie |
| uszkodzone dane | obcięte i zmodyfikowane oryginały (mapa, styl) są odrzucane bez awarii |

Szczegóły: [../formats/gmp.md](../formats/gmp.md), [../formats/sty.md](../formats/sty.md).

## 6–7. Render autentycznej mapy

**Tak.** Downtown (`wil.gmp` + `wil.sty`), Residential (`ste`) i Industrial (`bil`) renderują się
natywnie na macOS przez SDL_GPU/Metal: oteksturowane pokrywy bloków, poprawna orientacja (ciągłe
pasy i łuki, czytelne napisy „SLOW”), kamera śledząca i swobodna z przesuwaniem i zoomem,
diagnostyka w tytule okna. Okno: 119 klatek, średnio 8,7 ms, p99 20,4 ms (Debug). Zrzuty: lokalnie.

## 8. Problemy i ograniczenia

* Widok z góry rysuje tylko **pokrywy**; ściany 3D, skosy i perspektywa jeszcze nie istnieją.
* Oświetlenie pokryw (bity 10–11) nie jest stosowane — brak udokumentowanych współczynników;
  zgadywane wartości zniekształcały dachy.
* Kafle animowane (992–1022, chunk `ANIM`) i trójboczne skosy (1023) nie są rysowane.
* Nieobsłużone: delty `.sty`, pojazdy (CARI), obiekty (OBJI/MOBJ), czcionki, remapy, `SPEC`, `EDIT`.
* Wersja 10.5 nie zawiera muzyki i filmu; do testów EC2 zgodnych z GTA2_RE wystarcza.
* Limit GPU w AWS ma już wartość 4 vCPU (wniosek zamknięty), ale w tym zadaniu nie dotykano AWS.

## 9. Zmiany w kodzie i testy

Kod: `assets/sty_style.*`, `assets/game_install.*`, rozszerzony `gmp_map`, atlas i oteksturowane
pokrywy w `scene`, tekstury w `render` (shadery MSL/HLSL/GLSL→SPIR-V), nowa aplikacja `gta2n`,
narzędzia `tools/gta2-install`, reguły `.gitignore` dla plików gry.

| Zestaw | Wynik |
|---|---|
| Testy syntetyczne i regresji (bez danych), Debug | 54/54 (testy danych pominięte) |
| Z danymi (`G2N_GAME_DIR`, `G2N_GAME_DIR_105`), Debug | 54/54; 9 przypadków `[gamedata]`, 448 880 asercji |
| ASan/UBSan z danymi | 51/51 |
| Render CTest `gamedata.render.{wil,ste,bil}` | 3/3 (≥ 500 kolorów, 100 % pokrycia) |
| headless | 51/51 |
| clang-format, clang-tidy, granice modułów | bez uwag |

## 10. Następne kroki

1. Geometria 3D bloków: ściany (left/right/top/bottom) z perspektywą kamery, skosy 1–60, flat.
2. Animacje kafli (`ANIM`) i trójboczne skosy (znacznik 1023).
3. Sprite'y: obiekty mapy (MOBJ), pojazdy i piesi z `.sty` (CARI, delty).
4. Pomiar oświetlenia pokryw i kolorów na oryginale (EC2, `gfx-e2e` na 10.5).
5. Parser `.mmp` (opis poziomów) i `.gxt` (teksty).
