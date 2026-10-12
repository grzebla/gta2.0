# Raport: Orchestrator GTA2.0 (Astra + Claude) — 2026-10-12

## 1. Narzędzia

| Narzędzie | Wersja | Stan |
|---|---|---|
| Claude Code | 2.1.296 | logowanie `claude.ai` (subskrypcja), `apiKeySource=none` |
| Codex CLI | 0.160.1 | logowanie kontem ChatGPT (plan `prolite`) |
| GitHub CLI | 2.100.0 | zalogowany |
| AWS CLI | 2.37.12 | operacje AWS wyłącznie przez AWS MCP, tylko odczyt |
| Python | 3.9.6 (systemowy) | Orchestrator używa tylko biblioteki standardowej |

### Astra (`gpt-6-astra`)

Sprawdzona rzeczywistym zapytaniem — odpowiedź `ASTRA_OK`.
- Model potwierdzony w zapisie sesji: `turn_context.model = gpt-6-astra`.
- Nieistniejąca nazwa modelu daje błąd 400. Codex po cichu nie podmienia modelu.

### Claude Code w trybie nieinteraktywnym

Sprawdzone:
- JSON i `--json-schema` (`structured_output`);
- `--resume` (sesja pamięta kontekst);
- zdarzenie `rate_limit_event` z oknami `five_hour` i `seven_day` (`status`, `resetsAt`).

Pułapka: opcja `--tools` jest wariadyczna i „zjada” prompt podany jako argument, dlatego
prompt idzie przez stdin.

## 2. Zaimplementowane elementy

| Element | Gdzie |
|---|---|
| Orchestrator (pętla, stany, limity pętli) | `tools/orchestrator/gta20_orchestrator/loop.py` |
| Adapter Astry (`codex exec`, read-only, schemat JSON, weryfikacja modelu, limity z rollout) | `adapters/codex.py` |
| Adapter Claude (`claude -p`, `dontAsk`, allow/deny, sandbox Basha, `--session-id`/`--resume`, limity) | `adapters/claude.py` |
| Trwała pamięć: SQLite (WAL) + append-only `events` (triggery) + `events.jsonl` | `db.py` |
| Pamięć czytelna: `docs/agent-memory/*.md` (stan zastępowany przez Astrę, dzienniki dopisywane) | `memory.py` |
| Niezależna weryfikacja (te same bramki co CI, plus ochrona ścieżek, danych i sekretów) | `verify.py`, `toolchain.py` |
| CLI: doctor, start, pause, resume, stop, status, logs, memory, tasks, dry-run, report, unblock, answer, service, simulate | `cli.py` |
| launchd (kopia przypięta poza repo, KeepAlive przy awarii, ochrona przed pętlą awarii) | `service.py` |
| Raport Markdown/HTML odświeżany po każdym kroku | `report.py` → `.orchestrator/report.{md,html}` |
| Atrapy agentów i symulacja | `adapters/fake.py`, `sim.py` |
| Dokumentacja | `tools/orchestrator/README.md` |

## 3. Co przetestowano naprawdę, a co tylko przygotowano

| Mechanizm | Stan | Dowód |
|---|---|---|
| Wywołanie Astry i weryfikacja modelu | **przetestowane na żywo** | 6 przebiegów, każdy z `gpt-6-astra` w rollout |
| Wywołanie Claude, model, subskrypcja | **na żywo** | `claude-opus-5-5`, `apiKeySource=none` |
| Uprawnienia i sandbox Claude | **na żywo** (`doctor --live`) | zapis do ścieżki chronionej → deny; `touch ~/…` → „Operation not permitted”; sieć poza allowlistą → wyjątek; `git push` → deny; zapis w worktree → OK |
| Iteracja Astra → Claude → weryfikacja → Astra → pamięć | **na żywo** | T0001: plan → implementacja (4,5 min) → weryfikacja → **BLOCKED** (słusznie: brakowało testów bez danych i formatowania) → poprawka Orchestratora → `unblock` → wznowienie sesji → **APPROVED** → merge, push, draft PR #6 |
| Pętla poprawek | **na żywo** | T0002: dwa razy `CHANGES_REQUESTED` z konkretnymi uwagami (m.in. Claude zaproponował przechwytywanie na EC2 i bucket wbrew polityce; Astra to wychwyciła) |
| Przerwanie i wznowienie (`stop --now`) | **na żywo** | przebieg 9 zapisany jako `interrupted`; po restarcie przebieg 10 wznowił **tę samą sesję** (`--resume b10b2e8e…`) z istniejącym commitem; bez duplikatu |
| Twardy crash (SIGKILL) i wznowienie bez duplikatów | atrapy (proces rzeczywisty) | test `test_hard_kill_mid_run_resumes_without_duplicating_work` |
| Limit Claude z czasem odnowienia, próba, wznowienie sesji | atrapy | `quota_known_reset` |
| Limit bez czasu odnowienia (backoff) | atrapy | `quota_unknown_reset` |
| Limit Astry w trakcie recenzji | atrapy | `astra_quota_in_review` |
| Rozpoznawanie formatu limitu Claude/Codex | testy jednostkowe na strukturach zaobserwowanych na żywo | prawdziwy limit nie był celowo wyczerpywany |
| Ścieżki chronione, brak postępu, `NEEDS_HUMAN`, wszystko zablokowane, pauza/stop | atrapy | 8 scenariuszy + testy jednostkowe |
| launchd: start, crash → restart, czysty stop bez restartu | **na żywo** (osobna etykieta, atrapy agentów) | `launchctl` bootstrap; SIGKILL → nowy PID; stop → kod 0, brak restartu |
| Usługa produkcyjna | **przygotowana, NIE uruchomiona** | plist zainstalowany, stan `STOPPED` |
| Wielodniowa praca | **nieuruchomiona** (wymaga Twojej zgody) | — |
| Migracja na maszynę 24/7 | tylko opisana | README |
| CI | **zielone** | PR #4, #5, #6 (w tym nowe zadanie `orchestrator-tests`) |

Testy Orchestratora: 30/30 lokalnie (Python 3.9) i w CI (Python 3.12).

## 4. Wnioski z prawdziwej iteracji

1. **Astra działa jak nadzorca, nie stempel.**
   - Odmówiła zatwierdzenia T0001: weryfikacja Orchestratora nie obejmowała testów bez danych
     ani formatowania, a raport Claude był „partial”.
   - Po poprawce zatwierdziła zadanie, odhaczając każde kryterium z dowodem.
2. **Luka wykryta przez system.** Weryfikacja uruchamia teraz te same bramki co CI:
   - clang-format 23.1.3;
   - granice modułów;
   - `headless` bez `GTA20_GAME_DIR`;
   - `debug` z danymi.

   Orchestrator sam instaluje przypięty clang-format (sandbox Claude nie może pobierać
   pakietów).
3. **Ograniczenie sandboxa:** Claude nie uruchomi testów renderu z oknem (brak ekranu w
   sandboxie). Weryfikacja Orchestratora działa poza sandboxem i je uruchamia.
4. **Astra zgłosiła realne problemy projektu:**
   - nieaktualny komentarz o CMAP w `gmp_map.hpp`;
   - test smoke Vulkana nie blokuje scalenia;
   - obrazy z oryginalnych danych zapisywane w `build/` wewnątrz checkoutu (zadanie T0003).

## 5. Pamięć i plan M1

- `docs/agent-memory/ROADMAP.md` na `agents/integration` zawiera rozpisany przez Astrę plan M1:
  24 uporządkowane kroki z bramkami.
  - Kolejność kroków: ściany jednostkowe → konwencje → UV → siatka → wielopoziomowość →
    projekcja → shadery → głębia → przezroczystość → aplikacja → skosy (4 kroki) → bramka
    17 map → porównanie z oryginałem → dzielnice → benchmarki → cache CPU/GPU → widoczność →
    progi regresji → audyt.
  - Kryteria milestone'u są w bazie (`milestones`).
- Stan tablicy:

  | Zadanie | Stan |
  |---|---|
  | T0001 | DONE |
  | T0002 | CHANGES_REQUESTED, próba 2 z 3 |
  | T0003 | READY |

## 6. Zmiana nazwy (PR #4)

`grzebla/OpenGTA2` → `grzebla/gta2.0`, zmienione w miejscu (to samo ID repozytorium, PR-y i
historia zachowane). Szczegóły w `docs/repo-rename.md`.
- **Kod:** `gta20`, `gta2`, `gta20-headless`, CMake `GTA20`; stare `G2N_GAME_DIR` działa dalej.
- **AWS:** szablony `gta20-*` (nic nie było wdrożone); OIDC `GitHubRepo=gta2.0`.
- **GitHub:** zmienne repozytorium zaktualizowane; remote `origin` wskazuje nowy adres.

## 7. AWS (tylko odczyt)

- Limit „Running On-Demand G and VT” w eu-north-1 wynosi **4 vCPU**. Wniosek
  179182732600608 jest `CASE_CLOSED` (zamknięty 2026-10-12 19:43 UTC).
- Brak stosów, bucketów, instancji, budżetów i ról projektu.
- SCP blokuje listowanie dostawców OIDC oraz CloudFormation w us-east-1.
- Nic nie zostało utworzone ani zmienione.

## 8. Uruchomienie pracy autonomicznej (decyzja właściciela)

```bash
gta20-orchestrator doctor --live   # opcjonalnie
gta20-orchestrator start           # launchd: start teraz i przy każdym logowaniu
gta20-orchestrator status          # albo .orchestrator/report.html
gta20-orchestrator stop --unload   # zatrzymanie
```

Koszty i ryzyka do akceptacji:
- zużycie limitów subskrypcji Claude i ChatGPT;
- minuty GitHub Actions (push gałęzi integracyjnej najwyżej co 6 h);
- praca tylko przy zalogowanym, nieuśpionym Macu.
