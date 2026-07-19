# Build-Optionen

## Basiskommando

```bash
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
cd ~/src/MikoStorm

# Konfigurieren (einmalig, --clean für Komplett-Neubau)
autobuild configure -A 64 -c ReleaseFS_open -- --avx2 --fmodstudio --no-opensim

# Bauen
autobuild build -A 64 -c ReleaseFS_open -- --avx2 --fmodstudio --no-opensim --package
```

## Optionen im Detail

| Flag | Typ | Beschreibung |
|------|-----|-------------|
| `-A <arch>` | Autobuild | Zielarchitektur: `64` (empfohlen) oder `32` (veraltet, nicht unterstützt) |
| `-c <config>` | Autobuild | Build-Konfiguration (siehe Tabelle unten) |
| `--avx` | Viewer | AVX-Optimierungen (mutually exclusive mit `--avx2`) |
| `--avx2` | Viewer | AVX2-Optimierungen (mutually exclusive mit `--avx`) |
| `--fmodstudio` | Viewer | FMOD Studio für Audio (empfohlen, besserer Sound) |
| `--no-opensim` | Viewer | OpenSim-Unterstützung weglassen (für Second Life) |
| `--opensim` | Viewer | OpenSim-Unterstützung inkludieren (für Opensim-Grids) |
| `--package` | Viewer | tar.xz-Archiv und Startverzeichnis erstellen (Default: an) |
| `--clean` | Viewer | Alle gecachten Packages neu laden + komplett neu bauen |
| `--chan "<name>"` | Viewer | Channel-Name → Viewer heißt `Firestorm-<name>` |
| `--espeak` | Viewer | eSpeak-NG TTS einbinden (Default, **nur Linux**) |
| `--no-espeak` | Viewer | eSpeak-NG TTS deaktivieren |
| `-DLL_TESTS:BOOL=FALSE` | CMake | Tests überspringen (baut schneller) |
| `-DPACKAGE:BOOL=OFF` | CMake | Kein Archiv erstellen (nur Binaries) |

> [!NOTE]
> eSpeak-NG wird unter Windows nicht unterstützt. Bei Windows-Builds muss `--no-espeak` verwendet werden.

## Build-Konfigurationen (`-c`)

| Konfiguration | KDU | FMOD | OpenSim | Debug-Info |
|---------------|-----|------|---------|------------|
| `ReleaseFS` | ✓ | ✓ | ✗ | ✗ |
| `ReleaseFS_AVX` | ✓ | ✓ | ✗ | ✗ |
| `ReleaseFS_AVX2` | ✓ | ✓ | ✗ | ✗ |
| `ReleaseFS_open` | ✗ | ✗ | ✗ | ✗ |
| `ReleaseOS` | ✗ | ✗ | ✓ | ✗ |
| `RelWithDebInfoFS` | ✓ | ✓ | ✗ | ✓ |
| `RelWithDebInfoFS_open` | ✗ | ✗ | ✗ | ✓ |
| `RelWithDebInfoOS` | ✗ | ✗ | ✓ | ✓ |

## VSCode-Setup

1. `compile_commands.json` generieren (CMake erzeugt sie automatisch im Build-Ordner)
2. C++ IntelliSense auf GCC-11-Toolchain konfigurieren
3. Build-Task in `.vscode/tasks.json`:

```json
{
    "version": "2.0.0",
    "tasks": [{
        "label": "autobuild",
        "type": "shell",
        "command": "autobuild build -A 64 -c ReleaseFS_open -- --avx2 --fmodstudio --no-opensim --package",
        "options": {
            "env": {
                "AUTOBUILD_VARIABLES_FILE": "${env:HOME}/src/fs-build-variables/variables"
            }
        },
        "group": "build"
    }]
}
```

## Nützliche Umgebungsvariablen

| Variable | Effekt |
|----------|--------|
| `XZ_DEFAULTS="-T0"` | Parallele XZ-Kompression (mehr RAM) |
| `AUTOBUILD_VARIABLES_FILE=<pfad>` | Pfad zur Build-Variablen-Datei |

## Troubleshooting

- **`strip: file format not recognized`**: Harmlose Warnung bei Nicht-Binaries
- **`undefined reference`**: Meist fehlender `#include` oder vergessene Source-Datei in `CMakeLists.txt`
- **SDL2-Probleme**: `rm -rf build-linux-x86_64/packages/include/SDL2/ build-linux-x86_64/packages/lib/release/*SDL*`
