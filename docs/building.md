# Budowanie

Wymagania wspólne: CMake ≥ 3.25, Ninja, kompilator C++20, dostęp do sieci przy pierwszej
konfiguracji (zależności pobierane i weryfikowane SHA-256 do `.deps/`).

| Preset | Co buduje | Uwagi |
|---|---|---|
| `debug` | wszystko + testy | |
| `release` | wszystko + testy | |
| `asan` | wszystko + testy, ASan + UBSan | Clang/GCC (macOS, Linux) |
| `headless` | bez SDL: core, assets, sim, scene, testing, g2n-headless, testy | nie wymaga ekranu ani GPU |

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/bin/gta2n                     # okno; strzałki/WASD, kółko = zoom, Esc = wyjście
./build/release/bin/gta2n --offscreen 960x540 --screenshot shot.bmp
```

## macOS (Apple Silicon)

* Xcode Command Line Tools (Apple Clang), `brew install cmake ninja`.
* Backend: Metal. Shadery MSL kompiluje sterownik w czasie działania (narzędzie `metal`
  nie jest potrzebne).
* Zweryfikowano lokalnie 2026-10-12: macOS 27.0.1, Apple Clang 21, CMake 4.1.1, Ninja 1.13.2.
  Debug, Release, ASan/UBSan i headless budują się bez ostrzeżeń; 43/43 testów przechodzi;
  okno i render offscreen działają przez Metal.

## Windows x64

* Visual Studio 2022 lub Build Tools (MSVC), Windows SDK (dostarcza `dxc.exe`/`fxc.exe`), CMake, Ninja.
* Uruchom z „x64 Native Tools Command Prompt” (albo `vcvars64.bat`), potem presety jak wyżej.
* Backend: Direct3D 12. Shadery HLSL są kompilowane przy buildzie: DXIL przez `dxc` albo,
  w razie jego braku, DXBC SM5.1 przez `fxc`. Bez obu kompilatorów SDL_GPU spróbuje Vulkana.
* Status: budowane w CI (`windows-2025`). Uruchomienie okna na Windows nie było jeszcze
  ręcznie sprawdzone.

## Linux x64

```bash
sudo apt-get install ninja-build clang libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxi-dev libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev
CC=clang CXX=clang++ cmake --preset release
```

* Backend: Vulkan (sterownik GPU albo Mesa lavapipe: `mesa-vulkan-drivers`).
* Bez X11/Wayland (`DISPLAY` i `WAYLAND_DISPLAY` puste) tryb `--offscreen` sam przełącza
  SDL na wirtualny sterownik wideo `offscreen`.
* Status: budowane i testowane w CI (`ubuntu-24.04`, Clang); render offscreen przez lavapipe
  jako zadanie próbne.

## Shadery

| Backend | Plik | Generowanie |
|---|---|---|
| Vulkan | `engine/render/shaders/quad.{vert,frag}.glsl` → `spirv/*.spv` (commitowane) | `scripts/shaders/compile-spirv.sh <glslang 16.6.0>` |
| Metal | `engine/render/shaders/quad.metal` | brak (źródło osadzane w binarce) |
| D3D12 | `engine/render/shaders/quad.hlsl` | automatycznie przy buildzie na Windows |

Po zmianie GLSL uruchom skrypt i zacommituj `.spv` — CI sprawdza, czy pliki są aktualne.

## Dane gry

Repozytorium nie zawiera żadnych plików GTA 2. Przygotowanie lokalnej instalacji z
oryginalnych instalatorów (bez ich uruchamiania): [`tools/gta2-install/README.md`](../tools/gta2-install/README.md).
Zweryfikowane: wydanie 9.6 (Rockstar 2004, kompletne) i 10.5 (Freeloader, bez muzyki i filmów) mają
identyczne mapy i style. Aby użyć instalacji:

```bash
export G2N_GAME_DIR=/ścieżka/do/GTA2
./build/release/bin/gta2n --level data/wil.gmp     # wil/ste/bil; styl .sty dobierany automatycznie
./build/release/bin/gta2n --level data/ste.gmp --camera 100,60 --zoom 12   # Tab = kamera swobodna
ctest --preset release                             # włącza testy [gamedata] i gamedata.render.*
G2N_GAME_DIR_105=~/Games/GTA2-10.5 ctest ...        # dodatkowo porównanie danych 9.6 i 10.5
```
