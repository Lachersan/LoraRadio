#ifndef TRAYICONMANAGER_H
#define TRAYICONMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>

class TrayIconManager : public QObject {
    Q_OBJECT
public:
    explicit TrayIconManager(QObject *parent = nullptr);
    ~TrayIconManager() = default;

    QSystemTrayIcon* icon() const { return m_trayIcon; }

    signals:
        void showRequested();
    void quitRequested();

private:
    QSystemTrayIcon *m_trayIcon;
    QMenu            *m_menu;
};

#endif // TRAYICONMANAGER_H