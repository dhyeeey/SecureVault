#include "app/AppSettings.h"
#include "crypto/CryptoService.h"
#include "vault/VaultSession.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <cstdio>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SecureVault"));
    app.setOrganizationName(QStringLiteral("SecureVault"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    if (!CryptoService::init()) {
        std::fprintf(stderr, "Failed to initialize libsodium\n");
        return 1;
    }

    VaultSession session;
    session.installIdleGuard(&app);

    AppSettings settings(session.defaultVaultPath());
    session.setVaultPath(settings.vaultPath());
    QObject::connect(&settings, &AppSettings::vaultPathChanged, &session, [&]() {
        session.setVaultPath(settings.vaultPath());
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("vault"), &session);
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &settings);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("SecureVault", "Main");

    return QGuiApplication::exec();
}
