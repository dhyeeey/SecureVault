#include "AppSettings.h"

#include <QDir>
#include <QUrl>

namespace {
const char kVaultPath[] = "vault/path";

QString normalizePath(const QString &pathOrUrl)
{
    const QString trimmed = pathOrUrl.trimmed();
    if (trimmed.startsWith(QLatin1String("file:")))
        return QDir::cleanPath(QUrl(trimmed).toLocalFile());
    return trimmed.isEmpty() ? QString() : QDir::cleanPath(trimmed);
}
} // namespace

AppSettings::AppSettings(const QString &defaultVaultPath, QObject *parent)
    : QObject(parent)
    , m_defaultVaultPath(QDir::cleanPath(defaultVaultPath))
{
    // Earlier builds kept these unencrypted; they now live inside the vault.
    m_store.remove(QStringLiteral("behavior"));
    m_store.remove(QStringLiteral("security"));

    m_vaultPath = normalizePath(m_store.value(kVaultPath, m_defaultVaultPath).toString());
    if (m_vaultPath.isEmpty())
        m_vaultPath = m_defaultVaultPath;
}

void AppSettings::setVaultPath(const QString &pathOrUrl)
{
    const QString path = normalizePath(pathOrUrl);
    if (path.isEmpty() || m_vaultPath == path)
        return;
    m_vaultPath = path;
    m_store.setValue(kVaultPath, path);
    emit vaultPathChanged();
}
