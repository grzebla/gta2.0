# Raport: przebieg odkrywczy `m1-downtown-baseline` — run `local-20261013-084136` (2026-10-13)

Pierwszy przebieg oryginalnego GTA 2 (9.6 + DDrawCompat, host EC2 z T4) dla scenariusza
[m1-downtown-baseline](../../tests/gfx/scenarios/m1-downtown-baseline.md). Zadanie T0012: analiza
offline, bez dostępu do hosta i bez ponownego przebiegu.

**Rodzaj materiału:** zrzuty **oryginalnego GTA 2** (`provenance: originalGta2`). Zrzuty, pliki gry i
pełne logi hosta zostają w lokalnym katalogu wyników
`~/Games/GTA2-work/gfx-results/local-20261013-084136/` — w repozytorium są tylko liczby i opisy.
Tapeta pulpitu hosta wyświetla identyfikatory maszyny i adresy sieciowe; nie są tu przepisane.

## 0. Oznaczenia

| Znacznik | Znaczenie |
|---|---|
| **[O]** | fakt zaobserwowany w plikach przebiegu: piksele zrzutów (porównane lokalnie) albo zawartość pliku |
| **[R]** | wartość zwrócona/zapisana przez runner (`result.json`, `summary.json`); mówi, co runner policzył, nie co zrobiła gra |
| **[N]** | niezweryfikowane — hipoteza albo wniosek z semantyki kodu, którego tu nie uruchomiono |

Metoda: jednorazowy skrypt Python poza repozytorium (biblioteka standardowa: `zlib` + własny dekoder
PNG, bez zapisu obrazów) — SHA-256 plików, pełna rozdzielczość, porównanie RGB piksel po pikselu,
prostokąt otaczający zmian. Wizualnie obejrzano `frame-006s.png` (pozostałe klatki są z nim
identyczne na powierzchni gry — sekcja 2).

## 1. Wynik w skrócie

* **Wszystkie 20 zaplanowanych klatek istnieje i daje się odczytać** (2304×800, RGBA). Brak klatek
  brakujących lub nieczytelnych. [O]
* **Wszystkie 20 klatek = `MENU`** (menu główne GTA 2, podświetlone „PLAY”, napis „GTA2 V9.6”).
  Żadna klatka nie jest `WORLD`; **brak użytecznej referencji `WORLD`**. [O]
* **Powierzchnia gry jest identyczna co do piksela od 6 s do 118 s**, także po ENTER w 15, 25 i
  65 s. Zmienia się wyłącznie zegar na pasku zadań pulpitu (wykluczony z wniosku). [O]
* Werdykt `FAIL_GAME_DIALOG` w `summary.json` **nie jest dowodem okna dialogowego**: `dialogs=null`
  w `result.json` prawdopodobnie oznacza „zero okien”, a `Get-CaseVerdict` liczy `@($null)` jako
  jedno okno (sekcja 4, defekt D1). [N — wniosek z kodu, nieuruchomiony]
* Runner zgłasza 2 zdarzenia `SendInput` na każdy ENTER [R]; **czy gra je odebrała i czy okno gry miało fokus w chwili
  wstrzyknięcia — nieustalone**. [N]

## 2. Klatki: geometria i identyczność

**Układ zrzutu [O].** Zrzut obejmuje cały wirtualny ekran 2304×800 i składa się z dwóch powierzchni:

| Obszar (px zrzutu) | Zawartość |
|---|---|
| x 0–1279, y 0–799 (1280×800) | **powierzchnia gry**: obraz 4:3 z czarnymi pasami po bokach; obraz zaczyna się w x = 107 (pomiar w wierszu 400) |
| x 1280–2303, y 0–767 (1024×768) | pulpit Windows (bez otwartych okien) z paskiem zadań (przycisk GTA2) i zegarem |
| x 1280–2303, y 768–799 | czarny pas poza pulpitem |

Wysokość 800 przy trybie gry 640×480 (`window` w `result.json`: 0,0–640,480 [R]) oznacza
niecałkowite skalowanie obrazu gry (800/480 ≈ 1,667) [N — wniosek z wymiarów]. Każda przyszła
lokalizacja (scenariusz, sekcja 6) musi wycinać powierzchnię gry i uwzględniać tę skalę.

**Grupy identycznych plików [O]** (SHA-256 całego pliku PNG, pierwsze 16 znaków):

| Grupa | Sekundy | SHA-256 (prefiks) |
|---|---|---|
| A | 6, 12, 18, 22, 28, 32, 36 | `ab45da95dd3555dc` |
| B | 40, 45, 50, 55, 60, 64, 68, 72, 80, 90 | `107226ba35d81e8b` |
| C | 100, 110, 118 | `5160dff28fb53925` |

**Różnice między grupami (pełna rozdzielczość) [O]:**

| Para | Zmienione piksele | Prostokąt zmian (x, y) | Na powierzchni gry |
|---|---|---|---|
| 36 s → 40 s (A → B) | 37 | 2265–2270, 734–742 | 0 |
| 90 s → 100 s (B → C) | 50 | 2265–2271, 734–742 | 0 |
| 100 s → 6 s (C → A, kontrola) | 39 | 2265–2271, 734–742 | 0 |
| `baseline-desktop.png` → 6 s | 1 025 920 | 0–1572, 0–799 | cała powierzchnia gry + przycisk GTA2 na pasku |

Prostokąt 2265–2271 × 734–742 leży w zegarze paska zadań. Zmiany grup wypadają między 36 a 40 s oraz
90 a 100 s od startu (`startedAt` 08:45:22 UTC → zmiana minuty ok. 08:46:00 i 08:47:00), czyli są
zmianą minut zegara. [O]

**Klasyfikacja wszystkich 20 klatek** (etykiety z sekcji 5 scenariusza):

| s | Plik | Grupa | Wejście przed klatką | Etykieta | Uwagi |
|---|---|---|---|---|---|
| 6 | `frame-006s.png` | A | — | `MENU` | menu główne widoczne przed pierwszym ENTER (gotowość do odbioru wejścia nieustalona) |
| 12 | `frame-012s.png` | A | — | `MENU` | |
| 18 | `frame-018s.png` | A | ENTER @ 15 | `MENU` | bez zmian względem 12 s |
| 22 | `frame-022s.png` | A | | `MENU` | |
| 28 | `frame-028s.png` | A | ENTER @ 25 | `MENU` | bez zmian względem 22 s |
| 32 | `frame-032s.png` | A | | `MENU` | |
| 36 | `frame-036s.png` | A | | `MENU` | |
| 40 | `frame-040s.png` | B | | `MENU` | różnica tylko w zegarze |
| 45 | `frame-045s.png` | B | | `MENU` | |
| 50 | `frame-050s.png` | B | | `MENU` | |
| 55 | `frame-055s.png` | B | | `MENU` | |
| 60 | `frame-060s.png` | B | | `MENU` | |
| 64 | `frame-064s.png` | B | | `MENU` | |
| 68 | `frame-068s.png` | B | ENTER @ 65 | `MENU` | bez zmian względem 64 s |
| 72 | `frame-072s.png` | B | | `MENU` | |
| 80 | `frame-080s.png` | B | | `MENU` | |
| 90 | `frame-090s.png` | B | | `MENU` | |
| 100 | `frame-100s.png` | C | | `MENU` | różnica tylko w zegarze |
| 110 | `frame-110s.png` | C | | `MENU` | |
| 118 | `frame-118s.png` | C | | `MENU` | |

Podstawa etykiety `MENU`: statyczna pełnoekranowa grafika frontendu (logo, pozycje
PLAY/OPTIONS/QUIT, „GTA2 V9.6”), brak siatki kafli; obraz obejrzany wizualnie w 6 s, pozostałe
klatki identyczne na powierzchni gry. Porównania z `data/frontend/*.tga` nie wykonano. Żadna klatka
nie wymagała etykiety `UNKNOWN`.

## 3. Czy powierzchnia gry zmienia się po ENTER?

**Nie** — dla wszystkich trzech wejść [O]:

| Wejście | Klatka przed | Klatka po | Zmienione piksele na powierzchni gry |
|---|---|---|---|
| ENTER @ 15 s | 12 s | 18 s, 22 s | 0 (identyczne pliki) |
| ENTER @ 25 s | 22 s | 28 s … 36 s | 0 (identyczne pliki) |
| ENTER @ 65 s | 64 s | 68 s … 118 s | 0 (identyczne pliki lub różnica tylko w zegarze) |

Po wykluczeniu zegara pulpitu cały przebieg (6–118 s) ma jedną, niezmienną klatkę gry. Nie da się z
tego odróżnić „gra nie dostała klawisza” od „gra nie przetwarza klatek / pętla stoi” — statyczne menu
wyglądałoby tak samo w obu przypadkach (sekcja 5).

## 4. Uzgodnienie `FAIL_GAME_DIALOG` z `result.json`

| Źródło | Wartość | Rodzaj |
|---|---|---|
| `summary.json` → `cases[0].verdict` | `FAIL_GAME_DIALOG`, `overall: INCOMPATIBLE` | [R] werdykt `Get-CaseVerdict` |
| `result.json` → `dialogs` | `null` | [R] |
| `result.json` → `inputsSent` | 3 × ENTER (15, 25, 65 s), `events: 2` każdy | [R] suma wartości `SendInput` |
| `result.json` → `alive`, `exitCode`, `error` | `true`, `null`, `null` | [R] `Process.HasExited` na końcu |
| `result.json` → `window` | tytuł „GTA2”, widoczne, 0,0–640,480 | [R] pomiar po ostatniej klatce |
| `frames[*].processAlive`, `foreground` | `true` we wszystkich 20 klatkach | [R] `GetForegroundWindow() == MainWindowHandle` |
| `frames[*]` metryki | identyczne w 20 klatkach: `uniqueColors` 16881, `blackRatio` 0,3558, `changedRatio` 0,5565 | [R] próbkowanie co 4 px; zgodne z [O] z sekcji 2 |
| Zrzuty | brak okna dialogowego na obu powierzchniach; pulpit bez otwartych okien, na pasku zadań przycisk GTA2 | [O] |
| stdout hosta | „no Application log errors collected” dla przypadku | [R] |

**Wniosek.** Obserwacja (brak okna na zrzutach [O]) i wartości runnera (okno gry na pierwszym planie
w chwilach zrzutów [R]) przeczą temu, żeby gra stała na własnym oknie komunikatu. Werdykt wynika najpewniej z defektu D1
(sekcja 5): runner zapisuje `null` zamiast pustej listy, a werdykt traktuje `null` jako jedno okno.
Gdyby `dialogs` liczono jako 0, ta sama ścieżka `Get-CaseVerdict` dałaby `PASS` (progi
`minUniqueColors` 24, `maxBlackRatio` 0,97, `minChangedRatio` 0,25 są spełnione w każdej klatce)
[N]. `PASS` mówi jednak tylko, że gra coś narysowała — runner nie odróżnia `MENU` od `WORLD`, więc
żaden z tych werdyktów nie odpowiada na pytanie scenariusza.

**Wejścia.** `events: 2` to liczba zdarzeń (wciśnięcie + puszczenie), które `SendInput` przyjął do
kolejki wejścia systemu [R]. Nie dowodzi to dostarczenia do gry ani obsłużenia klawisza:
`SendInput` nie zgłasza m.in. blokady UIPI, a runner nie zapisuje żadnej obserwacji skutku
klawisza. Konsumpcja wejść jest **nieustalona** [N]; widocznego skutku brak [O].

## 5. Runner: rewizja i podejrzane defekty

**Rewizja przeczytana:** `origin/main` = `d7b8be7` (merge PR #9). Ostatnie zmiany plików:
`tests/gfx/windows/Run-Gta2Case.ps1` — `9ec2433` (blob `b38fddb`),
`tests/gfx/windows/GfxAnalysis.psm1` — `d274154` (blob `59ab0b6`),
`tests/gfx/windows/Invoke-GfxCompat.ps1` — `9ec2433`.

**Co wdrożono — nieustalone.** `scripts/ci/ec2-gpu-session.sh` (origin/main, wiersz 95) kopiuje na
host `tests/gfx/windows/` z checkoutu, który uruchomił sesję. Pola `dialogs` i `inputsSent` w
`result.json` zapisują tylko rewizje runnera zawierające `53be3cc`/`9ec2433`; kopia runnera w bazie tej
gałęzi (`a20c656`, wspólny przodek z main `8f820b7`) ich nie ma. Wyniki nie zawierają rewizji ani
skrótu wdrożonych skryptów, więc **nie da się udowodnić, że wdrożono właśnie `d7b8be7`**. Poniższe
defekty dotyczą kodu z `d7b8be7`; ich zgodność z przebiegiem jest wnioskiem, nie dowodem.

**D1 — fałszywy `FAIL_GAME_DIALOG` przy braku okien [N, wysoka pewność].**
* `Run-Gta2Case.ps1:209` (blok główny, `blame` `53be3cc`):
  `$result.dialogs = if ($result.alive) { @(Get-ProcessDialog ...) } else { @() }`. Przypisanie wyniku
  instrukcji `if` przechodzi przez potok PowerShella; pusta tablica nie daje żadnego elementu, więc
  zmienna dostaje `$null` (w JSON `null`). Znalezione okno dałoby obiekt z `title`/`texts`, nie
  `null`.
* `GfxAnalysis.psm1:106`, funkcja `Get-CaseVerdict` (`blame` `d274154`):
  `@($Result.dialogs).Count -gt 0` — dla `$null` `@($null).Count` wynosi 1, więc zwraca
  `FAIL_GAME_DIALOG`.
* Testy Pester (`tests/gfx/windows/tests/GfxAnalysis.Tests.ps1:91-96`) sprawdzają tylko niepustą listę
  okien; brak przypadku `null`/pustej listy.
* Nie uruchomiono lokalnie (brak `pwsh` w tym środowisku); wniosek z udokumentowanej semantyki
  PowerShella.

**D2 — brak dowodu dostarczenia wejścia [luka weryfikacji, nie udowodniony błąd].**
`Send-GameKey` (`Run-Gta2Case.ps1:102-120`) wysyła kod skanowania (`wVk = 0`,
`KEYEVENTF_SCANCODE`, przytrzymanie `HoldMs`) po `SetForegroundWindow` i zwraca tylko sumę wartości
`SendInput`. Runner nie zapisuje: `GetLastError` przy krótszej sumie, stanu `Process.Responding`
ani czasu CPU gry przed/po klawiszu, poziomu integralności procesu gry, ani rewizji wdrożonych
skryptów. Możliwe przyczyny braku reakcji (wszystkie [N], ten przebieg ich nie rozróżnia):

1. Gra nie przetwarza klatek/wejścia po wyświetleniu menu (identyczne piksele 6–118 s pasują też do
   zatrzymanej pętli).
2. Wstrzyknięty kod skanowania nie trafia na ścieżkę wejścia frontendu 9.6 (sposób odczytu klawiatury
   przez frontend nie jest w repo ustalony).
3. UIPI — mało prawdopodobne: runner działa z `RunLevel Limited` (`Invoke-GfxCompat.ps1:226`) i sam
   uruchamia grę (`Start-Process`), ale poziom integralności gry nie jest zapisany.

4. Okno gry nie miało fokusu w chwili wstrzyknięcia. `foreground: true` [R] jest mierzone tylko w
   chwilach zrzutów (najbliższe: 1–3 s przed i 3 s po każdym ENTER), a wynik `SetForegroundWindow` w
   `Send-GameKey` i stan pierwszego planu w trakcie wciśnięcia nie są zapisywane — fokus podczas
   wstrzyknięcia jest **niezweryfikowany** [N].
5. Gra nie była jeszcze gotowa na wejście. Menu było **widoczne** już w 6 s [O], ale widoczny
   obraz menu nie dowodzi, że frontend przyjmuje klawisze [N].

Żadnej z tych przyczyn ten przebieg nie wyklucza.

**Uwaga o wyświetlaczach [O].** `Initialize-GfxHost.ps1:291-300` wyłącza Basic Display Adapter
(środowisko przebiegu pokazuje go jako wyłączony), a mimo to wirtualny ekran ma dwie powierzchnie
(sekcja 2). Na samą klasyfikację to nie wpływa; ma znaczenie dla wycinania i skali przy lokalizacji.

## 6. Co ten przebieg dowodzi, a czego nie

Dowodzi [O]: oryginalne GTA 2 **9.6** startuje na hoście w pełnym ekranie przez DDrawCompat, menu
główne jest widoczne przed 6 s, domyślnie podświetlone jest „PLAY”, na zrzutach nie ma okna
dialogowego. Według runnera [R] proces żył do końca (≈ 120 s; `alive`, `processAlive`).

Nie dowodzi: że klawisze dotarły do gry; że okno gry miało fokus w chwili wstrzyknięcia; że frontend
był gotowy na wejście; że pętla gry działała po 6 s; niczego o świecie, kamerze ani położeniu w
`wil.gmp`. Pytania 1–4 scenariusza (sekcja 1 scenariusza) pozostają otwarte;
WG-PROJ i lokalizacja nie mają materiału.

## 7. Następny warunek wstępny (jeden)

**Przed decyzją o kolejnym płatnym przebiegu: właściciel przegląda podejrzewany defekt werdyktu D1 i
przygotowuje ograniczoną diagnostykę wejścia w runnerze, identyfikowaną rewizją.** Zmiana
harmonogramu wejść nie ma dziś podstawy w danych; `inputs` scenariusza zostaje bez zmian.

To zmiana w chronionej ścieżce `tests/gfx/windows/` (agenci jej nie modyfikują) — **przekazanie
właścicielowi** (przez Astrę). Proponowany zakres:

1. D1 do przeglądu: `Run-Gta2Case.ps1:209` (`null` zamiast pustej listy) i `GfxAnalysis.psm1:106`
   (`@($null).Count` = 1), z przypadkami Pester dla `null` i pustej listy.
2. Diagnostyka wokół każdego wstrzyknięcia w `Send-GameKey`/pętli zdarzeń: wynik
   `SetForegroundWindow`, stan pierwszego planu tuż przed i tuż po klawiszu, `GetLastError` gdy suma
   `SendInput` < 2, a pomocniczo `Process.Responding`, czas CPU i poziom integralności procesu gry.
3. Identyfikacja rewizji: SHA-256 wdrożonych skryptów runnera w `result.json`.

To są **dane diagnostyczne, nie dowód** dostarczenia ani obsłużenia klawisza: zawężają możliwe
przyczyny z sekcji 5, ale żadna z tych wartości nie pokazuje, że gra odczytała wejście. Konsumpcja
wejścia może pozostać nierozstrzygnięta także po przebiegu z diagnostyką. O kolejnym przebiegu należy
zdecydować dopiero po przejrzeniu tych zmian i zebranych danych.
