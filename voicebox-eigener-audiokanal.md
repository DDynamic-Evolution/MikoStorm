# Voicebox TTS: Eigener Audiokanal

Voicebox TTS bekommt einen eigenen Audiokanal (`AUDIO_TYPE_VOICEBOX`) mit
eigenem Lautstärkeregler und Stummschaltung, zugänglich über:

- Schnelleinstellungen (Volume-Pulldown)
- Systemeinstellungen (Sound-Tab)
- Voicebox-TTS-Einstellungen (direkt im Panel)

---

## 1. Neuer Enum-Wert in `llaudioengine.h`

**Datei:** `indra/llaudio/llaudioengine.h`, Zeile ~85

```cpp
enum LLAudioType
{
    AUDIO_TYPE_NONE    = 0,
    AUDIO_TYPE_SFX     = 1,
    AUDIO_TYPE_UI      = 2,
    AUDIO_TYPE_AMBIENT = 3,
    AUDIO_TYPE_VOICEBOX = 4, // NEU: Voicebox TTS
    AUDIO_TYPE_COUNT   = 5    // war 4
};
```

---

## 2. Neue Settings in `settings.xml`

**Datei:** `indra/newview/app_settings/settings.xml`

### AudioLevelVoiceBox (nach `AudioLevelVoice`, Zeile ~1995)

```xml
    <key>AudioLevelVoiceBox</key>
    <map>
      <key>Comment</key>
      <string>Audio level of VoiceBox TTS output</string>
      <key>Persist</key>
      <integer>1</integer>
      <key>Type</key>
      <string>F32</string>
      <key>Value</key>
      <real>0.5</real>
    </map>
```

### MuteVoiceBox (nach `MuteVoice`, Zeile ~8883)

```xml
    <key>MuteVoiceBox</key>
    <map>
      <key>Comment</key>
      <string>VoiceBox TTS plays at 0 volume</string>
      <key>Persist</key>
      <integer>1</integer>
      <key>Type</key>
      <string>Boolean</string>
      <key>Value</key>
      <integer>0</integer>
    </map>
```

---

## 3. `triggerSound` in `fschattts.cpp` anpassen

**Datei:** `indra/newview/fschattts.cpp`, Zeile ~337

```cpp
// ALT:
gAudiop->triggerSound(audio_uuid, gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI);

// NEU:
gAudiop->triggerSound(audio_uuid, gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_VOICEBOX);
```

---

## 4. Volume-Handling in `llvieweraudio.cpp` ergänzen

**Datei:** `indra/newview/llvieweraudio.cpp`, nach Zeile ~517 (nach Ambient)

```cpp
static LLCachedControl<bool> mute_voicebox(gSavedSettings, "MuteVoiceBox");
static LLCachedControl<F32> al_voicebox(gSavedSettings, "AudioLevelVoiceBox");
gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_VOICEBOX,
                          mute_voicebox() ? 0.f : al_voicebox());
```

---

## 5. Signal-Listener in `llviewercontrol.cpp` registrieren

**Datei:** `indra/newview/llviewercontrol.cpp`, nach Zeile ~1383

```cpp
setting_setup_signal_listener(gSavedSettings, "AudioLevelVoiceBox", handleAudioVolumeChanged);
setting_setup_signal_listener(gSavedSettings, "MuteVoiceBox", handleAudioVolumeChanged);
```

---

## 6. Slider in Schnelleinstellungen (`panel_volume_pulldown.xml`)

**Datei:** `indra/newview/skins/default/xui/en/panel_volume_pulldown.xml`
Einfügen nach dem Voice-Block (~Zeile 346), vor `</panel>`.

```xml
    <slider
     name="VoiceBox TTS Volume"
     label="VoiceBox TTS"
     top_pad="4"
     left="10"
     height="15"
     width="160"
     follows="left|top"
     layout="topleft"
     control_name="AudioLevelVoiceBox"
     disabled_control="MuteAudio"
     increment="0.025"
     initial_value="0.5"
     label_width="80"
     show_text="false"
     volume="true"
    >
        <slider.commit_callback
         function="Pref.setControlFalse"
         parameter="MuteVoiceBox"
        />
    </slider>
    <button
     name="mute_voicebox"
     left_pad="5"
     height="16"
     width="16"
     follows="top|left"
     layout="topleft"
     control_name="MuteVoiceBox"
     disabled_control="MuteAudio"
     image_selected="AudioMute_Off"
     image_unselected="Audio_Off"
     is_toggle="true"
     tab_stop="false"
    />
```

---

## 7. Slider in Sound-Einstellungen (`panel_preferences_sound.xml`)

**Datei:** `indra/newview/skins/default/xui/en/panel_preferences_sound.xml`
Einfügen nach dem Voice-Chat-Block (nach Zeile ~595), vor "Hear media and sounds from:".

```xml
        <slider
         control_name="AudioLevelVoiceBox"
         disabled_control="MuteAudio"
         follows="left|top"
         height="16"
         increment="0.025"
         initial_value="0.5"
         label="VoiceBox TTS"
         label_width="120"
         layout="topleft"
         left="0"
         name="VoiceBox Volume"
         show_text="false"
         slider_label.halign="right"
         top_pad="2"
         volume="true"
         width="300">
            <slider.commit_callback
             function="Pref.setControlFalse"
             parameter="MuteVoiceBox" />
        </slider>
        <button
         control_name="MuteVoiceBox"
         disabled_control="MuteAudio"
         follows="left|top"
         height="16"
         image_selected="AudioMute_Off"
         image_unselected="Audio_Off"
         is_toggle="true"
         layout="topleft"
         left_pad="5"
         name="mute_voicebox_btn"
         tab_stop="false"
         width="16" />
```

---

## 8. Slider in Voicebox-TTS-Panel (`panel_preferences_voicebox.xml`)

**Datei:** `indra/newview/skins/default/xui/en/panel_preferences_voicebox.xml`
Einfügen vor dem Info-Text am Ende, z. B. nach dem Name-Format-ComboBox (~Zeile 185).

```xml
    <text
     type="string"
     length="1"
     follows="left|top"
     layout="topleft"
     left="20"
     height="15"
     name="voicebox_volume_label"
     width="200"
     top_pad="15">
        Volume:
    </text>
    <slider
     control_name="AudioLevelVoiceBox"
     disabled_control="MuteAudio"
     follows="left|top"
     height="16"
     increment="0.025"
     initial_value="0.5"
     layout="topleft"
     left="20"
     name="voicebox_volume"
     show_text="true"
     volume="true"
     width="300">
        <slider.commit_callback
         function="Pref.setControlFalse"
         parameter="MuteVoiceBox" />
    </slider>
```

---

## Hinweise

- `AUDIO_TYPE_VOICEBOX = 4` liegt zwischen Ambient und Count.
- `mSecondaryGain[AUDIO_TYPE_COUNT]` wird automatisch größer – Initialisierung auf 1.0 erfolgt in einer Schleife über `AUDIO_TYPE_COUNT`.
- Die bestehenden `MuteSounds`/`MuteUI` etc. sind `MuteAudio`-untergeordnet – die neue `MuteVoiceBox` folgt demselben Muster.
- Der OpenAL- und FMOD-Treiber arbeiten typunabhängig; sie wenden `secondaryGain * sourceGain` an, ohne den Typ zu interpretieren.
