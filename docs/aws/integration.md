# Integracja z AWS: oryginalne GTA 2 na EC2 G4dn

Cel infrastruktury się nie zmienia: EC2 G4dn uruchamia **oryginalne** GTA 2 (Windows
Server 2022, NVIDIA T4), żeby (1) sprawdzić zgodność renderowania DirectDraw/Direct3D
(`gfx-compat`), (2) automatycznie uruchamiać scenariusze (`gfx-e2e`) i (3) w przyszłości
nagrywać dane referencyjne (`provenance: originalGta2`) dla testów porównawczych silnika.

Szczegóły techniczne: [../ci/gpu-testing.md](../ci/gpu-testing.md). Checklista przed
pierwszym uruchomieniem: [../ci/gfx-compat-checklist.md](../ci/gfx-compat-checklist.md).

## Stan (2026-10-12, tylko odczyty AWS)

| Pozycja | Stan |
|---|---|
| Plan projektu | PAID (120 USD kredytów) |
| Limit „Running On-Demand G and VT instances” (eu-north-1) | **4 vCPU** (zgłoszenie `179182732600608` zamknięte 2026-10-12 19:43 UTC, wartość potwierdzona `GetServiceQuota`) |
| SCP | blokuje `iam:*Provider*` (GitHub OIDC), Spot, CloudFormation w us-east-1, pobieranie sterowników z bucketów AWS w us-east-1 |
| Wdrożone zasoby | brak (żadnych stosów, instancji ani bucketów) |
| Advanced features | nieaktywne (decyzja właściciela; aktywacja nieodwracalna i usuwa limit wydatków) |

## Pierwszy test bez OIDC i advanced features — możliwy

Symulacja IAM (`SimulatePrincipalPolicy` z kontekstem SCP) dla bieżącej roli potwierdza
**39/39** akcji potrzebnych do wdrożenia stosu hosta i uruchomienia testu (CloudFormation,
IAM role/profile w ścieżce `/gta20/`, EC2 `RunInstances` g4dn, SSM `SendCommand`, S3,
Lambda, EventBridge, CloudWatch Logs). Pierwszy test przeprowadza operator z własnego
komputera skryptami `scripts/aws/`:

```bash
infra/aws/create-budget.sh <e-mail> 15                # alerty (nie twardy limit)
scripts/aws/deploy-gfx-stack.sh                         # 💰 sprawdza limit GPU ≥ 4, wymaga wpisania "uruchom-ec2"
scripts/aws/upload-game.sh ~/gta2-10.5.zip             # prywatny bucket (blokada publicznego dostępu sprawdzana)
scripts/aws/run-gfx-compat-local.sh probe              # 💰 stop hosta w trap (błąd, Ctrl-C, timeout)
scripts/aws/run-gfx-compat-local.sh compat
scripts/aws/cost-check.sh                              # inwentarz kosztowych zasobów (tylko odczyt)
scripts/aws/teardown.sh [--purge-bucket]               # usunięcie stosów; wymaga wpisania "usun-gta20"
```

GitHub Actions (`gfx-compat`, `gfx-e2e`) wymaga roli OIDC, więc dopiero po aktywacji
advanced features.

## Odpowiedzi na pytania weryfikacyjne

| Pytanie | Odpowiedź | Pewność |
|---|---|---|
| Czy sterownik NVIDIA obsługuje WDDM i Direct3D na T4? | Dokumentacja AWS: na Windows sterowniki **Tesla działają domyślnie w TCC** (sam compute); oficjalne ścieżki graficzne to GRID lub gaming (WDDM). Oba leżą w bucketach S3 w us-east-1, a SCP **blokuje** ich pobranie (potwierdzone symulacją). Używamy więc publicznego sterownika 581.80 (SHA-256 i podpis Authenticode NVIDIA weryfikowane), a bootstrap przełącza T4 z TCC na WDDM (`nvidia-smi -dm 0` + restart). | **niepotwierdzone** — rozstrzygnie `probe` (`driver_model.current`, `gpuProcessSeen`) |
| Czy Windows zapewnia aktywną sesję graficzną? | SSM działa w sesji 0 bez pulpitu. Bootstrap tworzy użytkownika `gta2runner` z autologowaniem na konsoli; gra startuje przez zadanie harmonogramu `LogonType Interactive` w jego sesji. Basic Display Adapter **nie jest wyłączany** (bez RDP/DCV to jedyny ekran sesji konsoli). | projekt; weryfikacja w `probe` (`interactiveSession`) |
| Czy watchdog działa niezależnie od GitHub Actions? | Tak: zadanie systemowe (`shutdown /s` po `MaxRuntimeMinutes`, instalowane w UserData) i Lambda-reaper co 10 min (od `LaunchTime`, limit ograniczony do 15–180 min) nie zależą od GitHuba ani od siebie nawzajem. | analiza + testy offline |
| Czy instancja zatrzyma się po awarii, timeoucie lub anulowaniu? | GitHub: szybki stop w zadaniu sesji + `ensure-stopped` (`if: always()`); lokalnie: `trap` w `run-gfx-compat-local.sh` (test Bash potwierdza stop po nieudanym teście). Niezależnie: watchdog i reaper. Najgorszy przypadek ~80 min (~1 USD). | testy offline (atrapa `aws`) |
| Czy bootstrap poprawnie obsługuje restarty? | Kod 3010 → SSM restartuje i ponawia komendę. Kroki są idempotentne (markery w `C:\GTA20\state`), a licznik restartów (maks. 3 na przebieg) przerywa pętlę błędem `REBOOT_LOOP`. | test Pester (`Register-RebootAttempt`) |
| Czy `gfx-compat` odróżnia działający proces od renderowania? | Tak: przed startem gry zrzucany jest pulpit bazowy, a klatka liczy się jako wyrenderowana tylko przy ≥ 24 kolorach, ≤ 97% czerni **i** zmianie ≥ 25% pikseli względem bazy (albo zmianie rozdzielczości). Żywy proces nad niezmienionym pulpitem daje `FAIL_NO_VISIBLE_CHANGE`. | 17 testów Pester na syntetycznych buforach |

## Plan pozyskania danych referencyjnych

1. `gfx-compat probe` → sterownik, WDDM, sesja interaktywna.
2. `gfx-compat compat` → wybór konfiguracji renderowania (natywny DDraw / DDrawCompat / dgVoodoo2).
3. `gfx-e2e` z wybraną konfiguracją → stabilne uruchamianie poziomów.
4. Narzędzie nagrywające (do zaprojektowania; tylko Windows, uruchamiane na EC2): wstrzykuje
   wejście ze scenariusza (z nazw w GTA2_RE wynika, że oryginał ma mechanizm nagrywania
   wejścia dla dem) i odczytuje stan obiektów co tick (adresy pamięci wersji 10.5, ustalone
   analizą — bez kopiowania kodu). Zapisuje ślad JSONL `provenance: originalGta2` do S3.
5. Ślady referencyjne (bez plików gry) trafiają do `tests/conformance/reference/` i są
   porównywane z adapterem `engine` w CI.
