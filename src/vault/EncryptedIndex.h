#pragma once

#include "VaultEntry.h"

#include <QVector>

class MasterKey;

// Per-vault preferences. Stored inside the encrypted index so they cannot be
// read or changed without the vault password.
struct VaultPrefs {
    int autoLockMinutes = 5; // 0 disables auto-lock
    bool confirmDelete = true;
};

class EncryptedIndex {
public:
    const QVector<VaultEntry> &entries() const { return m_entries; }
    QVector<VaultEntry> &entries() { return m_entries; }
    const VaultPrefs &prefs() const { return m_prefs; }
    VaultPrefs &prefs() { return m_prefs; }

    void clear();
    bool load(const QString &vaultDir, const unsigned char *masterKey32, QString *error);
    bool save(const QString &vaultDir, const unsigned char *masterKey32, QString *error) const;

    int indexOf(const QString &id) const;
    const VaultEntry *find(const QString &id) const;
    VaultEntry *find(const QString &id);

private:
    QVector<VaultEntry> m_entries;
    VaultPrefs m_prefs;
};
