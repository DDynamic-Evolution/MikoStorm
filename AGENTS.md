# MikoStorm

Custom MikoStorm viewer fork. Vollständig vom Firestorm-Upstream abgenabelt.

## Key Changes
- Firestorm/Phoenix-Branding komplett entfernt (UI, Menüs, URLs, Docs, Code-Kommentare)
- `phoenixviewer.com` / `firestormviewer.org` / `wiki.firestormviewer.org` URLs entfernt
- EncroChat komplett entfernt (fsimcipher.cpp/.h, Preferences-Subtab, IM-UI, Settings, Strings)
- 3D Stream (PandaView) via `--no-3dstream` optional deaktivierbar (Default: an)
- `3p.firestormviewer.org` URLs bleiben erhalten (Build-Dependencies)
- **NotecardWriter** — `~Name~Inhalt`-Kommando via `llOwnerSay` → Notecard im Inventar (append/erstellen/rotation)

# Build Instructions (Linux)

## System Requirements
- Ubuntu 22.04 LTS (x86_64), fully upgraded
- 16GB+ RAM, 64GB+ disk, 4+ core CPU
- GCC 11 (default on 22.04), glibc >= 2.34

## One-Time Setup

### Install packages
```bash
sudo apt install libgl1-mesa-dev libglu1-mesa-dev libpulse-dev build-essential \
  python3-pip git libssl-dev libxinerama-dev libxrandr-dev libfontconfig-dev \
  libfreetype6-dev gcc-11 cmake ccache ninja-build
```

### ccache konfigurieren (einmalig)
```bash
ccache --max-size 20G
# Empfohlen: in ~/.bashrc für Dauerhaftigkeit:
# echo 'export CCACHE_MAXSIZE=20G' >> ~/.bashrc
```

## clangd + clang-tidy Setup

`clangd` und `clang-tidy` (LLVM 18) sind installiert. `.clang-tidy` liegt im Projektroot.

### compile_commands.json generieren (für clangd)
```bash
./scripts/configure_firestorm.sh --config --compiler-cache --ninja --avx2 --fmodstudio --no-opensim --vscode
# Erzeugt build-linux-x86_64/compile_commands.json
```

### clang-tidy manuell ausführen
```bash
# Einzelne Datei:
clang-tidy --quiet indra/newview/llversioninfo.cpp 2>/dev/null

# Alle geänderten Dateien (git diff):
git diff --name-only | grep '\.cpp$' | xargs clang-tidy --quiet 2>/dev/null
```

### Clone repos
```bash
cd ~/src
git clone https://github.com/DDynamic-Evolution/phoenix-firestorm.git
git clone https://github.com/FirestormViewer/fs-build-variables.git
```

### Install autobuild
```bash
pip install -r requirements.txt
autobuild --version  # should be >= 3.9.3
```

### FMOD Studio (optional but recommended)
```bash
git clone https://github.com/FirestormViewer/3p-fmodstudio.git
cd ~/src/3p-fmodstudio
# Check FMOD_VERSION in build-cmd.sh, download matching Linux FMOD Studio API
# from https://fmod.com (free account required, NOT the FMOD Studio Tool)
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
autobuild build -A 64 --all
autobuild package -A 64 --results-file result.txt
# Copy hash + path from output, then register it:
cd ~/src/MikoStorm
autobuild installables edit fmodstudio platform=linux64 hash=<md5> url=file:///<path-to-tar.bz2>
```

## Build (empfohlen: Build-Script mit ccache + Ninja)

```bash
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
cd ~/src/MikoStorm

# Einmalig konfigurieren (--clean zum Neuladen aller Pakete)
./scripts/configure_firestorm.sh --config --compiler-cache --ninja --avx2 --fmodstudio --no-opensim

# Danach immer nur noch bauen:
./scripts/configure_firestorm.sh --build --compiler-cache --ninja
```

Das Build-Script liegt unter `scripts/configure_firestorm.sh`. Alle Optionen: `--help`.

### Quick Build (ohne KDU, ohne Package, ohne Script)
```bash
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
cd ~/src/MikoStorm

autobuild configure -A 64 -c ReleaseFS_open -- --avx2 --fmodstudio --no-opensim \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

autobuild build -A 64 -c ReleaseFS_open -- --avx2 --fmodstudio --no-opensim \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

### Full Build (KDU, FMOD, packaged)
```bash
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
cd ~/src/MikoStorm

./scripts/configure_firestorm.sh --config --compiler-cache --ninja --avx2 \
  --fmodstudio --kdu --package --no-opensim \
  --chan "Release" --btype Release

./scripts/configure_firestorm.sh --build --compiler-cache --ninja
```

### Build targets
| Target | Description |
|--------|-------------|
| `ReleaseFS` | KDU + FMOD , no OpenSim |
| `ReleaseFS_AVX` | + AVX optimizations |
| `ReleaseFS_AVX2` | + AVX2 optimizations |
| `ReleaseFS_open` | no KDU, no FMOD, no OpenSim |
| `ReleaseOS` | no KDU, no FMOD, with OpenSim |

### Useful switches
- `--chan "MyBuild"` → channel becomes `MikoStorm-MyBuild`
- `--clean` → force re-download all packages
- `--compiler-cache` → aktiviert ccache (deutlich schnellere Folgebuilds)
- `--ninja` → verwendet Ninja statt Make (schneller, bessere Parallelisierung)
- `--package` → package the build after compiling (creates tarball)
- `--3dstream` / `--no-3dstream` → 3D Stream (PandaView) ein-/ausschließen (Default: an)
- `export XZ_DEFAULTS="-T0"` → parallel compression (more RAM)

## Run
```bash
cd ~/src/MikoStorm/build-linux-x86_64/newview/packaged
./mikostorm
```

## Troubleshooting
- **Out of memory**: add swap (~10GB) or reduce CPU cores
- **SDL2 errors**: `rm -rf build-linux-x86_64/packages/include/SDL2/ build-linux-x86_64/packages/lib/release/*SDL*`
- **No sound**: build with `--fmodstudio` instead of OpenAL
- **ccache keine Wirkung**: `ccache -s` zeigt Statistik. Nach erstem Build sollten Treffer > 0 sein.

