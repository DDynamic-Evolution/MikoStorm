# Music Player – Sidepanel-MP3-Player für lokale Dateien

## Übersicht

Ein Sidepanel-Tab zum Abspielen lokaler MP3/WAV/OGG/FLAC-Dateien und Playlists.
Nutzung des FMOD-Streaming-Systems (kein OpenAL-Support). Design angelehnt an Winamp (kompakt).
Als Sidepanel-Tab integriert (wie Inventory, Appearance etc.).

## Architektur

```
FSMusicPlayer (LLSingleton)
├── FSPlaylist (m3u-laden/-speichern, Ordner-Rekursion)
├── FMOD::Sound + FMOD::Channel für aktuellen Titel
├── Timer/Callback für Song-Ende-Erkennung (update() im Main-Loop)
├── Musikstream-Integration (pause/resume bei Parcel-Stream-Änderung)
└── Signale: mStateChangedSignal, mTrackChangedSignal

LLSidepanelMusicPlayer (LLPanel) – Sidepanel-Tab
└── XUI: sidepanel_music_player.xml
```

## Neue Dateien

| Datei | Beschreibung |
|-------|-------------|
| `indra/newview/fsmusicplayer.h` | FSMusicPlayer-Singleton-Klasse (Header) |
| `indra/newview/fsmusicplayer.cpp` | FSMusicPlayer-Implementierung |
| `indra/newview/llsidepaneltabmusicplayer.h` | LLSidepanelMusicPlayer (LLPanel) Header |
| `indra/newview/llsidepaneltabmusicplayer.cpp` | LLSidepanelMusicPlayer-Implementierung |
| `indra/newview/skins/default/xui/en/sidepanel_music_player.xml` | Panel-XUI-Layout |

## Geänderte Dateien

| Datei | Änderung |
|-------|----------|
| `indra/newview/CMakeLists.txt` | +fsmusicplayer.cpp/h +llsidepaneltabmusicplayer.cpp/h; link gegen ll::fmodstudio |
| `indra/newview/llviewerfloaterreg.cpp` | Include + LLFloaterReg::add("music_player", …) |
| `indra/newview/llappviewer.cpp` | FSMusicPlayer::update() im Main-Loop (ca. 100ms) |
| `indra/newview/llvieweraudio.cpp` | start/stopInternetStream → FSMusicPlayer pausieren/resumen |
| `indra/newview/skins/default/xui/en/strings.xml` | Button-Labels, Tooltips |

## FSMusicPlayer-API

```cpp
class FSMusicPlayer : public LLSingleton<FSMusicPlayer>
{
public:
    void playIndex(S32 idx);
    void togglePlayPause();
    void stop();
    void next();
    void prev();

    void setVolume(F32 vol);  // 0..1 (default: 1.0 → regelt FMOD-Channel-Gain)
    F32 getVolume() const;

    // Playlist
    FSPlaylist& getPlaylist();
    void loadM3U(const std::string& path);
    void saveM3U(const std::string& path);
    void addFiles(const std::vector<std::string>& paths);
    void addDirectory(const std::string& path);  // rekursiv *.mp3/*.wav/*.ogg/*.flac

    // Status
    bool isPlaying() const;
    bool isPaused() const;
    std::string getCurrentTitle() const;
    std::string getCurrentArtist() const;
    F64 getCurrentPosition() const;
    F64 getCurrentDuration() const;
    S32 getCurrentIndex() const;

    // Main-Loop-Update (Song-Ende-Erkennung)
    void update();

    // Signale
    boost::signals2::signal<void()> mStateChangedSignal;
    boost::signals2::signal<void()> mTrackChangedSignal;

    // Stream-Suspension
    void suspend();   // pausieren und merken "von Stream unterbrochen"
    void resume();    // fortsetzen
    bool isSuspended() const;

private:
    enum EState { Stopped, Playing, Paused };
    EState mState;
    FMOD::Sound* mCurrentSound;
    FMOD::Channel* mChannel;
    FSPlaylist mPlaylist;
    bool mSuspendedByStream;
};
```

## FSPlaylist-Klasse (innerhalb von fsmusicplayer.h/.cpp oder eigener Datei)

```cpp
struct FSPlaylistEntry {
    std::string mFilePath;
    std::string mTitle;     // aus ID3-Tag oder Dateiname
    std::string mArtist;    // aus ID3-Tag
    F64 mDuration;          // Sekunden
};

class FSPlaylist {
public:
    S32 size() const;
    FSPlaylistEntry& operator[](S32 idx);
    S32 getCurrentIndex() const;
    void setCurrentIndex(S32 idx);

    bool loadM3U(const std::string& path);
    bool saveM3U(const std::string& path) const;

    void addFile(const std::string& path);
    void addFiles(const std::vector<std::string>& paths);
    void addDirectory(const std::string& path);  // rekursiv
    void removeIndex(S32 idx);
    void clear();

    void setShuffle(bool on);
    bool getShuffle() const;
    void setRepeatMode(S32 mode);  // 0=off, 1=all, 2=one
    S32 getRepeatMode() const;

    S32 getNextIndex(bool userInitiated) const;   // Repeat/Shuffle-Logik
    S32 getPrevIndex() const;

private:
    std::vector<FSPlaylistEntry> mEntries;
    S32 mCurrentIndex{-1};
    bool mShuffle{false};
    S32 mRepeatMode{0};  // 0=off, 1=all, 2=one
};
```

## UI-Layout (sidepanel_music_player.xml)

Winamp-kompakt, Sidepanel-typische Breite (~240px):

```
┌──────────────────────────────────┐
│  Music Player                [×] │
├──────────────────────────────────┤
│  01:23 ──────────○──── 04:56     │
│  Song Title - Artist             │
├──────────────────────────────────┤
│  ⏮  ▶  ⏹  ⏭   │ 🔀  🔁       │
│  Volume: ────○───                │
├──────────────────────────────────┤
│  ┌────────────────────────────┐  │
│  │ Song One.mp3               │  │
│  │ Another Song.mp3           │  │
│  │ ...                        │  │
│  └────────────────────────────┘  │
│  [+F] [+O] [−] [Clear]          │
│  [Load M3U] [Save M3U]          │
└──────────────────────────────────┘
```

Details:
- Fortschrittsbalken: `LLSlider` (read-only, klickbar zum Springen)
- Time-Label: `LLTextBox` "MM:SS / MM:SS"
- Titel/Künstler: `LLTextBox` (gekürzt/scrollend bei Überlänge)
- Steuerbuttons: `LLButton` mit Text-Symbolen (▶ ⏸ ⏹ ⏮ ⏭)
- Shuffle/Repeat: `LLButton` toggle (farblich aktiv/inaktiv)
- Volume: `LLSlider` (0-100)
- Playlist: `LLScrollListCtrl` (Textliste, zeilenselektierbar, Doppelklick = abspielen)
- Aktionsbuttons: `LLButton` für +File, +Ordner, −, Clear, Load M3U, Save M3U

## Sidepanel-Tab-Registrierung (`llviewerfloaterreg.cpp`)

```cpp
// Include:
#include "llsidepaneltabmusicplayer.h"

// In registerAllFloaters():
LLFloaterReg::add("music_player", "floater_music_player.xml",
    (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterSidePanelContainer>);
```

Dazu `floater_music_player.xml` mit:
```xml
<floater ... name="music_player" title="Music Player">
    <panel class="sidepanel_music_player" name="main_panel"
           filename="sidepanel_music_player.xml" follows="all" top="0"/>
</floater>
```

## Musikstream-Integration (`llvieweraudio.cpp`)

```cpp
void LLViewerAudio::startInternetStreamWithAutoFade(const std::string& url)
{
    // Wenn lokaler Player aktiv → pausieren
    if (FSMusicPlayer::getInstance()->isPlaying() ||
        FSMusicPlayer::getInstance()->isPaused())
    {
        FSMusicPlayer::getInstance()->suspend();
    }
    // bestehende Logik...
}

void LLViewerAudio::stopInternetStream(const std::string& url)
{
    // bestehende Logik...

    // Wenn lokaler Player suspendiert war → fortsetzen
    if (FSMusicPlayer::getInstance()->isSuspended())
    {
        FSMusicPlayer::getInstance()->resume();
    }
}
```

## Main-Loop-Hook (`llappviewer.cpp`)

In `LLAppViewer::mainLoop()` (oder idle()) in regelmäßigem Abstand:

```cpp
FSMusicPlayer::getInstance()->update();
```

`update()` prüft:
- `mChannel->isPlaying()` → wenn false: nächsten Titel starten (Repeat/Shuffle-Logik)
- Position/Dauer für UI-Updates via Signals

## FMOD-Streaming

```cpp
FMOD_RESULT result = gSystem->createStream(
    filePath.c_str(),
    FMOD_2D | FMOD_NONBLOCKING | FMOD_IGNORETAGS,
    nullptr,
    &mCurrentSound
);
```

Wenn Stream geladen ist:
```cpp
gSystem->playSound(mCurrentSound, nullptr, false, &mChannel);
mChannel->setVolume(mVolume);
mChannel->setPaused(false);
```

Aufräumen:
```cpp
mChannel->stop();
mCurrentSound->release();
```

Metadaten (ID3-Tags) werden nach dem Laden via FMOD-Tag-System ausgelesen:
- `sound->getNumTags()` / `sound->getTag()` für Artist, Title
- Fallback: Dateiname ohne Endung als Title

## Playlist-Persistenz

- Gespeicherte Playlist-Pfad in Settings: `FSMusicPlayerPlaylistPath` (String, leer = keine)
- M3U-Format zeilenweise:
  ```
  #EXTM3U
  #EXTINF:123,Song Title - Artist
  /pfad/zur/datei.mp3
  ```
- Ordner-Rekursion: `addDirectory()` listet rekursiv Dateien mit Endungen .mp3, .wav, .ogg, .flac

## Song-Ende-Logik (Repeat/Shuffle)

| Repeat-Mode | Song zu Ende | Next (user) | Prev (user) |
|-------------|--------------|-------------|-------------|
| Off | Stop | Nächster (oder Stop) | Vorheriger |
| All | Nächster (oder 0) | Nächster (oder 0) | Vorheriger |
| One | Selber nochmal | Nächster (oder 0) | Vorheriger |
| Shuffle+Off | Zufällig | Zufällig | Vorheriger |
| Shuffle+All | Zufällig | Zufällig | Vorheriger |
| Shuffle+One | Selber nochmal | Zufällig | Vorheriger |

## Build-Integration

- Nur mit FMOD Studio (OpenAL hat kein Streaming-Interface)
- Link gegen `ll::fmodstudio`
- In `CMakeLists.txt` mit `if (FMODSTUDIO)` schützen:

```cmake
if (FMODSTUDIO)
    list(APPEND viewer_SOURCE_FILES
        fsmusicplayer.cpp
        llsidepaneltabmusicplayer.cpp
    )
    list(APPEND viewer_HEADER_FILES
        fsmusicplayer.h
        llsidepaneltabmusicplayer.h
    )
    target_link_libraries(${VIEWER_BINARY} ... ll::fmodstudio)
endif()
```

## Offene Punkte (nicht implementiert)

- Equalizer / DSP (später)
- Cover-Art (nicht geplant)
- Hotkeys (nicht geplant)
- Web-Streams / Internet-Radio im Music-Player (keine Konkurrenz zum Parcel-Stream)
- Visualisierung / Spektrumanalyse (nicht geplant)

## Testplan

1. **Playlist-Funktionen:**
   - Einzeldatei hinzufügen und abspielen
   - Ordner (rekursiv) hinzufügen
   - Eintrag löschen, Liste leeren
   - M3U speichern und laden

2. **Wiedergabe:**
   - Play/Pause/Stop/Next/Prev
   - Repeat One → Song wiederholt sich
   - Repeat All → Playlist loopt
   - Shuffle → zufällige Reihenfolge
   - Song-Ende → nächster Titel startet automatisch
   - Fortschrittsbalken und Time-Label aktualisieren sich

3. **Volume:**
   - Volume-Slider im Player funktioniert
   - Music-Volume in den Sound-Preferences funktioniert weiterhin

4. **Musikstream-Interaktion:**
   - Parcel mit Stream → lokale Wiedergabe pausiert
   - Stream stoppt → lokale Wiedergabe setzt fort
   - Kein Stream → lokale Wiedergabe läuft einfach

5. **Sidepanel:**
   - Tab erscheint in der Sidepanel-Leiste
   - Tab öffnet/schließt sich
   - Größe anpassbar (min_width/min_height)
   - Panel bleibt beim Wechsel zwischen Tabs erhalten

6. **Build:**
   - Mit FMOD → Music-Player wird gebaut
   - Ohne FMOD (OpenAL) → Music-Player wird nicht gebaut, kein Link-Error
