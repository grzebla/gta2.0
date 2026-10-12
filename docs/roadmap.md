# Plan implementacji

Kolejność wynika z zależności w [mechanics-matrix.md](mechanics-matrix.md). Każdy etap
kończy się działającym, przetestowanym przyrostem; zgodność z oryginałem deklarujemy
tylko po teście porównawczym z danymi `originalGta2`.

| Etap | Zakres | Kryterium ukończenia |
|---|---|---|
| 0 ✅ | Szkielet: CMake/presety, CI 3 platformy, core, GMP, symulacja prototypowa, SDL_GPU, headless runner, testy | ten PR |
| 1 | Dane oryginalne: weryfikacja parsera GMP na prawdziwych mapach (`[gamedata]`), parser `.sty` (kafle, palety, sprite'y), tekstury w rendererze | mapa `wil.gmp` renderowana kaflami z `wil.sty` |
| 2 | Świat 3D z góry: bloki ze ścianami i skosami, warstwy z, kamera z wysokością | zrzut porównywalny wizualnie z oryginałem (gfx-e2e) |
| 3 | Referencje z oryginału: pierwszy `gfx-compat` na EC2 → `gfx-e2e` → narzędzie nagrywające ślady (`provenance: originalGta2`) dla prostych scenariuszy ruchu | pierwszy scenariusz porównawczy z danymi oryginału w repo (bez plików gry) |
| 4 | Ruch pieszego i kolizje zgodne z oryginałem | scenariusze ruchu PASS w tolerancji |
| 5 | Pojazdy: fizyka, wsiadanie, kolizje | scenariusze jazdy PASS |
| 6 | Piesi/AI, ruch uliczny, policja | scenariusze statystyczne |
| 7 | Broń, obrażenia, gangi, respekt | scenariusze zdarzeń |
| 8 | Skrypty misji, teksty, HUD | misja testowa przechodzi w obu grach |
| 9 | Dźwięk, zapis gry, frontend | — |

Zasada wydajności: każda optymalizacja zaczyna się od pomiaru (`gta20-headless bench`,
`gta2 --frames`), a regresje wydajności są wychwytywane w CI (podsumowanie benchmarku).
