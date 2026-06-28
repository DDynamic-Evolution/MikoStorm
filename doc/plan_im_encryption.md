# Plan: IM Encryption in Privacy Settings

## Overview

Add end-to-end encryption for Instant Messages (IMs) to the Firestorm viewer.
The feature is integrated as a new subtab in the existing Privacy preferences panel.

Only users who share the same encryption key can read encrypted messages.
Users without the key see the raw ciphertext (garbled / scrambled text).

---

## Architecture

### Wire Protocol Compatibility

No changes to the SL message protocol (`ImprovedInstantMessage`).

- Unencrypted IMs are sent exactly as before — no overhead, no marker.
- Encrypted IMs place a `[E2E]` marker + base64-encoded ciphertext in the existing `Message` string field.
- The `BinaryBucket` field is left untouched (inventory offers, group notices, etc. remain unchanged).

### Encryption Flow (Sender)

```
User types message in IM floater
    → FSFloaterIM::sendMsgFromInputEditor()
    → FSFloaterIM::sendMsg()
    → LLIMModel::sendMessage()
    → deliverMessage()                  ← ENCRYPTION HOOK
        IF FSIMEncryptionEnabled == true AND key is set:
            ciphertext = FSIMCipher::encrypt(plaintext, key)
            message = "[E2E]" + ciphertext
        → pack_instant_message()
        → gAgent.sendReliableMessage()
```

### Decryption Flow (Receiver)

```
UDP "ImprovedInstantMessage" received
    → process_improved_im()
    → LLIMProcessing::processNewMessage()  ← DECRYPTION HOOK
        IF message starts with "[E2E]" AND FSIMEncryptionEnabled == true:
            plaintext = FSIMCipher::decrypt(message.substr(5), key)
            IF decryption succeeded:
                message = plaintext
            ELSE:
                // keep message as-is (raw ciphertext displayed)
        → gIMMgr->addMessage()  ← displays in IM floater / chat history
```

### How Unencrypted Clients See It

- They receive `[E2E]<base64>` as the message text.
- They display it verbatim: `[E2E]9jf83jdD92l...` — garbled/unreadable.
- No error, no crash, nothing special — just meaningless text.

---

## New Files

| File | Purpose |
|------|---------|
| `indra/newview/fsimcipher.h` | `FSIMCipher` class declaration |
| `indra/newview/fsimcipher.cpp` | AES-256-GCM encrypt/decrypt via OpenSSL EVP |

---

## Modified Files

| File | Change |
|------|--------|
| `indra/newview/llimview.cpp` | Hook `deliverMessage()` to encrypt outgoing IM text |
| `indra/newview/llimprocessing.cpp` | Hook `processNewMessage()` to decrypt incoming IM text |
| `indra/newview/llfloaterpreference.cpp` | Add new subtab class `FSPanelPreferenceIMEncryption`, register with `LLPanelInjector` |
| `indra/newview/app_settings/settings_per_account.xml` | Add 3 new settings (toggle, key, algorithm) |
| `indra/newview/skins/default/xui/en/panel_preferences_privacy.xml` | Add 6th subtab with key field, algorithm dropdown, enable checkbox |
| `indra/newview/CMakeLists.txt` | Add `fsimcipher.cpp` to source list |

---

## Settings (in `settings_per_account.xml`)

| Setting Name | Type | Default | Description |
|---|---|---|---|
| `FSIMEncryptionEnabled` | Boolean | false | Master toggle for IM encryption |
| `FSIMEncryptionKey` | String | "" | The shared secret / password |
| `FSIMEncryptionAlgorithm` | S32 | 0 | 0 = AES-256-GCM, 1 = AES-256-CBC+HMAC |

Key is stored per-account (like passwords) so different avatars can have different keys.

---

## Encryption Details (`FSIMCipher`)

### Algorithm: AES-256-GCM (default)

- Key derivation: PBKDF2-HMAC-SHA256, 100000 iterations, 16-byte random salt
- 256-bit AES key
- 12-byte random IV (generated fresh per message via `RAND_bytes()`)
- 16-byte GCM authentication tag
- Authenticated encryption (confidentiality + integrity)

### Wire Format

`[E2E]` + base64 of the following binary blob:

```
Byte 0:       version (0x01 = AES-256-GCM, 0x02 = AES-256-CBC+HMAC)
Bytes 1-16:   PBKDF2 salt (for key derivation on decrypt)
Bytes 17-28:  IV (12 bytes)
Bytes 29-end: ciphertext + GCM tag (last 16 bytes)
```

Base64 is standard without line breaks.

### Message Length Limit

- Plaintext up to ~740 bytes: encrypted + base64 + marker stays within 1023-byte `MAX_MSG_STR_LEN` limit.
- Longer messages: send unencrypted with a warning to the user.
- Messages over 1023 bytes are truncated by the existing `snprintf` in `pack_instant_message_block()` anyway.

### Algorithm Extension

The `version` byte allows adding future algorithms (e.g., ChaCha20-Poly1305 as version `0x03`)
without breaking backward compatibility.

---

## UI: New "IM Encryption" Subtab in Privacy

### Tab placement

6th subtab in `panel_preferences_privacy.xml`, after "Autoresponse 2":

```
1. General
2. Logs & Transcripts
3. LookAt
4. Autoresponse 1
5. Autoresponse 2
6. IM Encryption   ← NEW
```

### Controls

| Control | Type | Description |
|---------|------|-------------|
| Enable encryption | `check_box` | Master on/off toggle, bound to `FSIMEncryptionEnabled` |
| Encryption key | `line_editor` (is_password="true") | Password-masked input for the shared secret, bound to `FSIMEncryptionKey` |
| Algorithm | `combo_box` | Algorithm selection, bound to `FSIMEncryptionAlgorithm` |

### Info Text

> Enable end-to-end encryption for Instant Messages.
> Only users with the same key can read encrypted messages.
> Users without the key will see scrambled text.
>
> This affects peer-to-peer IMs and group IMs.
> Nearby chat cannot be encrypted.

---

## Edge Cases & Error Handling

| Scenario | Behavior |
|----------|----------|
| No key set, encryption enabled | Send/receive without encryption, log a warning |
| Receive `[E2E]` but encryption disabled | Display raw `[E2E]...` text as-is |
| Decryption fails (wrong key, corrupted data) | Display raw `[E2E]...` text, log error |
| Sender has encryption, receiver does not | Receiver sees `[E2E]` + base64 gibberish |
| Both have encryption, different keys | Decryption fails, both see gibberish |
| Message too long to encrypt | Send unencrypted, notify user |
| Group chat with mixed participants | Encrypted messages show as gibberish to non-key members |

---

## Future Improvements

- Store key in protected data store (`bin_conf.dat` via `LLSecAPI`) instead of `settings_per_account.xml`
- ChaCha20-Poly1305 support (for platforms without AES-NI)
- Key exchange mechanism (out of scope for V1)
- Per-contact keys (out of scope for V1)
