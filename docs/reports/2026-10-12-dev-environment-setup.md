# Raport: przygotowanie środowiska OpenGTA2 (2026-10-12)

## Podsumowanie

* Zainstalowano 3 skille z `github/awesome-copilot` (zakres projektu, Claude Code,
  przypięte do commita `70ddff7f`). Żadnych duplikatów skilli AWS.
* Przygotowano architekturę GitHub Actions + EC2 G4dn + SSM, szablony
  CloudFormation, workflow i osobny test kompatybilności DirectDraw/Direct3D.
* **Nie utworzono ani nie zmieniono żadnych zasobów AWS.** Wszystkie wywołania AWS
  były wyłącznie odczytem (lista niżej).
* Znaleziono trzy blokery po stronie AWS, które wymagają Twojej decyzji, zanim
  cokolwiek wdrożysz: SCP blokuje OIDC, limit vCPU dla GPU = 0, plan FREE.

## 1. GitHub CLI

| Sprawdzenie | Wynik |
|---|---|
| `gh --version` | 2.100.0 (2026-09-03) |
| `gh skill` | dostępne (preview): install, list, preview, publish, search, update |
| `gh auth status` | zalogowano jako `grzebla` (keyring), protokół git: ssh |
| Zakresy tokenu | `admin:public_key`, `gist`, `read:org`, `repo` — **brak `workflow`**. Przy pushu przez SSH nie przeszkadza; potrzebny dopiero przy pushu przez HTTPS albo edycji plików `.github/workflows` przez API (`gh auth refresh -s workflow`). |
| Repozytorium `grzebla/OpenGTA2` | nie istnieje na GitHubie; lokalnie wykonano `git init` (bez remote, bez commitów) |

`git init` był konieczny: `gh skill install --scope project` umieszcza skille
względem katalogu głównego repozytorium git.

## 2–4. Agent Skills

Wyszukiwanie: `gh skill search` (GitHub Actions, gh CLI, C#, .NET, PowerShell,
Windows automation, pywinauto) oraz pełna lista `github/awesome-copilot`
(427 skilli, commit `70ddff7fab9d4d49743d221b56d761cff58048d0`). Przed instalacją
przejrzano `SKILL.md` i wszystkie pliki `references/` (pobrane przez API, porównane
po instalacji — identyczne; brak skryptów, poleceń sieciowych ani podejrzanych instrukcji).

### Zainstalowane (`.claude/skills/`, agent `claude-code`, scope `project`)

| Skill | Dlaczego |
|---|---|
| `github-actions-hardening` | model zagrożeń Actions: wstrzykiwanie `${{ }}`, pinowanie SHA, minimalne `permissions`, OIDC. Bezpośrednio dotyczy workflow sterujących płatnym EC2. |
| `github-actions-efficiency` | minuty CI — istotne przy przyszłej macierzy Windows/Linux/macOS dla OpenGTA2. |
| `csharp-xunit` | standard testów jednostkowych C#/.NET dla reimplementacji. |

Aktualizacja: `gh skill update --all` (skille są przypięte, więc aktualizacja jest świadoma).

### Odrzucone

| Skill | Powód |
|---|---|
| `dotnet-best-practices` | narzuca Semantic Kernel, MSTest+Moq, ResourceManager, Command Handler — szkodliwe dla silnika gry |
| `csharp-async` | ogólnikowe; async w pętli gry zwykle niepożądany |
| `dependabot` | 1300+ linii; konfigurację `.github/dependabot.yml` przygotowano bez niego |
| `codeql`, `secret-scanning` | brak kodu do skanowania; do rozważenia później |
| `aws-cost-optimize`, `aws-resource-health-diagnose`, `aws-cloudwatch-investigation`, `aws-resource-query`, `aws-well-architected-review`, `aws-cdk-python-setup` | duplikaty zainstalowanych skilli AWS (pkt 5) |
| skille gh CLI z innych repo (`trailofbits/skills` i in.) | spoza oficjalnego repo, nie wnoszą nic ponad wbudowaną wiedzę o `gh` |
| Automatyzacja Windows (`windows-desktop-e2e`, `pywinauto`, `windows-automation`) | brak w `awesome-copilot`; zewnętrzne, niezweryfikowane; UI Automation nie widzi kontrolek w pełnoekranowej grze DirectDraw — test opiera się na zrzutach ekranu i analizie pikseli |

## 5. Skille AWS

Zainstalowane wcześniej w zakresie użytkownika (21): `amazon-bedrock`, `aws-ai-ml`,
`aws-auth`, `aws-billing-and-cost-management`, `aws-blocks`, `aws-cdk`,
`aws-cloudformation`, `aws-compute`, `aws-containers`, `aws-database`,
`aws-deployment`, `aws-iam`, `aws-messaging-and-streaming`, `aws-networking`,
`aws-observability`, `aws-sdk-js-v3-usage`, `aws-sdk-python-usage`,
`aws-sdk-swift-usage`, `aws-security`, `aws-serverless`, `aws-storage`
(+ `launch-with-aws`, `signing-in-to-aws`). Pokrywają EC2/SSM, IAM, CloudFormation,
koszty i obserwowalność, więc nic z AWS nie doinstalowano. Skille z rejestru AWS MCP
(np. `aws-compute`) są dostępne na żądanie przez `retrieve_skill`.

## 6–9. Architektura i bezpieczeństwo kosztowe

Szczegóły: [`docs/ci/gpu-testing.md`](../ci/gpu-testing.md). W skrócie:

* GitHub Actions (OIDC, środowisko `aws-gpu` z zatwierdzaniem) startuje **istniejący**
  host `g4dn.xlarge` z Windows Server 2022, steruje nim przez SSM Run Command
  (brak RDP/SSH, brak ruchu przychodzącego), pobiera wyniki z S3 i zatrzymuje host.
* Trzy niezależne bezpieczniki zatrzymania: watchdog w systemie, Lambda-reaper co
  10 min, zadanie `ensure-stopped` (`if: always()`) z rolą tylko do zatrzymywania.
* Rola CI nie może tworzyć, zmieniać typu ani tagów instancji (jawny Deny).

## Stan konta AWS (tylko odczyt)

| Sprawdzenie | Wynik |
|---|---|
| Projekt / rola | konto `433213574600`, `AccountFullAccessRole` |
| Region | eu-north-1 (`~/.aws/config`) — potwierdź w AWS Settings > View all projects > Overview > Additional Info > Region |
| Plan | ~~FREE~~ → **PAID** (przejście wykonane w AWS Settings, potwierdzone `GetAccountPlanState` 2026-10-12) |
| G4dn w eu-north-1 | `g4dn.xlarge`/`2xlarge` w strefach a, b, c (T4 16 GiB) |
| Limit „On-Demand G and VT” | **4 vCPU** (od 2026-10-12 19:43 UTC) — wniosek ID `525751b73cff42dc9ecced945cb3ac3eUu9pcyFI`, zgłoszenie `179182732600608`, status `CASE_CLOSED`, wartość potwierdzona przez `GetServiceQuota` |
| Limit „On-Demand Standard” | 5 vCPU |
| VPC | tylko domyślne `vpc-012515c9ad46fee92` |
| AMI Windows Server 2022 | `ami-02115c83a28601673` (parametr publiczny SSM) |
| Dostawca OIDC | `ListOpenIDConnectProviders` → AccessDenied przez SCP (`iam:*Provider*`) |
| Budżety | brak |
| Cena g4dn.xlarge Windows | 0,742 USD/h; gp3 0,0836 USD/GB-mies. |

Wykonane odczyty: `freetier:GetAccountPlanState`, `sts:GetCallerIdentity`,
`ec2:DescribeInstanceTypeOfferings`, `ec2:DescribeInstanceTypes`, `ec2:DescribeVpcs`,
`servicequotas:GetServiceQuota` ×2, `ssm:GetParameter` ×2, `iam:ListRoles`,
`iam:ListOpenIDConnectProviders` (odmowa), `budgets:DescribeBudgets`, `pricing:GetProducts` ×5.

## Utworzone / zmienione pliki

| Plik | Opis |
|---|---|
| `infra/aws/gfx-test-stack.yaml` | bucket artefaktów, SG bez ruchu przychodzącego, rola + profil instancji, EC2 G4dn z watchdogiem w UserData, Lambda-reaper + harmonogram |
| `infra/aws/github-oidc.yaml` | dostawca OIDC (warunkowo), rola `opengta2-gfx-ci`, rola `opengta2-gfx-ci-stop` |
| `infra/aws/create-budget.sh` | budżet miesięczny z alertami (API Budgets) |
| `scripts/ci/ec2-gpu-session.sh` | resolve/start/upload/run/collect/stop z walidacją wejść |
| `tests/gfx/compat-matrix.json` | macierz testu DirectDraw/Direct3D |
| `tests/gfx/windows/Initialize-GfxHost.ps1` | idempotentny bootstrap: użytkownik + autologon, ACL rejestru, gra z S3, sterownik NVIDIA |
| `tests/gfx/windows/Invoke-GfxCompat.ps1` | orkiestrator (SYSTEM/SSM): probe, przypadki, werdykty, upload |
| `tests/gfx/windows/Run-Gta2Case.ps1` | przypadek w sesji interaktywnej: start gry, zrzuty, analiza pikseli |
| `.github/workflows/gfx-session.yml` | wielokrotnego użytku: sesja + `ensure-stopped` |
| `.github/workflows/gfx-compat.yml` | ręczny test kompatybilności (probe/compat) |
| `.github/workflows/gfx-e2e.yml` | pełna automatyzacja, wyłączona do czasu wyniku `COMPATIBLE` |
| `.github/workflows/infra-lint.yml` | actionlint, shellcheck, cfn-lint, PSScriptAnalyzer, JSON |
| `.github/dependabot.yml` | aktualizacje akcji przypiętych do SHA |
| `.gitignore` | wyklucza pliki gry, wyniki, `.env` |
| `docs/ci/gpu-testing.md` | architektura, IAM, koszty, wdrożenie |
| `CLAUDE.md` | zapisano poziom pomocy LOW |
| `.claude/skills/*` | 3 zainstalowane skille |

Wszystkie akcje w workflow przypięte do pełnych SHA (`actions/checkout` v7.0.1,
`aws-actions/configure-aws-credentials` v6.3.0, `actions/upload-artifact` v7.0.2).

## Walidacja lokalna

| Narzędzie | Wynik |
|---|---|
| actionlint 1.7.12 (+ shellcheck) | 0 problemów |
| shellcheck | 0 problemów |
| cfn-lint 1.57.2 (`--regions eu-north-1`) | 0 problemów |
| PowerShell 7.6.6 parser + PSScriptAnalyzer 1.25.0 | 0 błędów składni, 0 ostrzeżeń |
| `ec2-gpu-session.sh` na atrapie `aws` | wszystkie etapy przechodzą; próby wstrzyknięcia przez `GFX_CASES` i `MAX_MINUTES` odrzucone |

**Nie zweryfikowano** działania na prawdziwym Windows/EC2 — to jest cel pierwszego
uruchomienia `gfx-compat` w trybie `probe`.

## Wymagane uprawnienia i decyzje

| Kto / gdzie | Co | Koszt |
|---|---|---|
| Właściciel projektu, AWS Settings | aktywacja advanced features (odblokowuje `iam:*Provider*` dla OIDC) | 0 |
| Właściciel projektu, AWS Settings | limit wydatków; ewentualnie przejście na plan Paid | — |
| Service Quotas | „Running On-Demand G and VT instances” ≥ 4 vCPU | 0 |
| Wdrażający (rola z prawami CloudFormation + IAM) | `CAPABILITY_IAM` / `CAPABILITY_NAMED_IAM` dla obu stosów | stos hosta 💰 |
| GitHub (admin repo) | utworzenie repo, środowisk `aws-gpu` (recenzenci) i `aws-gpu-stop`, zmiennych | 0 |
| Ty | własna kopia GTA 2 10.5 wgrana do prywatnego bucketu | ~0 |

## Następne kroki

1. ~~Plan Paid~~ (zrobione), ~~wniosek o limit GPU~~ (złożony, PENDING). Pozostaje decyzja o advanced features — aktywacja jest nieodwracalna i usuwa limit wydatków.
2. Utwórz repo `grzebla/OpenGTA2` na GitHubie i wypchnij pliki (nic nie zostało wypchnięte).
3. `infra/aws/create-budget.sh <e-mail> 15` i limit wydatków.
4. 💰 Wdróż `gfx-test-stack.yaml`, następnie `github-oidc.yaml`.
5. Wgraj grę, uzupełnij `game.sha256`, skonfiguruj środowiska i zmienne GitHub.
6. 💰 `gfx-compat` `mode=probe` → oceń sterownik i sesję interaktywną.
7. 💰 `gfx-compat` `mode=compat` → przy `COMPATIBLE` promuj przypadki do `gfx-e2e`.

## Aktualizacja 2026-10-12 (druga runda)

* **Repozytorium:** lokalnie nie było kodu OpenGTA2, historii ani licencji — tylko pliki
  infrastruktury z tej sesji. Pierwszy commit zawiera wyłącznie je. Licencji nie dodano
  (decyzja właściciela). GTA2_RE (`CriminalRETeam/gta2_re`) **nie ma licencji**, więc jego
  kod można traktować tylko jako referencję, bez kopiowania do OpenGTA2.
* **GitHub:** utworzono prywatne `grzebla/OpenGTA2` (ID `1415959822`). Wymagani recenzenci
  środowisk nie są dostępni dla tego planu (HTTP 422) — zaufanie OIDC przypięto do ID
  repozytorium, właściciela i aktora, gałęzi `main` i pliku `gfx-session.yml`; każde ręczne
  uruchomienie wymaga `cost_confirmation=uruchom-ec2`.
* **AWS:** plan PAID (120 USD kredytów). SCP nadal blokuje `iam:CreateOpenIDConnectProvider`
  (symulacja IAM). Limit GPU 0, wniosek `CASE_OPENED` (aktualizacja 19:43 UTC: limit 4 vCPU, zgłoszenie zamknięte).
* **Sterownik:** buckety sterowników AWS (us-east-1) są zablokowane przez SCP; przypięto
  oficjalny NVIDIA 581.80 (R580) dla Windows Server 2022/2025 z SHA-256 i weryfikacją
  podpisu Authenticode; bootstrap przełącza T4 z TCC na WDDM, jeśli trzeba.
* **Poprawki:** reaper ogranicza `MaxRuntimeMinutes` do 15–180; `.gitignore` pomija
  stan Claude Code.
* Checklista i kryteria PASS/FAIL: [`docs/ci/gfx-compat-checklist.md`](../ci/gfx-compat-checklist.md).
