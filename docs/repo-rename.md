# Zmiana nazwy: OpenGTA2 → GTA2.0 (`grzebla/gta2.0`)

Zatwierdzona przez właściciela 2026-10-12. Zmieniono nazwę **istniejącego** repozytorium
(`gh repo rename`), więc historia, gałęzie, PR #2/#3 i ID repozytorium (`1415959822`) zostały
zachowane. GitHub przekierowuje stare URL-e `grzebla/OpenGTA2`.

| Element | Przed | Po |
|---|---|---|
| Projekt | OpenGTA2 / gta2-native | **GTA2.0** |
| Repozytorium | `grzebla/OpenGTA2` | `grzebla/gta2.0` |
| Namespace C++, katalogi `include/` | `g2n` | `gta20` |
| Cele CMake / makra / opcje | `g2n_*`, `G2N_*` | `gta20_*`, `GTA20_*` |
| Projekt CMake | `gta2_native` | `GTA20` |
| Aplikacja | `gta2n` (`apps/gta2n`) | `gta2` (`apps/gta2`) |
| Runner bez okna | `g2n-headless` | `gta20-headless` |
| Dane gry | `G2N_GAME_DIR`, `G2N_GAME_DIR_105` | `GTA20_GAME_DIR`, `GTA20_GAME_DIR_105` (stare nazwy nadal działają jako zapasowe) |
| AWS: stosy, role, bucket, budżet, tag | `opengta2-*`, `Project=OpenGTA2`, `/opengta2/`, `C:\OpenGTA2` | `gta20-*`, `Project=GTA20`, `/gta20/`, `C:\GTA20` |
| OIDC `GitHubRepo` (warunek `sub`, `job_workflow_ref`) | `OpenGTA2` | `gta2.0` |
| Zmienne repozytorium `AWS_GFX_*_ROLE_ARN`, `GFX_BUCKET` | `opengta2…` | `gta20…` |

Zasoby AWS **nie były wdrożone** w chwili zmiany (sprawdzone odczytowo: brak stosów,
bucketów, instancji, ról i budżetów), więc nazwy w szablonach zmieniono bez migracji.
Zabezpieczenia są identyczne: warunki zaufania OIDC nadal przypinają niezmienne
`repository_id`, `repository_owner_id`, `actor_id`, gałąź `main` i plik workflow.

Bez zmian (historia): `docs/archive/`, datowane raporty w `docs/reports/`, gałąź
`integrate/opengta2-upstream`, remote `upstream` (ikkentim/opengta2) i wzmianki o tym, że
projekt **nie** korzysta z upstreamowego OpenGTA2 (C#).

Lokalny katalog roboczy może pozostać `~/Projects/OpenGTA2` (ścieżka nie jest nigdzie
zapisana na stałe); zmiana nazwy katalogu jest opcjonalna.
