# GTA2_RE jako referencja (zasady clean-room)

[CriminalRETeam/gta2_re](https://github.com/CriminalRETeam/gta2_re) to dekompilacja GTA 2
w wersji PC 10.5. Repozytorium **nie ma licencji** (stan sprawdzony 2026-10-12), więc
wszystkie prawa pozostają przy autorach. Traktujemy je wyłącznie jako materiał do
zrozumienia zachowania oryginalnej gry.

## Zasady

1. **Nie kopiujemy** kodu, struktur, układów pól ani fragmentów funkcji z GTA2_RE. Dotyczy
   to także przepisywania „linijka w linijkę” na C++20.
2. Analiza i implementacja są rozdzielone:
   * **Analiza** → specyfikacja zachowania w `docs/specs/<system>.md` własnymi słowami
     (wejścia, wyjścia, stany, stałe zmierzone na oryginale), ze źródłami (dokument formatu,
     pomiar na EC2, nazwa funkcji GTA2_RE jako wskazówka, gdzie szukać).
   * **Implementacja** powstaje na podstawie specyfikacji i testów porównawczych, nie na
     podstawie kodu GTA2_RE.
3. Wartości liczbowe (stałe fizyki, czasy, progi) ustalamy pomiarem na oryginalnej grze
   (ślady `originalGta2`), a nie odczytem z dekompilacji.
4. W specyfikacji każde stwierdzenie ma status: **potwierdzone testem** (link do scenariusza),
   **z dokumentu formatu**, albo **hipoteza**.
5. W kodzie i commitach podajemy źródło informacji (dokument formatu, scenariusz, pomiar).

## Co wiemy o strukturze oryginału (tylko nazwy, jako mapa do analizy)

* Pętla gry i kolejność aktualizacji: `Game_0x40.cpp` (`UpdateGame`, `GetFrameDurationMs`) —
  oryginał symuluje stałym krokiem.
* RNG: `rng.cpp`. Nagrywanie wejścia (tryb demo): `InputRecorder_67F8B0.cpp`.
* Mapowanie systemów na pliki: [mechanics-matrix.md](../mechanics-matrix.md) oraz archiwalny
  audyt [archive/2026-10-12-opengta2-feature-audit.md](../archive/2026-10-12-opengta2-feature-audit.md).

## Inne źródła

* Dokumenty formatów DMA Design (mapa: [formats/gmp.md](../formats/gmp.md)).
* Pomiary na oryginalnej grze uruchamianej na EC2 ([aws/integration.md](../aws/integration.md)).
