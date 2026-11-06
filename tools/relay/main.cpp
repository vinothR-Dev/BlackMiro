// Standalone BlackMiro relay for live collaboration on a server or always-on PC.
// Usage: BlackMiroRelay [--port 8765]
// The relay only forwards end-to-end encrypted frames; it can't read boards.
#include "RelayServer.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("BlackMiroRelay"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("BlackMiro live-collaboration relay"));
    parser.addHelpOption();
    QCommandLineOption portOpt(QStringLiteral("port"), QStringLiteral("TCP port to listen on."), QStringLiteral("port"),
                               QStringLiteral("8765"));
    parser.addOption(portOpt);
    parser.process(app);

    RelayServer relay;
    const quint16 port = quint16(parser.value(portOpt).toUInt());
    QTextStream out(stdout);
    if (!relay.listen(port)) {
        out << "Could not listen on port " << port << Qt::endl;
        return 1;
    }
    out << "BlackMiro relay listening on port " << relay.port() << Qt::endl;
    QObject::connect(&relay, &RelayServer::clientCountChanged, [&out](int n) {
        out << "clients: " << n << Qt::endl;
    });
    return app.exec();
}
