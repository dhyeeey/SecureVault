# SecureVault

A desktop encrypted file vault for Windows and Linux. Files stay on your computer as ciphertext — this is a normal desktop app, not a portable USB tool.

Files on disk are ciphertext with obfuscated names. Real names, folders, and contents are only visible after you unlock with your password.

**C++20 · Qt 6 QML · libsodium** · one codebase for Windows and Linux.

> **Lost password = lost vault.** There is no recovery.

## Features

- Create or unlock a vault with a password (Argon2id)
- Browse folders, search, sort, rename, and create folders
- Import and export files or whole folders (drag-and-drop supported)
- Copy / cut / paste **inside** the vault and **with** File Explorer (and the Linux file manager)
  - Copy keeps the original
  - Cut moves (paste into the vault encrypts, then removes the source; paste to the desktop decrypts, then removes it from the vault)
- Multi-select with row checkboxes
- Delete confirmation, with an optional “don’t ask again” per vault
- Open a file: decrypts a temp copy and launches the system handler
- Auto-lock after idle time; lock wipes keys and temp files from memory/disk
- Change password without re-encrypting every file
- Settings only after unlock — auto-lock and delete confirmation live **inside** the encrypted index

## Threat model

**Protects against**

- Someone copying the vault folder off disk, a USB stick, or a backup
- Casual inspection of filenames (the index is encrypted; blobs have hex names)

**Does not protect against**

- Malware, keyloggers, or a compromised OS
- Memory dumps while the vault is unlocked
- Forgetting the password

This is encryption at rest, not a kernel disk encryptor like BitLocker or VeraCrypt. Unlocking decrypts data **in the app**. Opening a file writes a plaintext temp copy; that copy is wiped on lock.

## How encryption works

Password → **Argon2id** → wrapping key → unwraps a random 256-bit master key.

| On disk | What it is |
| --- | --- |
| `vault.header` | Magic `SVLT`, Argon2id parameters, salt, wrapped master key |
| `index.enc` | AEAD-encrypted JSON tree (names, dates, sizes, per-vault settings) |
| `blobs/ab/cd/<id>` | Per-file **XChaCha20-Poly1305** secretstream ciphertext |

Changing the password re-wraps the master key. File blobs are not rewritten.

A later version can switch to a single `.vault` container (`ContainerBlobStore`) without changing the UI. That backend is stubbed, not wired up.

Default vault location: `Documents/SecureVault`.

## Using the app

1. Create a vault with a password of at least 8 characters, or unlock an existing one.
2. **Open a different vault…** on the unlock screen picks another folder. **Settings → Switch vault…** (while unlocked) locks the current vault first, then points at a new folder.
3. Import files, drag them in, or paste from the file manager. Use row checkboxes for multi-select.
4. Double-click a file to open it. **Lock** (or wait for auto-lock, 5 minutes by default) to wipe keys and temp files.
5. Settings is only reachable while unlocked. Auto-lock and “confirm before deleting” are stored encrypted in that vault. Only the last-used vault **path** is stored in plain `QSettings`, because the app needs it before unlock.

## Build

### Requirements

- Qt 6.10+ (developed with **6.12**) with Quick
- CMake 3.16+
- C++20 compiler (MSVC 2022 on Windows, GCC or Clang on Linux)
- Git (CMake fetches [libsodium-cmake](https://github.com/robinlinden/libsodium-cmake) on first configure)
- Network access for that first configure

If vcpkg already has `libsodium`, CMake uses `unofficial-sodium` instead of fetching.

### Qt Creator

Open `CMakeLists.txt`, pick a Desktop Qt 6 kit (MSVC or GCC), Configure, Build. The first configure clones libsodium and takes longer. Run the `appSecureVault` target.

### Command line (Windows)

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G "NMake Makefiles JOM" -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
cmake --build build
```

### Command line (Linux)

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
cmake --build build
```

## Packaging

- **Windows:** build Release, then `packaging/windows-deploy.ps1` (`windeployqt --qmldir`).
- **Linux:** `packaging/linux-deploy.sh` (uses `linuxdeployqt` if installed).

macOS is the same source; v1 does not ship notarized packages.

## Project layout

```
secure-vault/
├── main.cpp                 # App entry, sodium init, QML context
├── src/
│   ├── app/                 # Unencrypted app settings (last vault path)
│   ├── crypto/              # libsodium wrappers (AEAD, Argon2id, secretstream)
│   └── vault/               # Header, index, blob store, session
├── *.qml                    # Qt Quick UI
└── packaging/
```

## Status

Early **v0.1** folder vault. Useful as a personal encrypted file browser; not audited. Do not treat it as a substitute for full-disk encryption or a reviewed password manager.

## License

No license file is published yet. All rights reserved until one is added.
