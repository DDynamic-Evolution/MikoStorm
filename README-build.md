# MikoStorm bauen (Linux)

## Voraussetzungen

- Ubuntu 22.04 LTS (x86_64), vollständig geupdated
- 16 GB+ RAM, 64 GB+ Festplatte, 4+ Kerne
- GCC 11 (default auf 22.04), glibc >= 2.34

## Einmalige Einrichtung

### Pakete installieren

```bash
sudo apt install libgl1-mesa-dev libglu1-mesa-dev libpulse-dev build-essential \
  python3-pip git libssl-dev libxinerama-dev libxrandr-dev libfontconfig-dev \
  libfreetype6-dev gcc-11 cmake
```

### Repos klonen

```bash
cd ~/src
git clone https://github.com/DDynamic-Evolution/phoenix-firestorm.git
git clone https://github.com/FirestormViewer/fs-build-variables.git
```

### autobuild installieren

```bash
pip install -r ~/src/phoenix-firestorm/requirements.txt
autobuild --version   # sollte >= 3.9.3 sein
```

### FMOD Studio (optional, aber empfohlen)

```bash
git clone https://github.com/FirestormViewer/3p-fmodstudio.git ~/src/3p-fmodstudio
cd ~/src/3p-fmodstudio
# FMOD_VERSION aus build-cmd.sh ablesen, passende Linux API von
# https://fmod.com herunterladen (kostenloser Account nötig)
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
autobuild build -A 64 --all
autobuild package -A 64 --results-file result.txt
# Hash + Pfad aus der Ausgabe kopieren, dann registrieren:
cd ~/src/phoenix-firestorm
autobuild installables edit fmodstudio platform=linux64 hash=<md5> url=file:///<pfad-zum-tar.bz2>
```

## Bauen

```bash
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
export XZ_DEFAULTS="-T0"   # parallele Kompression (mehr RAM)
cd ~/src/phoenix-firestorm
```

### Konfigurieren (einmalig, `--clean` für frischen Download)

```bash
autobuild configure -A 64 -c ReleaseFS_open -- --clean --avx2 --fmodstudio \
  --no-opensim --chan="Release" -DLL_TESTS:BOOL=FALSE
```

### Bauen und Paketieren

```bash
autobuild build -A 64 -c ReleaseFS_open -- --avx2 --fmodstudio \
  --no-opensim --chan="Release" -DLL_TESTS:BOOL=FALSE
```

**Erklärung der Optionen:**

| Option | Bedeutung |
|--------|-----------|
| `-A 64` | 64-Bit Build |
| `-c ReleaseFS_open` | Build-Ziel: ohne KDU, ohne OpenSim, ohne FMOD (Basis) |
| `--avx2` | AVX2-Optimierungen aktivieren |
| `--fmodstudio` | FMOD Studio Sound aktivieren |
| `--no-opensim` | OpenSim-Unterstützung deaktivieren |
| `--chan="Release"` | Channel-Name → wird zu `MikoStorm-Release` |
| `--clean` | Alte Build-Artefakte löschen vor Konfiguration |
| `-DLL_TESTS:BOOL=FALSE` | Tests überspringen (schneller) |
| `XZ_DEFAULTS="-T0"` | Alle CPU-Kerne fürs Packen nutzen |

### Ohne FMOD bauen (OpenAL)

```bash
autobuild configure -A 64 -c ReleaseFS_open -- --avx2 --no-opensim --chan="Release"
```

### Mit KDU bauen (JPEG2000, proprietär)

```bash
autobuild configure -A 64 -c ReleaseFS_AVX2 -- --fmodstudio --no-opensim --chan="Release"
```

## Ausführen

```bash
cd ~/src/phoenix-firestorm/build-linux-x86_64/newview/packaged
./mikostorm
```

## Fehlersuche

- **Nicht genug RAM:** Swap vergrößern (~10 GB) oder CPU-Kerne reduzieren
- **SDL2-Fehler:** `rm -rf build-linux-x86_64/packages/include/SDL2/ build-linux-x86_64/packages/lib/release/*SDL*`
- **Kein Ton:** Mit `--fmodstudio` statt OpenAL bauen
