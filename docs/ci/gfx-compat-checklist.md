# Checklista przed pierwszym `gfx-compat` (stan na 2026-10-12)

Legenda: ✅ zweryfikowane · ⏳ czeka na zewnętrzną decyzję · 🧑 wymaga Twojej decyzji · 💰 tworzy koszty

## 1. Stan konta AWS (odczyty z 2026-10-12)

| Pozycja | Stan | |
|---|---|---|
| Plan projektu | PAID, aktywny, 120 USD kredytów | ✅ |
| Region | eu-north-1; G4dn (`xlarge`, `2xlarge`) w strefach a, b, c | ✅ |
| Limit „Running On-Demand G and VT instances” | 4 vCPU (zgłoszenie `179182732600608` zamknięte 2026-10-12 19:43 UTC) | ✅ |
| SCP: `iam:CreateOpenIDConnectProvider` | **explicitDeny** (symulacja IAM) — wymaga advanced features | 🧑 |
| SCP: `ec2:RunInstances` g4dn, `cloudformation:CreateStack`, `lambda:CreateFunction`, `events:PutRule`, `s3:CreateBucket`, `ssm:SendCommand`, `iam:CreateRole`/`PassRole`, `budgets:*` | allowed | ✅ |
| SCP: Spot, CloudFormation w us-east-1, `s3:GetObject` z bucketów sterowników AWS w us-east-1 | explicitDeny — projekt tego nie używa | ✅ |
| Istniejące instancje EC2 | brak | ✅ |
| Budżety / monitory anomalii kosztów | brak | 🧑 |
| AMI Windows Server 2022 | `ami-02115c83a28601673` (2026.09.17, wycofanie 2028-09-18) | ✅ |
| Dokument SSM `AWS-RunPowerShellScript` | Active | ✅ |
| Domyślne podsieci | `subnet-01ca4e08a6f9fa5b3` (1a), `subnet-04e7f796ef09fd1c1` (1b), `subnet-0f13ba440d6148a1e` (1c); publiczny IPv4 automatycznie | ✅ |

## 2. Weryfikacja szablonów i polityk

| Sprawdzenie | Wynik |
|---|---|
| cfn-lint 1.57.2 (`--regions eu-north-1`, w tym dostępność typów zasobów w regionie) | 0 problemów |
| IAM Access Analyzer `ValidatePolicy` — wszystkie polityki uprawnień, polityka bucketu, zaufania ról usług | 0 findings |
| Access Analyzer — zaufanie OIDC po zmianie | tylko sugestia `CONFIRM_AUDIENCE_CLAIM_TYPE` (informacyjna; `aud` jest już porównywany przez `StringEquals` bez kwalifikatora) |
| actionlint 1.7.12, shellcheck, PSScriptAnalyzer 1.25.0 | 0 problemów |
| gitleaks 8.30.1 na całym repo | brak wycieków |

Zmiana po weryfikacji: GitHub odrzucił środowisko z wymaganym recenzentem
(HTTP 422: plan nie obsługuje tej reguły w repo prywatnym). Zaufanie OIDC opiera się
teraz na niezmiennych ID repozytorium, właściciela i aktora, gałęzi `main` oraz pliku
`gfx-session.yml`. Zatwierdzeniem jest ręczne `workflow_dispatch` z wpisanym
`cost_confirmation=uruchom-ec2`.

## 3. Niezależność mechanizmów zatrzymania EC2

| Mechanizm | Wyzwalacz | Zależy od | Nie zależy od | Znane ograniczenia |
|---|---|---|---|---|
| A. Watchdog w systemie (`GTA20-Watchdog`) | `shutdown /s` po `MaxRuntimeMinutes` od startu systemu | Windows uruchomiony, harmonogram zadań, IMDS (fallback 60 min) | GitHub, Lambda, EventBridge, API EC2, IAM | restart systemu (np. bootstrap 3010) liczy czas od nowa; zawieszony system go nie wykona |
| B. Lambda reaper (co 10 min) | `LaunchTime` > `MaxRuntimeMinutes` + 10 min (ograniczone do 15–180) | EventBridge, Lambda, API EC2, rola reapera | Windows, GitHub, OIDC | błędy Lambdy nie mają alarmu; reaguje do ~20 min po limicie |
| C. `ensure-stopped` w GitHub Actions | koniec sesji (sukces, błąd, timeout, anulowanie) | GitHub Actions, OIDC, rola stop | Windows, Lambda | nie zadziała przy awarii GitHuba lub przy braku roli/dostawcy OIDC |
| D. Szybki stop w zadaniu sesji | ostatni krok sesji (`if: always()`) | jak C | jak C | — |

Wnioski:
* A i B są od siebie niezależne i od GitHuba. A działa wewnątrz systemu, B z zewnątrz.
  B używa `LaunchTime`, którego restart systemu nie zmienia, więc ogranicza całkowity czas
  także wtedy, gdy A został wydłużony restartami.
* Wspólne punkty: A i B czytają ten sam tag `MaxRuntimeMinutes`. Tagu nie może zmienić
  rola CI (jawny Deny `ec2:CreateTags`), a oba mechanizmy ograniczają wartość do 15–180.
  B, C i D zależą od działania API `StopInstances`.
* Najgorszy przypadek przy awarii GitHuba i zawieszonym Windows: zatrzymanie przez B po
  ok. 60 + 10 + 10 = 80 min, czyli ~1 USD.

## 4. Koszty (cennik AWS Price List API, eu-north-1, 2026-10-12)

| Stan | Składniki | Koszt |
|---|---|---|
| Zatrzymana instancja (stały) | EBS gp3 60 GiB × 0,0836 USD | **≈ 5,02 USD/mies.** |
| | publiczny IPv4 — zwalniany przy zatrzymaniu | 0 |
| | Lambda reaper: ~4 320 wywołań × 128 MB × <1 s ≈ 540 GB-s | ≈ 0 (w darmowym limicie Lambdy; bez niego < 0,01 USD) |
| | CloudWatch Logs reapera (~1–2 MB/mies. × 0,54 USD/GB), reguła EventBridge | ≈ 0 |
| | S3: gra ~0,3–0,4 GB × 0,023 USD; `runs/` wygasa po 14 dniach | ≈ 0,01 USD |
| Działająca instancja | g4dn.xlarge Windows 0,742 USD/h + IPv4 0,005 USD/h | **≈ 0,75 USD/h** |
| Pierwszy `probe` | bootstrap: pobranie sterownika ~712 MB, instalacja, 1–2 restarty, ~20–30 min | ≈ 0,25–0,40 USD |
| `compat` (8 przypadków) | ~30–40 min | ≈ 0,40–0,50 USD |
| Najgorszy przypadek jednej sesji | 80 min (pkt 3) | ≈ 1,00 USD |
| Transfer | ruch przychodzący i EC2↔S3 w tym samym regionie | 0 |
| GitHub Actions (repo prywatne) | sesja ≈ czas testu na `ubuntu-24.04` | zużywa darmowe minuty konta |

## 5. Kontrola wydatków po aktywacji advanced features

| Mechanizm | Teraz | Po aktywacji |
|---|---|---|
| Limit wydatków (spend limit) w AWS Settings | dostępny (stanu nie da się odczytać przez API — `billing:*` zablokowane SCP) | **usunięty, nie da się go utworzyć ponownie** |
| AWS Budgets z alertami e-mail (`create-budget.sh`) | dozwolone (`budgets:*` w us-east-1), brak budżetu | dostępne |
| Budget Actions (np. `STOP_EC2_INSTANCES` przez SSM, lub nałożenie SCP/polityki IAM) | `budgets:CreateBudgetAction` dozwolone | dostępne; wymagają roli wykonawczej dla Budgets |
| Cost Anomaly Detection | dostępne, brak monitorów | dostępne |
| Własne SCP (np. zakaz typów instancji innych niż g4dn) | niedostępne (SCP zarządzane przez AWS) | dostępne |
| Bezpieczniki projektu (A–D, IAM bez `RunInstances`) | wdrażane szablonem | bez zmian |

Dane Budgets są aktualizowane z opóźnieniem (zwykle kilka razy na dobę), więc alerty i
akcje budżetowe nie zastąpią mechanizmów A–D. Te zatrzymują hosta w ciągu minut.

## 6. Checklista przed pierwszym uruchomieniem

| # | Krok | Kto | |
|---|---|---|---|
| 1 | Wniosek o limit GPU zatwierdzony (≥ 4 vCPU) | AWS | ✅ |
| 2 | Advanced features aktywowane (warunek utworzenia dostawcy OIDC) | Ty, AWS Settings | 🧑 |
| 3 | Budżet z alertami: `infra/aws/create-budget.sh <e-mail> 15` — najlepiej przed krokiem 2 | Ty lub ja po zgodzie | 🧑 |
| 4 | Stos `gta20-gfx` (instancja startuje od razu, watchdog wyłączy ją po 60 min) | Ty lub ja po zgodzie | 💰🧑 |
| 5 | Zaraz po kroku 4: zatrzymać instancję albo od razu uruchomić `probe` | — | 💰 |
| 6 | Stos `gta20-github-oidc` | Ty lub ja po zgodzie (po kroku 2) | 🧑 |
| 7 | Gra: zip własnej instalacji GTA 2 10.5 → `s3://gta20-gfx-artifacts-<account-id>/game/gta2-10.5.zip` (prywatny bucket z blokadą dostępu publicznego); SHA-256 → `game.sha256` w macierzy, commit na `main` | Ty | 🧑 |
| 8 | Zmienne repo `AWS_REGION`, `GFX_BUCKET`, `AWS_GFX_ROLE_ARN`, `AWS_GFX_STOP_ROLE_ARN` | ustawione | ✅ |
| 9 | `infra-lint` zielony na `main` | GitHub | ✅ po pushu |
| 10 | `gfx-compat`: `mode=probe`, `max_minutes=45`, `cost_confirmation=uruchom-ec2` | Ty | 💰🧑 |
| 11 | Ocena `environment.json` wg kryteriów probe (pkt 7) | ja | — |
| 12 | `gfx-compat`: `mode=compat`, `cases=all` | Ty | 💰🧑 |

## 7. Kryteria PASS/FAIL

### Probe (`mode=probe`, `overall=PROBE_ONLY`)

| Kryterium | PASS | FAIL → działanie |
|---|---|---|
| Sesja SSM | `Success`, w budżecie czasu | timeout/`Failed` → logi `ssm-output/` |
| Sterownik | `driver.strategy = url`, `attempts[].ok = true` | `none` → sprawdzić podpis/hash, dostęp do `us.download.nvidia.com` |
| Adapter NVIDIA | `Win32_VideoController` z „NVIDIA”, `Status=OK`, `ConfigManagerErrorCode=0` | błąd urządzenia → sterownik/WDDM |
| Tryb sterownika | `nvidia-smi driver_model.current = WDDM` | `TCC` po restarcie → przełączenie nieudane |
| Sesja interaktywna | `interactiveSession` z `SessionId ≥ 1` dla `gta2runner` | brak → autologon (`NO_INTERACTIVE_SESSION`) |
| Zatrzymanie | instancja `stopped` po przebiegu (oba zadania stop zielone) | ręczny stop i analiza |

### Przypadek testowy (`mode=compat`)

| Werdykt | Znaczenie |
|---|---|
| `PASS` | proces żyje po 30 s, ≥ 1 zrzut z ≥ 24 unikalnymi kolorami, ≤ 97% czerni i zmianą ≥ 25% pikseli względem pulpitu bazowego, ostatni zrzut nadal wyrenderowany, tryb zgodny z żądanym |
| `PASS_FELL_BACK_TO_FULLSCREEN` | jak PASS, ale okno nie wyszło i gra przełączyła się na pełny ekran (`start_mode` 0 → 1) |
| `FAIL_PROCESS_EXITED` | proces zakończył się przed końcem obserwacji |
| `FAIL_NO_VISIBLE_CHANGE` | proces żyje, ale ekran nie różni się od pulpitu sprzed startu gry (nic nie narysowano na ekranie) |
| `FAIL_BLANK_SCREEN` | żaden zrzut nie spełnia progów (czarny/pusty ekran) |
| `FAIL_RENDER_STOPPED` | coś wyrenderowano, ale ostatni zrzut jest pusty (zawieszenie, utrata urządzenia) |
| `ERROR*` | błąd infrastruktury testu (brak wrappera, brak `result.json`, timeout przypadku) |

### Wynik całości

| `overall` | Warunek | Decyzja |
|---|---|---|
| `COMPATIBLE` | ≥ 1 przypadek `PASS*` | promocja: `GFX_E2E_CASES=<id PASS>`, `GFX_E2E_ENABLED=true` |
| `INCOMPATIBLE` | 0 przypadków `PASS*`, probe OK | G4dn z tą konfiguracją nie renderuje GTA 2 — analiza zrzutów i dxdiag przed dalszymi kosztami |
| `INFRASTRUCTURE_ERROR` | wszystkie przypadki `ERROR*` | błąd środowiska testu — nie jest wynikiem kompatybilności |
| brak `summary.json` | błąd infrastruktury | nie traktować jako wyniku kompatybilności |

Dodatkowa informacja (nie wpływa na werdykt): `gpuProcessSeen=true` potwierdza, że
`gta2.exe` używał T4, a nie programowego renderowania przez adapter podstawowy.
