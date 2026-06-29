# MikoStorm Phoenix Firestorm

Custom Phoenix Firestorm viewer fork.

## Key Changes
- Server identification: `getServerChannel()` → `"Firestorm-Releasex64"` (für LL), UI zeigt `"MikoStorm-Release"` (Channel-Name)
- EncroChat komplett entfernt (fsimcipher.cpp/.h, Preferences-Subtab, IM-UI, Settings, Strings)

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
  libfreetype6-dev gcc-11 cmake
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
cd ~/src/phoenix-firestorm
autobuild installables edit fmodstudio platform=linux64 hash=<md5> url=file:///<path-to-tar.bz2>
```

## Quick Build (no KDU, no package)

```bash
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
cd ~/src/phoenix-firestorm

# Configure (one time, re-run with --clean to force re-download packages)
autobuild configure -A 64 -c ReleaseFS_open -- --avx2 --fmodstudio --no-opensim

# Build
autobuild build -A 64 -c ReleaseFS_open -- --avx2 --fmodstudio --no-opensim
```

## Full Build (KDU, FMOD, packaged)

```bash
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
cd ~/src/phoenix-firestorm

# Configure (one time, re-run with --clean to force re-download)
autobuild configure -A 64 -c ReleaseFS -- --avx2 --fmodstudio --no-opensim -DLL_TESTS:BOOL=FALSE -DVIEWER_CHANNEL:STRING="MikoStorm-Release"

# Build & package
autobuild build -A 64 -c ReleaseFS -- --avx2 --fmodstudio --no-opensim -DLL_TESTS:BOOL=FALSE
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
- `-DLL_TESTS:BOOL=FALSE` → skip tests (faster)
- `--package` → package the build after compiling (creates tarball)
- `export XZ_DEFAULTS="-T0"` → parallel compression (more RAM)

## Run
```bash
cd ~/src/phoenix-firestorm/build-linux-x86_64/newview/packaged
./mikostorm
```

## Troubleshooting
- **Out of memory**: add swap (~10GB) or reduce CPU cores
- **SDL2 errors**: `rm -rf build-linux-x86_64/packages/include/SDL2/ build-linux-x86_64/packages/lib/release/*SDL*`
- **No sound**: build with `--fmodstudio` instead of OpenAL

