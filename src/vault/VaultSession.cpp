#include "VaultSession.h"

#include "FolderBlobStore.h"
#include "VaultFormat.h"
#include "crypto/CryptoService.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QMimeData>
#include <QMutexLocker>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <QVector>
#include <QtConcurrent>

#include <sodium.h>

#include <algorithm>
#include <cstring>

namespace {
void secureDeleteFile(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadWrite)) {
        const qint64 sz = f.size();
        QByteArray z(qMin<qint64>(sz, 1024 * 1024), '\0');
        qint64 left = sz;
        f.seek(0);
        while (left > 0) {
            const qint64 n = qMin<qint64>(left, z.size());
            if (f.write(z.constData(), n) != n)
                break;
            left -= n;
        }
        f.flush();
        f.close();
    }
    QFile::remove(path);
}

void wipeDirSecure(const QString &dirPath)
{
    if (dirPath.isEmpty())
        return;
    QDir dir(dirPath);
    if (!dir.exists())
        return;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                                                    | QDir::Hidden | QDir::System);
    for (const QFileInfo &fi : entries) {
        if (fi.isDir())
            wipeDirSecure(fi.absoluteFilePath());
        else
            secureDeleteFile(fi.absoluteFilePath());
    }
    QDir().rmdir(dirPath);
    if (QDir(dirPath).exists())
        QDir(dirPath).removeRecursively();
}

bool pathIsInside(const QString &rootRaw, const QString &pathRaw)
{
    const QString root = QDir::cleanPath(rootRaw);
    const QString path = QDir::cleanPath(pathRaw);
    if (root.isEmpty() || path.isEmpty())
        return false;
    if (path.compare(root, Qt::CaseInsensitive) == 0)
        return true;
    const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
    return path.startsWith(prefix, Qt::CaseInsensitive);
}

constexpr auto kPreferredDropEffect =
    "application/x-qt-windows-mime;value=\"Preferred DropEffect\"";

void clearClipboardIfUrlsUnder(const QString &dir)
{
    if (dir.isEmpty() || !QGuiApplication::instance())
        return;
    QClipboard *clip = QGuiApplication::clipboard();
    if (!clip)
        return;
    const QMimeData *md = clip->mimeData();
    if (!md || !md->hasUrls())
        return;
    const QString prefix = QDir::cleanPath(dir);
    for (const QUrl &url : md->urls()) {
        const QString local = QDir::cleanPath(url.toLocalFile());
        if (local == prefix || local.startsWith(prefix + QLatin1Char('/'))
            || local.startsWith(prefix + QLatin1Char('\\'))) {
            clip->clear();
            return;
        }
    }
}
} // namespace

VaultSession::VaultSession(QObject *parent)
    : QObject(parent)
    , m_store(std::make_unique<FolderBlobStore>())
{
    m_idleTimer.setSingleShot(true);
    connect(&m_idleTimer, &QTimer::timeout, this, [this]() {
        if (m_unlocked && !m_busy)
            lock();
        else if (m_unlocked && m_busy)
            m_lockAfterBusy = true;
    });
    m_clipPollTimer.setInterval(400);
    connect(&m_clipPollTimer, &QTimer::timeout, this, &VaultSession::onOsClipDirChanged);
    connect(&m_clipWatcher, &QFileSystemWatcher::directoryChanged, this, &VaultSession::onOsClipDirChanged);
    connect(&m_clipWatcher, &QFileSystemWatcher::fileChanged, this, &VaultSession::onOsClipDirChanged);
    if (QGuiApplication::instance()) {
        if (QClipboard *clip = QGuiApplication::clipboard())
            connect(clip, &QClipboard::dataChanged, this, &VaultSession::clipboardChanged);
    }
    m_vaultPath = defaultVaultPath();
}

VaultSession::~VaultSession()
{
    lock();
}

void VaultSession::setVaultPath(const QString &path)
{
    if (m_unlocked || m_busy)
        return;
    const QString local = toLocalPath(path);
    if (m_vaultPath == local)
        return;
    m_vaultPath = local;
    emit vaultPathChanged();
}

void VaultSession::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query)
        return;
    m_searchQuery = query;
    emit searchQueryChanged();
    refreshModel();
}

void VaultSession::setIdleTimeoutMs(int ms)
{
    if (ms < 0)
        ms = 0;
    if (ms > 0 && ms < 5000)
        ms = 5000;
    if (m_idleTimeoutMs == ms)
        return;
    m_idleTimeoutMs = ms;
    if (ms > 0)
        m_idleTimer.setInterval(ms);
    else
        m_idleTimer.stop();
    emit idleTimeoutMsChanged();
    if (m_unlocked)
        touchIdleTimer();
}

void VaultSession::installIdleGuard(QCoreApplication *app)
{
    if (!app)
        return;
    app->installEventFilter(this);
    m_idleTimer.setInterval(m_idleTimeoutMs);
}

bool VaultSession::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        if (m_unlocked)
            touchIdleTimer();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

bool VaultSession::vaultExists() const
{
    return VaultFormat::isVaultDir(m_vaultPath);
}

int VaultSession::itemCount() const
{
    return m_index.entries().size();
}

QString VaultSession::currentFolderName() const
{
    if (m_currentFolderId.isEmpty())
        return QStringLiteral("My Vault");
    if (const VaultEntry *e = m_index.find(m_currentFolderId))
        return e->name;
    return QStringLiteral("My Vault");
}

QString VaultSession::usedSizeText() const
{
    qint64 total = 0;
    for (const VaultEntry &e : m_index.entries()) {
        if (!e.isDir)
            total += e.size;
    }
    return FileIndexModel::formatBytes(total);
}

QVariantList VaultSession::breadcrumbs() const
{
    QVariantList chain;
    QString id = m_currentFolderId;
    for (int depth = 0; !id.isEmpty() && depth < 256; ++depth) {
        const VaultEntry *e = m_index.find(id);
        if (!e)
            break;
        chain.prepend(QVariantMap{{QStringLiteral("id"), e->id}, {QStringLiteral("name"), e->name}});
        id = e->parentId;
    }
    chain.prepend(QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QStringLiteral("My Vault")}});
    return chain;
}

void VaultSession::sortBy(const QString &key)
{
    if (key != QLatin1String("name") && key != QLatin1String("date") && key != QLatin1String("size"))
        return;
    if (m_sortKey == key) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortKey = key;
        m_sortAscending = key == QLatin1String("name");
    }
    emit sortChanged();
    refreshModel();
}

void VaultSession::setAutoLockMinutes(int minutes)
{
    minutes = qMax(0, minutes);
    if (minutes == autoLockMinutes())
        return;
    updatePrefs([minutes](VaultPrefs &p) { p.autoLockMinutes = minutes; });
}

void VaultSession::setConfirmDelete(bool value)
{
    if (value == confirmDelete())
        return;
    updatePrefs([value](VaultPrefs &p) { p.confirmDelete = value; });
}

bool VaultSession::updatePrefs(const std::function<void(VaultPrefs &)> &mutate)
{
    if (!m_unlocked || m_busy || !m_master.valid()) {
        emit vaultPrefsChanged();
        return false;
    }
    const VaultPrefs before = m_index.prefs();
    mutate(m_index.prefs());
    QString err;
    if (!saveIndex(&err)) {
        m_index.prefs() = before;
        setLastError(err);
        emit vaultPrefsChanged();
        return false;
    }
    applyPrefs();
    emit vaultPrefsChanged();
    return true;
}

void VaultSession::applyPrefs()
{
    setIdleTimeoutMs(m_index.prefs().autoLockMinutes * 60 * 1000);
}

QString VaultSession::entryName(const QString &id) const
{
    const VaultEntry *e = m_index.find(id);
    return e ? e->name : QString();
}

QString VaultSession::defaultVaultPath() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QStringLiteral("SecureVault"));
}

bool VaultSession::isVaultAt(const QString &path) const
{
    return VaultFormat::isVaultDir(toLocalPath(path));
}

void VaultSession::createVault(const QString &path, const QString &password)
{
    const QString local = toLocalPath(path);
    QString pw = password;
    runAsync([this, local, pw](QString *error) mutable {
        const bool ok = createVaultImpl(local, pw, error);
        CryptoService::wipe(const_cast<QString &>(pw));
        return ok;
    });
}

void VaultSession::unlock(const QString &path, const QString &password)
{
    const QString local = toLocalPath(path);
    QString pw = password;
    runAsync([this, local, pw](QString *error) mutable {
        const bool ok = unlockImpl(local, pw, error);
        CryptoService::wipe(const_cast<QString &>(pw));
        return ok;
    });
}

void VaultSession::lock()
{
    if (m_busy) {
        m_lockAfterBusy = true;
        return;
    }
    setLastError({});
    closeVaultState();
}

void VaultSession::changePassword(const QString &oldPassword, const QString &newPassword)
{
    QString oldPw = oldPassword;
    QString newPw = newPassword;
    runAsync([this, oldPw, newPw](QString *error) mutable {
        const bool ok = changePasswordImpl(oldPw, newPw, error);
        CryptoService::wipe(oldPw);
        CryptoService::wipe(newPw);
        return ok;
    });
}

void VaultSession::importPaths(const QStringList &paths, bool move)
{
    runAsync([this, paths, move](QString *error) { return importPathsImpl(paths, error, move); });
}

void VaultSession::exportEntries(const QStringList &ids, const QString &destDir)
{
    const QString dest = toLocalPath(destDir);
    runAsync([this, ids, dest](QString *error) { return exportEntriesImpl(ids, dest, error); });
}

void VaultSession::deleteEntries(const QStringList &ids)
{
    runAsync([this, ids](QString *error) { return deleteEntriesImpl(ids, error); });
}

void VaultSession::renameEntry(const QString &id, const QString &newName)
{
    if (!m_unlocked || m_busy)
        return;
    VaultEntry *e = m_index.find(id);
    if (!e)
        return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) {
        setLastError(QStringLiteral("Name cannot be empty"));
        return;
    }
    e->name = uniqueName(e->parentId, trimmed, e->id);
    e->modifiedMs = QDateTime::currentMSecsSinceEpoch();
    QString err;
    if (!saveIndex(&err))
        setLastError(err);
    refreshModel();
}

void VaultSession::mkdir(const QString &name)
{
    if (!m_unlocked || m_busy)
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        setLastError(QStringLiteral("Folder name cannot be empty"));
        return;
    }
    VaultEntry e;
    e.id = newEntryId();
    e.parentId = m_currentFolderId;
    e.name = uniqueName(m_currentFolderId, trimmed);
    e.isDir = true;
    e.modifiedMs = QDateTime::currentMSecsSinceEpoch();
    m_index.entries().push_back(e);
    QString err;
    if (!saveIndex(&err))
        setLastError(err);
    refreshModel();
}

void VaultSession::navigate(const QString &folderId)
{
    if (!m_unlocked)
        return;
    if (!folderId.isEmpty() && !m_index.find(folderId))
        return;
    m_currentFolderId = folderId;
    emit currentFolderChanged();
    refreshModel();
}

void VaultSession::goUp()
{
    if (m_currentFolderId.isEmpty())
        return;
    if (const VaultEntry *e = m_index.find(m_currentFolderId))
        navigate(e->parentId);
    else
        navigate({});
}

void VaultSession::openOrEnter(const QString &id)
{
    const VaultEntry *e = m_index.find(id);
    if (!e)
        return;
    if (e->isDir) {
        navigate(e->id);
        return;
    }
    QString err;
    if (!openEntryToTemp(*e, &err))
        setLastError(err);
}

void VaultSession::copyEntries(const QStringList &ids)
{
    m_clipIds = ids;
    m_clipMode = ClipMode::Copy;
    emit clipboardChanged();
    startOsClipboardExport(ids, false);
}

void VaultSession::cutEntries(const QStringList &ids)
{
    m_clipIds = ids;
    m_clipMode = ClipMode::Cut;
    emit clipboardChanged();
    startOsClipboardExport(ids, true);
}

bool VaultSession::canPaste() const
{
    if (!m_unlocked)
        return false;
    QStringList osPaths;
    bool osMove = false;
    if (readOsClipboardImport(&osPaths, &osMove))
        return true;
    return !m_clipIds.isEmpty();
}

void VaultSession::paste()
{
    QStringList osPaths;
    bool osMove = false;
    if (readOsClipboardImport(&osPaths, &osMove)) {
        runAsync([this, osPaths, osMove](QString *error) {
            return importPathsImpl(osPaths, error, osMove);
        });
        return;
    }
    runAsync([this](QString *error) { return pasteImpl(error); });
}

QStringList VaultSession::currentVisibleIds() const
{
    QStringList ids;
    for (int i = 0; i < m_files.rowCount(); ++i) {
        if (const VaultEntry *e = m_files.at(i))
            ids.push_back(e->id);
    }
    return ids;
}

QStringList VaultSession::fileIds() const
{
    QStringList ids;
    for (const VaultEntry &e : m_index.entries()) {
        if (!e.isDir)
            ids.push_back(e.id);
    }
    return ids;
}

bool VaultSession::createVaultImpl(QString path, QString password, QString *error)
{
    path = toLocalPath(path);
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("Choose a vault folder");
        return false;
    }
    if (password.size() < 8) {
        if (error)
            *error = QStringLiteral("Password must be at least 8 characters");
        return false;
    }
    if (VaultFormat::isVaultDir(path)) {
        if (error)
            *error = QStringLiteral("A vault already exists in that folder");
        return false;
    }
    if (!QDir().mkpath(path)) {
        if (error)
            *error = QStringLiteral("Cannot create vault folder");
        return false;
    }

    closeVaultState();

    if (!m_master.ensure()) {
        if (error)
            *error = QStringLiteral("Cannot allocate master key");
        return false;
    }
    QByteArray rnd = CryptoService::randomBytes(CryptoService::kKeyBytes);
    memcpy(m_master.data(), rnd.constData(), CryptoService::kKeyBytes);
    CryptoService::wipe(rnd);

    VaultHeader header;
    header.version = VaultFormat::kVersion;
    header.kdf = CryptoService::interactiveKdf();
    header.salt = CryptoService::randomBytes(CryptoService::kSaltBytes);

    unsigned char wrapping[CryptoService::kKeyBytes];
    if (!CryptoService::deriveWrappingKey(password, header.salt, header.kdf, wrapping, error)) {
        CryptoService::wipe(wrapping, sizeof(wrapping));
        m_master.reset();
        return false;
    }
    if (!VaultFormat::wrapMasterKey(wrapping, m_master.data(), &header.wrappedMaster, error)) {
        CryptoService::wipe(wrapping, sizeof(wrapping));
        m_master.reset();
        return false;
    }
    CryptoService::wipe(wrapping, sizeof(wrapping));

    if (!VaultFormat::writeHeader(path, header, error)) {
        m_master.reset();
        return false;
    }

    m_store = std::make_unique<FolderBlobStore>();
    if (!m_store->openStore(path, error)) {
        m_master.reset();
        return false;
    }
    m_index.clear();
    m_vaultPath = path;
    if (!saveIndex(error)) {
        m_master.reset();
        return false;
    }
    m_currentFolderId.clear();
    m_unlocked = true;
    return true;
}

bool VaultSession::unlockImpl(QString path, QString password, QString *error)
{
    path = toLocalPath(path);
    VaultHeader header;
    if (!VaultFormat::readHeader(path, &header, error))
        return false;

    unsigned char wrapping[CryptoService::kKeyBytes];
    if (!CryptoService::deriveWrappingKey(password, header.salt, header.kdf, wrapping, error)) {
        CryptoService::wipe(wrapping, sizeof(wrapping));
        return false;
    }
    if (!m_master.ensure()) {
        CryptoService::wipe(wrapping, sizeof(wrapping));
        if (error)
            *error = QStringLiteral("Cannot allocate master key");
        return false;
    }
    if (!VaultFormat::unwrapMasterKey(wrapping, header.wrappedMaster, m_master.data(), error)) {
        CryptoService::wipe(wrapping, sizeof(wrapping));
        m_master.reset();
        return false;
    }
    CryptoService::wipe(wrapping, sizeof(wrapping));

    if (!m_index.load(path, m_master.data(), error)) {
        m_master.reset();
        return false;
    }
    m_store = std::make_unique<FolderBlobStore>();
    if (!m_store->openStore(path, error)) {
        m_master.reset();
        m_index.clear();
        return false;
    }
    m_vaultPath = path;
    m_currentFolderId.clear();
    m_unlocked = true;
    return true;
}

bool VaultSession::changePasswordImpl(QString oldPassword, QString newPassword, QString *error)
{
    if (!m_unlocked || !m_master.valid()) {
        if (error)
            *error = QStringLiteral("Unlock the vault first");
        return false;
    }
    if (newPassword.size() < 8) {
        if (error)
            *error = QStringLiteral("Password must be at least 8 characters");
        return false;
    }

    VaultHeader current;
    if (!VaultFormat::readHeader(m_vaultPath, &current, error))
        return false;

    unsigned char wrapping[CryptoService::kKeyBytes];
    unsigned char check[CryptoService::kKeyBytes];
    if (!CryptoService::deriveWrappingKey(oldPassword, current.salt, current.kdf, wrapping, error)
        || !VaultFormat::unwrapMasterKey(wrapping, current.wrappedMaster, check, error)
        || sodium_memcmp(check, m_master.data(), CryptoService::kKeyBytes) != 0) {
        CryptoService::wipe(wrapping, sizeof(wrapping));
        CryptoService::wipe(check, sizeof(check));
        if (error)
            *error = QStringLiteral("Current password is incorrect");
        return false;
    }
    CryptoService::wipe(wrapping, sizeof(wrapping));
    CryptoService::wipe(check, sizeof(check));

    VaultHeader next;
    next.version = VaultFormat::kVersion;
    next.kdf = CryptoService::interactiveKdf();
    next.salt = CryptoService::randomBytes(CryptoService::kSaltBytes);
    if (!CryptoService::deriveWrappingKey(newPassword, next.salt, next.kdf, wrapping, error)) {
        CryptoService::wipe(wrapping, sizeof(wrapping));
        return false;
    }
    const bool wrapped = VaultFormat::wrapMasterKey(wrapping, m_master.data(), &next.wrappedMaster, error);
    CryptoService::wipe(wrapping, sizeof(wrapping));
    if (!wrapped)
        return false;
    return VaultFormat::writeHeader(m_vaultPath, next, error);
}

bool VaultSession::pathsAreOurStaging(const QStringList &paths) const
{
    if (m_clipExportDir.isEmpty() || paths.isEmpty())
        return false;
    for (const QString &p : paths) {
        if (!pathIsInside(m_clipExportDir, p))
            return false;
    }
    return true;
}

bool VaultSession::readOsClipboardImport(QStringList *paths, bool *move) const
{
    if (paths)
        paths->clear();
    if (move)
        *move = false;
    if (!QGuiApplication::instance())
        return false;
    const QMimeData *md = QGuiApplication::clipboard()->mimeData();
    if (!md || !md->hasUrls())
        return false;

    if (move) {
        const QByteArray effect = md->data(QLatin1String(kPreferredDropEffect));
        if (effect.size() >= 4) {
            quint32 value = 0;
            memcpy(&value, effect.constData(), 4);
            *move = (value == 2u); // DROPEFFECT_MOVE
        }
    }

    QStringList localPaths;
    for (const QUrl &url : md->urls()) {
        const QString local = toLocalPath(url.isLocalFile() ? url.toLocalFile() : url.toString());
        if (local.isEmpty() || !QFileInfo::exists(local))
            continue;
        if (pathIsInside(m_vaultPath, local))
            continue;
        localPaths.push_back(QDir::cleanPath(local));
    }
    if (localPaths.isEmpty() || pathsAreOurStaging(localPaths))
        return false;
    if (paths)
        *paths = localPaths;
    return true;
}

void VaultSession::removeOsSourcePaths(const QStringList &paths)
{
    for (const QString &raw : paths) {
        const QString path = QDir::cleanPath(toLocalPath(raw));
        if (path.isEmpty() || pathIsInside(m_vaultPath, path) || pathIsInside(m_clipExportDir, path))
            continue;
        const QFileInfo fi(path);
        if (!fi.exists())
            continue;
        if (fi.isDir())
            QDir(path).removeRecursively();
        else
            QFile::remove(path);
    }
}

bool VaultSession::importPathsImpl(QStringList paths, QString *error, bool move)
{
    if (!m_unlocked || !m_master.valid()) {
        if (error)
            *error = QStringLiteral("Vault is locked");
        return false;
    }

    struct Item {
        QString absPath;
        QString parentId;
        QString name;
        bool isDir = false;
    };

    QVector<Item> items;
    std::function<void(const QString &, const QString &)> walk;
    walk = [&](const QString &abs, const QString &parentId) {
        const QFileInfo fi(abs);
        if (!fi.exists() || pathIsInside(m_vaultPath, fi.absoluteFilePath()))
            return;
        if (fi.isDir()) {
            items.push_back({fi.absoluteFilePath(), parentId, fi.fileName(), true});
            const int dirIndex = items.size() - 1;
            QDir dir(fi.absoluteFilePath());
            const auto kids = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                                QDir::Name);
            for (const QFileInfo &child : kids)
                walk(child.absoluteFilePath(), QString::number(dirIndex));
        } else {
            items.push_back({fi.absoluteFilePath(), parentId, fi.fileName(), false});
        }
    };

    for (const QString &raw : paths) {
        const QString local = toLocalPath(raw);
        if (!local.isEmpty())
            walk(local, m_currentFolderId);
    }

    if (items.isEmpty()) {
        if (error)
            *error = QStringLiteral("Nothing to import");
        return false;
    }

    QVector<QString> assignedIds(items.size());
    int fileCount = 0;
    for (const Item &it : items) {
        if (!it.isDir)
            ++fileCount;
    }

    int filesDone = 0;
    for (int i = 0; i < items.size(); ++i) {
        Item it = items[i];
        QString parentId = it.parentId;
        bool numeric = false;
        const int parentIndex = parentId.toInt(&numeric);
        if (numeric && parentIndex >= 0 && parentIndex < assignedIds.size())
            parentId = assignedIds[parentIndex];
        if (parentId.isEmpty() && numeric)
            parentId = m_currentFolderId;

        VaultEntry e;
        e.id = newEntryId();
        e.parentId = parentId;
        e.name = uniqueName(parentId, it.name);
        e.isDir = it.isDir;
        e.modifiedMs = QFileInfo(it.absPath).lastModified().toMSecsSinceEpoch();
        assignedIds[i] = e.id;

        if (it.isDir) {
            m_index.entries().push_back(e);
            continue;
        }

        setProgress(fileCount == 0 ? 1.0 : double(filesDone) / double(fileCount),
                    QStringLiteral("Encrypting %1").arg(it.name));
        if (!encryptFileToBlob(it.absPath, &e, error))
            return false;
        m_index.entries().push_back(e);
        ++filesDone;
        setProgress(fileCount == 0 ? 1.0 : double(filesDone) / double(fileCount),
                    QStringLiteral("Encrypted %1").arg(it.name));
    }

    if (!saveIndex(error))
        return false;
    if (move)
        removeOsSourcePaths(paths);
    return true;
}

bool VaultSession::exportEntriesImpl(QStringList ids, QString destDir, QString *error)
{
    destDir = toLocalPath(destDir);
    if (destDir.isEmpty() || !QDir().mkpath(destDir)) {
        if (error)
            *error = QStringLiteral("Cannot create export folder");
        return false;
    }

    std::function<bool(const VaultEntry &, const QString &)> exportOne;
    exportOne = [&](const VaultEntry &e, const QString &into) -> bool {
        const QString dest = QDir(into).filePath(e.name);
        if (e.isDir) {
            if (!QDir().mkpath(dest)) {
                if (error)
                    *error = QStringLiteral("Cannot create folder %1").arg(e.name);
                return false;
            }
            for (const VaultEntry &child : m_index.entries()) {
                if (child.parentId == e.id) {
                    if (!exportOne(child, dest))
                        return false;
                }
            }
            return true;
        }
        setProgress(0, QStringLiteral("Decrypting %1").arg(e.name));
        return decryptBlobToFile(e, dest, error);
    };

    for (const QString &id : ids) {
        const VaultEntry *e = m_index.find(id);
        if (!e)
            continue;
        if (!exportOne(*e, destDir))
            return false;
    }
    return true;
}

bool VaultSession::deleteEntriesImpl(QStringList ids, QString *error)
{
    QSet<QString> drop;
    for (const QString &id : ids) {
        drop.insert(id);
        for (const QString &d : descendants(id))
            drop.insert(d);
    }
    if (drop.contains(m_currentFolderId))
        m_currentFolderId.clear();

    QVector<VaultEntry> kept;
    kept.reserve(m_index.entries().size());
    for (const VaultEntry &e : m_index.entries()) {
        if (!drop.contains(e.id)) {
            kept.push_back(e);
            continue;
        }
        if (!e.isDir && !e.blobId.isEmpty()) {
            if (!m_store->removeBlob(e.blobId, error))
                return false;
        }
    }
    m_index.entries() = kept;
    return saveIndex(error);
}

bool VaultSession::pasteImpl(QString *error)
{
    if (m_clipIds.isEmpty() || m_clipMode == ClipMode::None) {
        if (error)
            *error = QStringLiteral("Clipboard is empty");
        return false;
    }
    if (m_clipMode == ClipMode::Cut) {
        for (const QString &id : m_clipIds) {
            VaultEntry *e = m_index.find(id);
            if (!e)
                continue;
            if (e->id == m_currentFolderId || descendants(e->id).contains(m_currentFolderId)) {
                if (error)
                    *error = QStringLiteral("Cannot move a folder into itself");
                return false;
            }
            if (e->parentId == m_currentFolderId)
                continue;
            e->parentId = m_currentFolderId;
            e->name = uniqueName(m_currentFolderId, e->name, e->id);
            e->modifiedMs = QDateTime::currentMSecsSinceEpoch();
        }
        m_clipIds.clear();
        m_clipMode = ClipMode::None;
        const auto cancel = [this]() { cancelOsClipboardMove(); };
        if (QThread::currentThread() == thread())
            cancel();
        else
            QMetaObject::invokeMethod(this, cancel, Qt::QueuedConnection);
        return saveIndex(error);
    }

    for (const QString &id : m_clipIds) {
        if (!copySubtree(id, m_currentFolderId, error))
            return false;
    }
    return saveIndex(error);
}

void VaultSession::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
    if (!busy)
        setProgress(0, {});
}

void VaultSession::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void VaultSession::setProgress(double value, const QString &text)
{
    m_progress = value;
    m_progressText = text;
    emit progressChanged();
}

void VaultSession::setUnlocked(bool unlocked)
{
    if (m_unlocked == unlocked)
        return;
    m_unlocked = unlocked;
    emit unlockedChanged();
}

void VaultSession::refreshModel()
{
    QVector<VaultEntry> visible;
    const QString q = m_searchQuery.trimmed();
    if (!q.isEmpty()) {
        for (const VaultEntry &e : m_index.entries()) {
            if (e.name.contains(q, Qt::CaseInsensitive))
                visible.push_back(e);
        }
    } else {
        for (const VaultEntry &e : m_index.entries()) {
            if (e.parentId == m_currentFolderId)
                visible.push_back(e);
        }
    }
    applySort(&visible);
    m_files.setEntries(std::move(visible));
    emit statsChanged();
    emit currentFolderChanged();
    emit vaultChanged();
}

void VaultSession::touchIdleTimer()
{
    if (m_idleTimeoutMs > 0)
        m_idleTimer.start(m_idleTimeoutMs);
}

void VaultSession::wipeOpenedTemps()
{
    for (const QString &p : m_openedTemps)
        secureDeleteFile(p);
    m_openedTemps.clear();
    if (!m_openDir.isEmpty()) {
        QDir dir(m_openDir);
        dir.removeRecursively();
        m_openDir.clear();
    }
}

void VaultSession::closeVaultState()
{
    finishOsCutIfConsumed();
    stopOsCutWatcher();
    flushPendingOsCutDelete();
    m_clipExportGen.fetchAndAddRelaxed(1);
    QString clipDir;
    {
        QMutexLocker locker(&m_ioMutex);
        clipDir = m_clipExportDir;
        m_clipExportDir.clear();
        wipeDirSecure(clipDir);
        wipeOpenedTemps();
        m_index.clear();
        m_master.reset();
        if (m_store)
            m_store->closeStore();
    }
    clearClipboardIfUrlsUnder(clipDir);
    m_currentFolderId.clear();
    m_searchQuery.clear();
    m_clipIds.clear();
    m_clipMode = ClipMode::None;
    m_idleTimer.stop();
    const bool wasUnlocked = m_unlocked;
    m_unlocked = false;
    refreshModel();
    emit clipboardChanged();
    emit searchQueryChanged();
    emit vaultPrefsChanged();
    if (wasUnlocked)
        emit unlockedChanged();
}

void VaultSession::runAsync(const std::function<bool(QString *)> &fn)
{
    if (m_busy)
        return;
    setLastError({});
    setBusy(true);
    (void)QtConcurrent::run([this, fn]() {
        QString err;
        const bool ok = fn(&err);
        QMetaObject::invokeMethod(
            this,
            [this, ok, err]() {
                setBusy(false);
                if (ok) {
                    if (m_master.valid()) {
                        m_unlocked = true;
                        emit unlockedChanged();
                        applyPrefs();
                        emit vaultPrefsChanged();
                    }
                    emit vaultPathChanged();
                    refreshModel();
                    if (m_unlocked)
                        touchIdleTimer();
                } else {
                    setLastError(err);
                    if (!m_unlocked)
                        closeVaultState();
                }
                flushPendingOsCutDelete();
                if (m_lockAfterBusy) {
                    m_lockAfterBusy = false;
                    lock();
                }
            },
            Qt::QueuedConnection);
    });
}

QString VaultSession::toLocalPath(const QString &pathOrUrl) const
{
    const QUrl url(pathOrUrl);
    if (url.isLocalFile())
        return QDir::cleanPath(url.toLocalFile());
    if (pathOrUrl.startsWith(QLatin1String("file:")))
        return QDir::cleanPath(QUrl(pathOrUrl).toLocalFile());
    return QDir::cleanPath(pathOrUrl);
}

QString VaultSession::uniqueName(const QString &parentId, const QString &desired,
                                const QString &ignoreId) const
{
    auto taken = [&](const QString &name) {
        for (const VaultEntry &e : m_index.entries()) {
            if (!ignoreId.isEmpty() && e.id == ignoreId)
                continue;
            if (e.parentId == parentId && e.name.compare(name, Qt::CaseInsensitive) == 0)
                return true;
        }
        return false;
    };
    if (!taken(desired))
        return desired;

    QString base = desired;
    QString ext;
    const int dot = desired.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        base = desired.left(dot);
        ext = desired.mid(dot);
    }
    for (int i = 1; i < 10000; ++i) {
        const QString cand = base + QStringLiteral(" (") + QString::number(i) + QStringLiteral(")") + ext;
        if (!taken(cand))
            return cand;
    }
    return desired + QStringLiteral(" (copy)");
}

void VaultSession::stopOsCutWatcher()
{
    m_clipPollTimer.stop();
    const QStringList dirs = m_clipWatcher.directories();
    const QStringList files = m_clipWatcher.files();
    if (!dirs.isEmpty())
        m_clipWatcher.removePaths(dirs);
    if (!files.isEmpty())
        m_clipWatcher.removePaths(files);
    m_clipOsIds.clear();
    m_clipOsTopPaths.clear();
}

void VaultSession::armOsCutWatcher(const QString &dir, const QStringList &ids, const QStringList &topPaths)
{
    stopOsCutWatcher();
    if (ids.isEmpty() || topPaths.isEmpty() || dir.isEmpty())
        return;
    m_clipOsIds = ids;
    m_clipOsTopPaths = topPaths;
    m_clipWatcher.addPath(dir);
    for (const QString &p : topPaths) {
        if (QFileInfo::exists(p))
            m_clipWatcher.addPath(p);
    }
    m_clipPollTimer.start();
}

void VaultSession::cancelOsClipboardMove()
{
    stopOsCutWatcher();
    m_clipExportGen.fetchAndAddRelaxed(1);
    QString dir;
    {
        QMutexLocker locker(&m_ioMutex);
        dir = m_clipExportDir;
        m_clipExportDir.clear();
        wipeDirSecure(dir);
    }
    clearClipboardIfUrlsUnder(dir);
}

void VaultSession::onOsClipDirChanged()
{
    finishOsCutIfConsumed();
}

void VaultSession::finishOsCutIfConsumed()
{
    if (m_clipOsIds.isEmpty() || m_clipOsTopPaths.isEmpty())
        return;
    for (const QString &p : m_clipOsTopPaths) {
        if (QFileInfo::exists(p))
            return;
    }

    const QStringList ids = m_clipOsIds;
    stopOsCutWatcher();
    m_clipIds.clear();
    m_clipMode = ClipMode::None;
    emit clipboardChanged();

    if (m_busy || !m_unlocked || !m_master.valid()) {
        m_pendingOsCutDelete = ids;
        return;
    }

    QString err;
    if (!deleteEntriesImpl(ids, &err))
        setLastError(err);
    refreshModel();
}

void VaultSession::flushPendingOsCutDelete()
{
    if (m_pendingOsCutDelete.isEmpty() || !m_master.valid()) {
        m_pendingOsCutDelete.clear();
        return;
    }
    const QStringList ids = m_pendingOsCutDelete;
    m_pendingOsCutDelete.clear();
    QString err;
    if (!deleteEntriesImpl(ids, &err))
        setLastError(err);
    if (m_unlocked)
        refreshModel();
}

void VaultSession::startOsClipboardExport(const QStringList &ids, bool move)
{
    finishOsCutIfConsumed();
    stopOsCutWatcher();
    if (!QGuiApplication::instance() || ids.isEmpty() || !m_unlocked || !m_master.valid())
        return;

    struct ClipItem {
        QString relPath;
        QString blobId;
        bool isDir = false;
    };

    QVector<ClipItem> snapshot;
    QSet<QString> usedTop;
    std::function<void(const VaultEntry &, const QString &)> walk;
    walk = [&](const VaultEntry &e, const QString &prefix) {
        const QString rel = prefix.isEmpty() ? e.name : prefix + QLatin1Char('/') + e.name;
        snapshot.push_back({rel, e.blobId, e.isDir});
        if (!e.isDir)
            return;
        for (const VaultEntry &child : m_index.entries()) {
            if (child.parentId == e.id)
                walk(child, rel);
        }
    };
    for (const QString &id : ids) {
        const VaultEntry *e = m_index.find(id);
        if (!e)
            continue;
        QString topName = e->name;
        if (usedTop.contains(topName.toLower())) {
            VaultEntry renamed = *e;
            int n = 1;
            while (usedTop.contains(topName.toLower()) && n < 10000) {
                QString base = e->name;
                QString ext;
                const int dot = e->name.lastIndexOf(QLatin1Char('.'));
                if (dot > 0 && !e->isDir) {
                    base = e->name.left(dot);
                    ext = e->name.mid(dot);
                }
                topName = base + QStringLiteral(" (") + QString::number(n++) + QStringLiteral(")") + ext;
            }
            renamed.name = topName;
            usedTop.insert(topName.toLower());
            walk(renamed, {});
        } else {
            usedTop.insert(topName.toLower());
            walk(*e, {});
        }
    }
    if (snapshot.isEmpty())
        return;

    const int gen = m_clipExportGen.fetchAndAddRelaxed(1) + 1;
    const QString dir = QDir::temp().filePath(
        QStringLiteral("SecureVault-clip-") + QUuid::createUuid().toString(QUuid::WithoutBraces));

    (void)QtConcurrent::run([this, snapshot, gen, dir, ids, move]() {
        QString err;
        bool ok = true;
        for (const ClipItem &item : snapshot) {
            if (gen != m_clipExportGen.loadRelaxed()) {
                ok = false;
                break;
            }
            const QString dest = QDir(dir).filePath(item.relPath);
            if (item.isDir) {
                if (!QDir().mkpath(dest)) {
                    ok = false;
                    break;
                }
                continue;
            }
            VaultEntry tmp;
            tmp.blobId = item.blobId;
            if (!decryptBlobToFile(tmp, dest, &err)) {
                ok = false;
                break;
            }
        }
        if (!ok || gen != m_clipExportGen.loadRelaxed()) {
            wipeDirSecure(dir);
            return;
        }
        QPointer<VaultSession> self(this);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, gen, dir, ids, move]() {
                if (!self || gen != self->m_clipExportGen.loadRelaxed() || !self->m_unlocked) {
                    wipeDirSecure(dir);
                    return;
                }
                QStringList paths;
                const QFileInfoList entries = QDir(dir).entryInfoList(
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &fi : entries)
                    paths.push_back(fi.absoluteFilePath());
                if (paths.isEmpty()) {
                    wipeDirSecure(dir);
                    return;
                }
                {
                    QMutexLocker locker(&self->m_ioMutex);
                    if (!self->m_clipExportDir.isEmpty() && self->m_clipExportDir != dir)
                        wipeDirSecure(self->m_clipExportDir);
                    self->m_clipExportDir = dir;
                }
                auto *mime = new QMimeData;
                QList<QUrl> urls;
                urls.reserve(paths.size());
                for (const QString &p : paths)
                    urls.push_back(QUrl::fromLocalFile(p));
                mime->setUrls(urls);
                QByteArray effect(4, '\0');
                const quint32 dropEffect = move ? 2u : 1u; // DROPEFFECT_MOVE : DROPEFFECT_COPY
                memcpy(effect.data(), &dropEffect, sizeof(dropEffect));
                mime->setData(QLatin1String(kPreferredDropEffect), effect);
                QGuiApplication::clipboard()->setMimeData(mime);
                if (move)
                    self->armOsCutWatcher(dir, ids, paths);
            },
            Qt::QueuedConnection);
    });
}

QString VaultSession::newEntryId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QStringList VaultSession::descendants(const QString &id) const
{
    QStringList out;
    QStringList stack{id};
    while (!stack.isEmpty()) {
        const QString cur = stack.takeLast();
        for (const VaultEntry &e : m_index.entries()) {
            if (e.parentId == cur) {
                out.push_back(e.id);
                if (e.isDir)
                    stack.push_back(e.id);
            }
        }
    }
    return out;
}

bool VaultSession::saveIndex(QString *error)
{
    return m_index.save(m_vaultPath, m_master.data(), error);
}

bool VaultSession::encryptFileToBlob(const QString &srcPath, VaultEntry *entry, QString *error)
{
    QMutexLocker locker(&m_ioMutex);
    if (!m_master.valid() || !m_store) {
        if (error)
            *error = QStringLiteral("Vault is locked");
        return false;
    }
    QFile in(srcPath);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot read %1").arg(srcPath);
        return false;
    }
    entry->blobId = QString::fromLatin1(CryptoService::randomBytes(16).toHex());
    entry->size = in.size();
    unsigned char fileKey[CryptoService::kKeyBytes];
    CryptoService::deriveFileKey(m_master.data(), entry->blobId.toLatin1(), fileKey);
    const bool ok = m_store->writeBlob(entry->blobId, &in, fileKey, error);
    CryptoService::wipe(fileKey, sizeof(fileKey));
    return ok;
}

bool VaultSession::decryptBlobToFile(const VaultEntry &entry, const QString &destPath, QString *error)
{
    QMutexLocker locker(&m_ioMutex);
    if (!m_master.valid() || !m_store) {
        if (error)
            *error = QStringLiteral("Vault is locked");
        return false;
    }
    QFileInfo fi(destPath);
    QDir().mkpath(fi.absolutePath());
    QSaveFile out(destPath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot write %1").arg(destPath);
        return false;
    }
    unsigned char fileKey[CryptoService::kKeyBytes];
    CryptoService::deriveFileKey(m_master.data(), entry.blobId.toLatin1(), fileKey);
    const bool ok = m_store->readBlob(entry.blobId, &out, fileKey, error);
    CryptoService::wipe(fileKey, sizeof(fileKey));
    if (!ok) {
        out.cancelWriting();
        return false;
    }
    return out.commit();
}

bool VaultSession::copySubtree(const QString &sourceId, const QString &destParentId, QString *error)
{
    const VaultEntry *src = m_index.find(sourceId);
    if (!src)
        return true;

    VaultEntry copy = *src;
    copy.id = newEntryId();
    copy.parentId = destParentId;
    copy.name = uniqueName(destParentId, src->name);
    copy.modifiedMs = QDateTime::currentMSecsSinceEpoch();

    if (!src->isDir) {
        const QString tmpPath = QDir::temp().filePath(
            QStringLiteral("sv-copy-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (!decryptBlobToFile(*src, tmpPath, error))
            return false;
        const bool ok = encryptFileToBlob(tmpPath, &copy, error);
        secureDeleteFile(tmpPath);
        if (!ok)
            return false;
        m_index.entries().push_back(copy);
        return true;
    }

    copy.blobId.clear();
    copy.size = 0;
    m_index.entries().push_back(copy);
    const QString newParent = copy.id;
    QStringList childIds;
    for (const VaultEntry &e : m_index.entries()) {
        if (e.parentId == sourceId)
            childIds.push_back(e.id);
    }
    for (const QString &cid : childIds) {
        if (!copySubtree(cid, newParent, error))
            return false;
    }
    return true;
}

void VaultSession::applySort(QVector<VaultEntry> *list) const
{
    const QString key = m_sortKey;
    const bool asc = m_sortAscending;
    std::stable_sort(list->begin(), list->end(), [&](const VaultEntry &a, const VaultEntry &b) {
        if (a.isDir != b.isDir)
            return a.isDir;
        int cmp = 0;
        if (key == QLatin1String("date"))
            cmp = a.modifiedMs < b.modifiedMs ? -1 : (a.modifiedMs > b.modifiedMs ? 1 : 0);
        else if (key == QLatin1String("size"))
            cmp = a.size < b.size ? -1 : (a.size > b.size ? 1 : 0);
        if (cmp == 0)
            cmp = a.name.compare(b.name, Qt::CaseInsensitive);
        return asc ? cmp < 0 : cmp > 0;
    });
}

bool VaultSession::openEntryToTemp(const VaultEntry &entry, QString *error)
{
    if (m_openDir.isEmpty()) {
        m_openDir = QDir::temp().filePath(QStringLiteral("SecureVault-")
                                          + QUuid::createUuid().toString(QUuid::WithoutBraces));
        QDir().mkpath(m_openDir);
    }
    const QString dest = QDir(m_openDir).filePath(entry.name);
    if (!decryptBlobToFile(entry, dest, error))
        return false;
    QFile::setPermissions(dest, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    m_openedTemps.push_back(dest);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dest))) {
        if (error)
            *error = QStringLiteral("Could not open the file with a system app");
        return false;
    }
    return true;
}
