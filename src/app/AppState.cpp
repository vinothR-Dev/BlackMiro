#include "AppState.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QStyleHints>

AppState::AppState(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_themeMode = s.value(QStringLiteral("theme/mode"), QStringLiteral("system")).toString();
    m_lastEmail = s.value(QStringLiteral("profile/lastEmail")).toString();
    m_remember = s.value(QStringLiteral("profile/remember"), true).toBool();
    if (m_remember)
        m_email = s.value(QStringLiteral("profile/email")).toString();
    m_recent = s.value(QStringLiteral("recent/boards")).toList();
    m_pinned = s.value(QStringLiteral("toolbar/pinned"),
                       QStringList{ "templates", "sticky", "text", "shapes", "pen", "frame", "connector", "emoji", "image" })
                   .toStringList();

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &AppState::updateDark);
    applyTheme();
}

void AppState::setThemeMode(const QString &mode)
{
    const QString m = (mode == QLatin1String("light") || mode == QLatin1String("dark")) ? mode : QStringLiteral("system");
    if (m == m_themeMode)
        return;
    m_themeMode = m;
    QSettings().setValue(QStringLiteral("theme/mode"), m_themeMode);
    applyTheme();
    emit themeChanged();
}

void AppState::applyTheme()
{
    QStyleHints *hints = QGuiApplication::styleHints();
    if (m_themeMode == QLatin1String("light"))
        hints->setColorScheme(Qt::ColorScheme::Light);
    else if (m_themeMode == QLatin1String("dark"))
        hints->setColorScheme(Qt::ColorScheme::Dark);
    else
        hints->unsetColorScheme(); // follow the operating system
    updateDark();
}

void AppState::updateDark()
{
    bool d;
    if (m_themeMode == QLatin1String("dark"))
        d = true;
    else if (m_themeMode == QLatin1String("light"))
        d = false;
    else
        d = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    if (d != m_dark) {
        m_dark = d;
        emit themeChanged();
    }
}

bool AppState::isValidEmail(const QString &email) const
{
    static const QRegularExpression re(QStringLiteral(R"(^[^\s@]+@[^\s@]+\.[^\s@]{2,}$)"));
    return re.match(email.trimmed()).hasMatch();
}

bool AppState::signIn(const QString &email, bool remember)
{
    const QString e = email.trimmed().toLower();
    if (!isValidEmail(e))
        return false;
    m_email = e;
    m_lastEmail = e;
    m_remember = remember;
    QSettings s;
    s.setValue(QStringLiteral("profile/lastEmail"), e);
    s.setValue(QStringLiteral("profile/remember"), remember);
    if (remember)
        s.setValue(QStringLiteral("profile/email"), e);
    else
        s.remove(QStringLiteral("profile/email"));
    emit profileChanged();
    return true;
}

void AppState::addRecent(const QString &path, const QString &title, const QString &thumb)
{
    if (path.isEmpty())
        return;
    removeRecent(path);
    m_recent.prepend(QVariantMap{ { "path", path }, { "title", title }, { "thumb", thumb },
                                  { "time", QDateTime::currentDateTime() } });
    while (m_recent.size() > 24)
        m_recent.removeLast();
    QSettings().setValue(QStringLiteral("recent/boards"), m_recent);
    emit recentChanged();
}

void AppState::removeRecent(const QString &path)
{
    for (qsizetype i = 0; i < m_recent.size(); ++i)
        if (m_recent[i].toMap().value("path").toString() == path) {
            m_recent.removeAt(i);
            QSettings().setValue(QStringLiteral("recent/boards"), m_recent);
            emit recentChanged();
            return;
        }
}

void AppState::setPinnedTools(const QStringList &ids)
{
    if (ids == m_pinned)
        return;
    m_pinned = ids;
    QSettings().setValue(QStringLiteral("toolbar/pinned"), m_pinned);
    emit pinnedToolsChanged();
}

void AppState::setToolPinned(const QString &id, bool pinned)
{
    QStringList ids = m_pinned;
    ids.removeAll(id);
    if (pinned)
        ids << id;
    setPinnedTools(ids);
}

void AppState::signOut()
{
    m_email.clear();
    QSettings().remove(QStringLiteral("profile/email"));
    emit profileChanged();
}
