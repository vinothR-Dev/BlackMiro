#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// App-wide state shared by every screen: colour theme and the local email profile.
//
// Sign-in is email-only and password-less. There is no server, so the email is a
// local profile (stored on this device, used as the board owner) - it is not verified.
class AppState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeChanged)   // system | light | dark
    Q_PROPERTY(bool dark READ dark NOTIFY themeChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY profileChanged)
    Q_PROPERTY(QString email READ email NOTIFY profileChanged)
    Q_PROPERTY(QString lastEmail READ lastEmail NOTIFY profileChanged)
    Q_PROPERTY(bool rememberMe READ rememberMe NOTIFY profileChanged)
    Q_PROPERTY(QVariantList recentBoards READ recentBoards NOTIFY recentChanged)   // [{path,title,thumb,time}]
    Q_PROPERTY(QStringList pinnedTools READ pinnedTools WRITE setPinnedTools NOTIFY pinnedToolsChanged)

public:
    explicit AppState(QObject *parent = nullptr);

    QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString &mode);
    bool dark() const { return m_dark; }

    bool signedIn() const { return !m_email.isEmpty(); }
    QString email() const { return m_email; }
    QString lastEmail() const { return m_lastEmail; }
    bool rememberMe() const { return m_remember; }

    Q_INVOKABLE bool isValidEmail(const QString &email) const;
    Q_INVOKABLE bool signIn(const QString &email, bool remember);
    Q_INVOKABLE void signOut();

    QVariantList recentBoards() const { return m_recent; }
    Q_INVOKABLE void addRecent(const QString &path, const QString &title, const QString &thumb);
    Q_INVOKABLE void removeRecent(const QString &path);

    QStringList pinnedTools() const { return m_pinned; }
    void setPinnedTools(const QStringList &ids);
    Q_INVOKABLE void setToolPinned(const QString &id, bool pinned);

signals:
    void themeChanged();
    void profileChanged();
    void recentChanged();
    void pinnedToolsChanged();

private:
    void applyTheme();
    void updateDark();

    QString m_themeMode = QStringLiteral("system");
    bool m_dark = false;
    QString m_email;
    QString m_lastEmail;
    bool m_remember = true;
    QVariantList m_recent;
    QStringList m_pinned;
};
