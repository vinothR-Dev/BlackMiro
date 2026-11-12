#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>
#include <QUrl>

#include "LibraryPreviewProvider.h"
#include "TemplatePreviewProvider.h"

Q_IMPORT_QML_PLUGIN(MiruStylePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Miru"));
    QGuiApplication::setOrganizationName(QStringLiteral("Miru"));
    // Isolated settings (profile, theme) for automated screenshot runs.
    if (qEnvironmentVariableIsSet("MIRU_SETTINGS_ORG")) {
        QGuiApplication::setOrganizationName(qEnvironmentVariable("MIRU_SETTINGS_ORG"));
    } else {
        // The app was called BlackMiro in v1.0: carry its settings (profile, theme,
        // recent boards) and data folder (templates, saved files) over once.
        QSettings now;
        if (now.allKeys().isEmpty()) {
            QSettings old(QStringLiteral("BlackMiro"), QStringLiteral("BlackMiro"));
            for (const QString &k : old.allKeys())
                now.setValue(k, old.value(k));
        }
        const QString newData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString oldData = newData;
        oldData.replace(QStringLiteral("/Miru/Miru"), QStringLiteral("/BlackMiro/BlackMiro"));
        if (oldData != newData && QDir(oldData).exists() && !QDir(newData).exists()) {
            QDir().mkpath(QFileInfo(newData).path());
            QDir().rename(oldData, newData);
        }
    }
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Miru/assets/miru-icon.svg")));
    QQuickStyle::setStyle(QStringLiteral("MiruStyle"));
    QFont uiFont = QGuiApplication::font();
    uiFont.setPixelSize(14);
    QGuiApplication::setFont(uiFont);
    // Light / dark / system theme is applied by the AppState singleton (theme button).

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("templates"), new TemplatePreviewProvider);
    engine.addImageProvider(QStringLiteral("shapes"), new LibraryPreviewProvider(false));
    engine.addImageProvider(QStringLiteral("icons"), new LibraryPreviewProvider(true));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Miru", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;
    QObject *root = engine.rootObjects().first();

    // Usage: Miru [file.board | image files...] [--screenshot out.png]
    QStringList args = QCoreApplication::arguments().mid(1);
    QString screenshot;
    const qsizetype shotIdx = args.indexOf(QStringLiteral("--screenshot"));
    if (shotIdx >= 0 && shotIdx + 1 < args.size()) {
        screenshot = args.at(shotIdx + 1);
        args.remove(shotIdx, 2);
    }
    // Files from the command line / "Open with": a board opens; images are placed
    // in a horizontal row, exactly like a drag & drop.
    const bool openExport = args.removeAll(QStringLiteral("--open-export")) > 0;
    // Automated two-window collaboration test:
    //   --host-session <file>   start hosting and write the invite link to <file>
    //   --join-session <link>   join that session
    //   --screenshot-delay <ms>
    auto takeArg = [&args](const QString &name) {
        const qsizetype i = args.indexOf(name);
        if (i < 0 || i + 1 >= args.size())
            return QString();
        const QString v = args.at(i + 1);
        args.remove(i, 2);
        return v;
    };
    const QString hostOut = takeArg(QStringLiteral("--host-session"));
    const QString joinLink = takeArg(QStringLiteral("--join-session"));
    const int shotDelay = takeArg(QStringLiteral("--screenshot-delay")).toInt();
    const int callDelay = takeArg(QStringLiteral("--call-delay")).toInt();
    // --call <qmlFunction>: open a screen for automated screenshots (e.g. openSignIn).
    QString callFn;
    const qsizetype callIdx = args.indexOf(QStringLiteral("--call"));
    if (callIdx >= 0 && callIdx + 1 < args.size()) {
        callFn = args.at(callIdx + 1);
        args.remove(callIdx, 2);
    }
    if (!args.isEmpty()) {
        QVariantList urls;
        for (const QString &a : std::as_const(args))
            urls << QUrl::fromLocalFile(a);
        QMetaObject::invokeMethod(root, "openPaths", Q_ARG(QVariant, urls));
    }

    if (openExport)
        QMetaObject::invokeMethod(root, "openExport");
    if (!callFn.isEmpty())
        QTimer::singleShot(callDelay > 0 ? callDelay : 300, root, [root, callFn]() { QMetaObject::invokeMethod(root, callFn.toLatin1().constData()); });

    if (!hostOut.isEmpty())
        QTimer::singleShot(800, root, [root, hostOut]() {
            QObject *collab = root->property("collabRef").value<QObject *>();
            QMetaObject::invokeMethod(collab, "startHosting");
            QTimer::singleShot(1500, collab, [collab, hostOut]() {
                QFile f(hostOut);
                if (f.open(QIODevice::WriteOnly))
                    f.write(collab->property("inviteLink").toString().toUtf8());
            });
        });
    if (!joinLink.isEmpty())
        QTimer::singleShot(800, root, [root, joinLink]() {
            QObject *collab = root->property("collabRef").value<QObject *>();
            QMetaObject::invokeMethod(collab, "join", Q_ARG(QString, joinLink));
        });

    if (!screenshot.isEmpty()) {
        // Headless smoke check / docs: grab the rendered window and quit.
        QTimer::singleShot(shotDelay > 0 ? shotDelay : 2000, &app, [root, screenshot]() {
            if (auto *w = qobject_cast<QQuickWindow *>(root))
                w->grabWindow().save(screenshot);
            root->setProperty("closeConfirmed", true);   // no "save changes?" prompt
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
