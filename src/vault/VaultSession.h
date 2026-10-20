#pragma once

#include "EncryptedIndex.h"
#include "FileIndexModel.h"
#include "crypto/CryptoService.h"

#include <QAtomicInteger>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <functional>
#include <memory>

class BlobStore;
class QCoreApplication;

class VaultSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool unlocked READ unlocked NOTIFY unlockedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool vaultExists READ vaultExists NOTIFY vaultPathChanged)
    Q_PROPERTY(bool atRoot READ atRoot NOTIFY currentFolderChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY statsChanged)
    Q_PROPERTY(int idleTimeoutMs READ idleTimeoutMs WRITE setIdleTimeoutMs NOTIFY idleTimeoutMsChanged)
    Q_PROPERTY(QString vaultPath READ vaultPath WRITE setVaultPath NOTIFY vaultPathChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(QString currentFolderId READ currentFolderId NOTIFY currentFolderChanged)
    Q_PROPERTY(QString currentFolderName READ currentFolderName NOTIFY currentFolderChanged)
    Q_PROPERTY(QString usedSizeText READ usedSizeText NOTIFY statsChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString defaultVaultPath READ defaultVaultPath CONSTANT)
    Q_PROPERTY(QString protectionText READ protectionText CONSTANT)
    Q_PROPERTY(QVariantList breadcrumbs READ breadcrumbs NOTIFY currentFolderChanged)
    Q_PROPERTY(QString sortKey READ sortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)
    Q_PROPERTY(int autoLockMinutes READ autoLockMinutes WRITE setAutoLockMinutes NOTIFY vaultPrefsChanged)
    Q_PROPERTY(bool confirmDelete READ confirmDelete WRITE setConfirmDelete NOTIFY vaultPrefsChanged)
    Q_PROPERTY(FileIndexModel *files READ files CONSTANT)

public:
    explicit VaultSession(QObject *parent = nullptr);
    ~VaultSession() override;

    bool unlocked() const { return m_unlocked; }
    bool busy() const { return m_busy; }
    bool vaultExists() const;
    bool atRoot() const { return m_currentFolderId.isEmpty(); }
    bool canPaste() const;
    double progress() const { return m_progress; }
    int itemCount() const;
    int idleTimeoutMs() const { return m_idleTimeoutMs; }
    QString vaultPath() const { return m_vaultPath; }
    QString lastError() const { return m_lastError; }
    QString progressText() const { return m_progressText; }
    QString currentFolderId() const { return m_currentFolderId; }
    QString currentFolderName() const;
    QString usedSizeText() const;
    QString searchQuery() const { return m_searchQuery; }
    QString defaultVaultPath() const;
    QString protectionText() const { return QStringLiteral("XChaCha20-Poly1305 · Argon2id"); }
    QVariantList breadcrumbs() const;
    QString sortKey() const { return m_sortKey; }
    bool sortAscending() const { return m_sortAscending; }
    int autoLockMinutes() const { return m_index.prefs().autoLockMinutes; }
    bool confirmDelete() const { return m_index.prefs().confirmDelete; }
    FileIndexModel *files() { return &m_files; }

    // Only take effect while unlocked; persisted in the encrypted index.
    void setAutoLockMinutes(int minutes);
    void setConfirmDelete(bool value);

    void setVaultPath(const QString &path);
    void setSearchQuery(const QString &query);
    void setIdleTimeoutMs(int ms);
    void installIdleGuard(QCoreApplication *app);

    Q_INVOKABLE void createVault(const QString &path, const QString &password);
    Q_INVOKABLE void unlock(const QString &path, const QString &password);
    Q_INVOKABLE void lock();
    Q_INVOKABLE void changePassword(const QString &oldPassword, const QString &newPassword);
    Q_INVOKABLE void importPaths(const QStringList &paths, bool move = false);
    Q_INVOKABLE void exportEntries(const QStringList &ids, const QString &destDir);
    Q_INVOKABLE void deleteEntries(const QStringList &ids);
    Q_INVOKABLE void renameEntry(const QString &id, const QString &newName);
    Q_INVOKABLE void mkdir(const QString &name);
    Q_INVOKABLE void navigate(const QString &folderId);
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void openOrEnter(const QString &id);
    Q_INVOKABLE void copyEntries(const QStringList &ids);
    Q_INVOKABLE void cutEntries(const QStringList &ids);
    Q_INVOKABLE void paste();
    Q_INVOKABLE bool isVaultAt(const QString &path) const;
    Q_INVOKABLE void sortBy(const QString &key);
    Q_INVOKABLE QString entryName(const QString &id) const;
    Q_INVOKABLE QStringList currentVisibleIds() const;

    // Synchronous helpers used by tests and background jobs.
    bool createVaultImpl(QString path, QString password, QString *error);
    bool unlockImpl(QString path, QString password, QString *error);
    bool changePasswordImpl(QString oldPassword, QString newPassword, QString *error);
    bool importPathsImpl(QStringList paths, QString *error, bool move = false);
    bool exportEntriesImpl(QStringList ids, QString destDir, QString *error);
    bool deleteEntriesImpl(QStringList ids, QString *error);
    bool pasteImpl(QString *error);
    QStringList fileIds() const;

signals:
    void unlockedChanged();
    void busyChanged();
    void vaultPathChanged();
    void lastErrorChanged();
    void progressChanged();
    void currentFolderChanged();
    void searchQueryChanged();
    void statsChanged();
    void clipboardChanged();
    void idleTimeoutMsChanged();
    void sortChanged();
    void vaultPrefsChanged();
    void vaultChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class ClipMode { None, Copy, Cut };

    void setBusy(bool busy);
    void setLastError(const QString &error);
    void setProgress(double value, const QString &text);
    void setUnlocked(bool unlocked);
    void refreshModel();
    void touchIdleTimer();
    void wipeOpenedTemps();
    void closeVaultState();
    void runAsync(const std::function<bool(QString *)> &fn);
    bool updatePrefs(const std::function<void(VaultPrefs &)> &mutate);
    void applyPrefs();
    QString toLocalPath(const QString &pathOrUrl) const;
    QString uniqueName(const QString &parentId, const QString &desired,
                       const QString &ignoreId = {}) const;
    void startOsClipboardExport(const QStringList &ids, bool move);
    void armOsCutWatcher(const QString &dir, const QStringList &ids, const QStringList &topPaths);
    void stopOsCutWatcher();
    void cancelOsClipboardMove();
    void onOsClipDirChanged();
    void finishOsCutIfConsumed();
    void flushPendingOsCutDelete();
    bool readOsClipboardImport(QStringList *paths, bool *move) const;
    bool pathsAreOurStaging(const QStringList &paths) const;
    void removeOsSourcePaths(const QStringList &paths);
    QString newEntryId() const;
    QStringList descendants(const QString &id) const;
    bool saveIndex(QString *error);
    bool encryptFileToBlob(const QString &srcPath, VaultEntry *entry, QString *error);
    bool decryptBlobToFile(const VaultEntry &entry, const QString &destPath, QString *error);
    bool copySubtree(const QString &sourceId, const QString &destParentId, QString *error);
    void applySort(QVector<VaultEntry> *list) const;
    bool openEntryToTemp(const VaultEntry &entry, QString *error);

    FileIndexModel m_files;
    EncryptedIndex m_index;
    MasterKey m_master;
    std::unique_ptr<BlobStore> m_store;
    QTimer m_idleTimer;
    QString m_vaultPath;
    QString m_lastError;
    QString m_progressText;
    QString m_currentFolderId;
    QString m_searchQuery;
    QString m_sortKey = QStringLiteral("name");
    bool m_sortAscending = true;
    QStringList m_clipIds;
    QStringList m_openedTemps;
    QString m_openDir;
    QString m_clipExportDir;
    QStringList m_clipOsIds;
    QStringList m_clipOsTopPaths;
    QStringList m_pendingOsCutDelete;
    QFileSystemWatcher m_clipWatcher;
    QTimer m_clipPollTimer;
    QMutex m_ioMutex;
    QAtomicInteger<int> m_clipExportGen{0};
    ClipMode m_clipMode = ClipMode::None;
    double m_progress = 0;
    int m_idleTimeoutMs = 5 * 60 * 1000;
    bool m_unlocked = false;
    bool m_busy = false;
    bool m_lockAfterBusy = false;
};
