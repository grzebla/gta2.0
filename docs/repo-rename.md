# Zmiana nazwy repozytorium na `grzebla/gta2-native` — analiza wpływu

**Status:** propozycja. Zdalne repozytorium **nie** zostało przemianowane (wymaga Twojej zgody).
Lokalnie migracja kodu jest niezależna od nazwy.

| Obszar | Wpływ | Działanie |
|---|---|---|
| URL-e GitHub, `git remote` | GitHub przekierowuje stare URL-e (HTTP i git) | opcjonalnie `git remote set-url origin git@github.com:grzebla/gta2-native.git` |
| ID repozytorium (`1415959822`), właściciela, aktora | **bez zmian** (ID są niezmienne) | — |
| OIDC: warunek `sub` = `repo:grzebla/OpenGTA2:ref:refs/heads/main` | **zmienia się** na `repo:grzebla/gta2-native:…` | zmienić parametr `GitHubRepo` w `infra/aws/github-oidc.yaml` (stos nie jest jeszcze wdrożony, więc tylko domyślna wartość) |
| OIDC: `job_workflow_ref` = `grzebla/OpenGTA2/.github/workflows/gfx-session.yml@…` | **zmienia się** | j.w. (wyliczany z `GitHubOwner`/`GitHubRepo`) |
| IAM: nazwy ról `opengta2-gfx-ci*`, tag `Project=OpenGTA2`, stos `opengta2-gfx`, bucket `opengta2-gfx-artifacts-<konto>` | niezależne od nazwy repo | można zostawić; ewentualna zmiana na `gta2-native` przed pierwszym wdrożeniem (później wymaga odtworzenia zasobów) |
| Zmienne repozytorium (`AWS_REGION`, `GFX_BUCKET`, ARN-y ról) | zachowane przy zmianie nazwy | — |
| Workflow | nie zawierają nazwy repo | — |
| Dokumentacja | odwołania do `grzebla/OpenGTA2` | aktualizacja tekstów |
| Lokalny katalog `~/Projects/OpenGTA2` | bez wpływu | opcjonalnie zmiana nazwy katalogu |

Kolejność po zatwierdzeniu: (1) zmiana nazwy w GitHub (Settings → Rename), (2) zmiana
domyślnej wartości `GitHubRepo` + dokumentacji w jednym PR, (3) dopiero potem wdrożenie
stosu OIDC. Jeśli stos OIDC zostanie wdrożony przed zmianą nazwy, trzeba go zaktualizować
(`aws cloudformation deploy … --parameter-overrides GitHubRepo=gta2-native`), inaczej
zatwierdzone uruchomienia GPU nie dostaną poświadczeń.
