#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

// Settings readable before unlock. Anything security-relevant lives in the
// encrypted vault index instead (see VaultPrefs).
class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString vaultPath READ vaultPath WRITE setVaultPath NOTIFY vaultPathChanged)
    Q_PROPERTY(QString defaultVaultPath READ defaultVaultPath CONSTANT)

public:
    explicit AppSettings(const QString &defaultVaultPath, QObject *parent = nullptr);

    QString vaultPath() const { return m_vaultPath; }
    QString defaultVaultPath() const { return m_defaultVaultPath; }

    void setVaultPath(const QString &pathOrUrl);

signals:
    void vaultPathChanged();

private:
    QSettings m_store;
    QString m_defaultVaultPath;
    QString m_vaultPath;
};
