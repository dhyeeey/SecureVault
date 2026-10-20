#include "EncryptedIndex.h"

#include "VaultFormat.h"
#include "crypto/CryptoService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

void EncryptedIndex::clear()
{
    m_entries.clear();
    m_prefs = {};
}

int EncryptedIndex::indexOf(const QString &id) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id)
            return i;
    }
    return -1;
}

const VaultEntry *EncryptedIndex::find(const QString &id) const
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &m_entries[i];
}

VaultEntry *EncryptedIndex::find(const QString &id)
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &m_entries[i];
}

bool EncryptedIndex::load(const QString &vaultDir, const unsigned char *masterKey32, QString *error)
{
    clear();
    QFile f(VaultFormat::indexPath(vaultDir));
    if (!f.exists())
        return true;
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open index.enc");
        return false;
    }
    const QByteArray raw = f.readAll();
    if (raw.isEmpty())
        return true;
    if (static_cast<quint8>(raw[0]) != CryptoService::kIndexVersion) {
        if (error)
            *error = QStringLiteral("Unknown index version");
        return false;
    }

    QByteArray json = CryptoService::aeadDecrypt(masterKey32,
                                                 raw.mid(1),
                                                 VaultFormat::indexAad(),
                                                 error);
    if (json.isEmpty()) {
        if (error)
            *error = QStringLiteral("Cannot decrypt index (wrong password or corruption)");
        return false;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    CryptoService::wipe(json);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("Index JSON is invalid");
        return false;
    }

    const QJsonObject settings = doc.object().value(QStringLiteral("settings")).toObject();
    m_prefs.autoLockMinutes = qMax(0, settings.value(QStringLiteral("autoLockMinutes")).toInt(5));
    m_prefs.confirmDelete = settings.value(QStringLiteral("confirmDelete")).toBool(true);

    const QJsonArray arr = doc.object().value(QStringLiteral("entries")).toArray();
    m_entries.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        VaultEntry e;
        e.id = o.value(QStringLiteral("id")).toString();
        e.parentId = o.value(QStringLiteral("parent")).toString();
        e.name = o.value(QStringLiteral("name")).toString();
        e.isDir = o.value(QStringLiteral("dir")).toBool();
        e.blobId = o.value(QStringLiteral("blob")).toString();
        e.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
        e.modifiedMs = static_cast<qint64>(o.value(QStringLiteral("mtime")).toDouble());
        if (!e.id.isEmpty() && !e.name.isEmpty())
            m_entries.push_back(std::move(e));
    }
    return true;
}

bool EncryptedIndex::save(const QString &vaultDir, const unsigned char *masterKey32, QString *error) const
{
    QJsonArray arr;
    for (const VaultEntry &e : m_entries) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), e.id);
        o.insert(QStringLiteral("parent"), e.parentId);
        o.insert(QStringLiteral("name"), e.name);
        o.insert(QStringLiteral("dir"), e.isDir);
        o.insert(QStringLiteral("blob"), e.blobId);
        o.insert(QStringLiteral("size"), static_cast<double>(e.size));
        o.insert(QStringLiteral("mtime"), static_cast<double>(e.modifiedMs));
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("v"), 1);
    root.insert(QStringLiteral("settings"),
                QJsonObject{{QStringLiteral("autoLockMinutes"), m_prefs.autoLockMinutes},
                            {QStringLiteral("confirmDelete"), m_prefs.confirmDelete}});
    root.insert(QStringLiteral("entries"), arr);

    QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QByteArray ct = CryptoService::aeadEncrypt(masterKey32, json, VaultFormat::indexAad(), error);
    CryptoService::wipe(json);
    if (ct.isEmpty())
        return false;

    QByteArray raw;
    raw.reserve(1 + ct.size());
    raw.append(static_cast<char>(CryptoService::kIndexVersion));
    raw.append(ct);

    QSaveFile f(VaultFormat::indexPath(vaultDir));
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot write index.enc");
        return false;
    }
    if (f.write(raw) != raw.size()) {
        if (error)
            *error = QStringLiteral("Failed writing index.enc");
        return false;
    }
    if (!f.commit()) {
        if (error)
            *error = QStringLiteral("Failed to commit index.enc");
        return false;
    }
    return true;
}
