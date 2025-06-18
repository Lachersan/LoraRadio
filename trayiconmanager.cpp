#include "trayiconmanager.h"
#include <QAction>
#include <QApplication>

TrayIconManager::TrayIconManager(QObject *parent)
    : QObject(parent),
      m_trayIcon(new QSystemTrayIcon(QIcon(":/icons/icon.png"), this)),
      m_menu(new QMenu)
{
    // Действие "Показать"
    QAction *showAction = new QAction(tr("Показать"), this);
    connect(showAction, &QAction::triggered, this, &TrayIconManager::showRequested);

    // Действие "Выход"
    QAction *quitAction = new QAction(tr("Выход"), this);
    connect(quitAction, &QAction::triggered, this, &TrayIconManager::quitRequested);

    m_menu->addAction(showAction);
    m_menu->addAction(quitAction);

    m_trayIcon->setContextMenu(m_menu);
    m_trayIcon->setToolTip(tr("Lofi Radio Player"));
    m_trayIcon->show();

    // Двойной клик на иконку
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r){
        if (r == QSystemTrayIcon::Trigger)
            emit showRequested();
    });
}