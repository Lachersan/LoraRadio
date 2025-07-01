#include "MainWindow.h"
#include "RadioPlayer.h"
#include "StationManager.h"
#include "IconButton.h"
#include "StationDialog.h"
#include "QuickControlPopup.h"
#include "fluent_icons.h"

#include <QListWidget>
#include <QSlider>
#include <QSpinBox>
#include <QMenuBar>
#include <QSystemTrayIcon>
#include <QAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSettings>
#include <QCursor>
#include <QCloseEvent>

MainWindow::MainWindow(StationManager *stations, QWidget *parent)
    : QMainWindow(parent),
      m_stations(stations),
      m_radio(new RadioPlayer(stations, this))

{
    setupUi();
    setupTray();
    setupConnections();
    onStationsChanged();
    onStationsChanged();

    QSettings s("MyApp", "LoraRadio");
    int vol = s.value("audio/volume", 5).toInt();
    m_volumeSlider->setValue(vol);
    m_volumeSpin->setValue(vol);


    setWindowTitle("LoraRadio");
    setWindowIcon(QIcon(":/icons/icon.png"));
    resize(700, 480);
}

void MainWindow::setupUi()
{
    m_listWidget   = new QListWidget;
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSpin   = new QSpinBox;
    m_volumeSlider->setRange(0, 100);
    m_volumeSpin->setRange(0, 100);

    const QSize icoSize(24,24);
    m_btnAdd       = new IconButton(fluent_icons::ic_fluent_add_circle_28_filled, 32, Qt::white, tr("Добавить"), this);
    m_btnRemove    = new IconButton("trash", icoSize, {{"color","#FFF"}}, tr("Удалить"), this);
    m_btnUpdate    = new IconButton("edit", icoSize, {{"color","#FFF"}}, tr("Изменить"), this);
    m_btnReconnect = new IconButton("sync-alt", icoSize, {{"color","#FFF"}}, tr("Переподключить"), this);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnRemove);
    btnLayout->addWidget(m_btnUpdate);
    btnLayout->addWidget(m_btnReconnect);

    auto *leftPanel = new QVBoxLayout;
    leftPanel->addWidget(m_listWidget);
    leftPanel->addLayout(btnLayout);

    auto *volLayout = new QHBoxLayout;
    volLayout->addWidget(m_volumeSlider);
    volLayout->addWidget(m_volumeSpin);

    auto *central = new QWidget;
    auto *mainL   = new QHBoxLayout(central);
    mainL->addLayout(leftPanel);
    mainL->addLayout(volLayout);

    setCentralWidget(central);
}

void MainWindow::setupTray()
{
    m_trayIcon = new QSystemTrayIcon(QIcon(":/icons/icon.png"), this);
    auto *menu = new QMenu(this);

    menu->addAction(tr("Показать"), this, &QWidget::showNormal);

    m_autostartAction = menu->addAction(tr("Автозапуск"));
    m_autostartAction->setCheckable(true);
    QSettings s("MyApp", "LoraRadio");
    m_autostartAction->setChecked(s.value("autostart/enabled", false).toBool());

    menu->addSeparator();
    menu->addAction(tr("Выход"), qApp, &QCoreApplication::quit);

    m_trayIcon->setContextMenu(menu);
    m_trayIcon->show();

    m_quickPopup = new QuickControlPopup(m_stations, this);
    m_quickPopup->setFixedSize(200, 150);

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this,       &MainWindow::onTrayActivated);
    connect(m_quickPopup, &QuickControlPopup::stationSelected,
        m_radio,     &RadioPlayer::selectStation);
    connect(m_stations, &StationManager::lastStationIndexChanged,
        m_quickPopup, &QuickControlPopup::setCurrentStation);
    connect(m_quickPopup, &QuickControlPopup::reconnectRequested,
            m_radio,     &RadioPlayer::reconnectStation);
    connect(m_quickPopup, &QuickControlPopup::volumeChanged,
            m_radio,     &RadioPlayer::changeVolume);
    connect(m_radio, &RadioPlayer::volumeChanged,
            m_quickPopup, &QuickControlPopup::setVolume);

}

void MainWindow::setupConnections()
{
    connect(m_radio, &RadioPlayer::stationsChanged,
            this,    &MainWindow::onStationsChanged);
    connect(m_listWidget, &QListWidget::currentRowChanged,
            m_radio, &RadioPlayer::selectStation);

    connect(m_volumeSlider, &QSlider::valueChanged,
            m_radio,           &RadioPlayer::changeVolume);
    connect(m_volumeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        m_radio,        &RadioPlayer::changeVolume);
    connect(m_radio, &RadioPlayer::volumeChanged,
            this,   &MainWindow::onVolumeChanged);

    connect(m_btnAdd, &QPushButton::clicked, this, &MainWindow::onAddClicked);
    connect(m_btnRemove, &QPushButton::clicked, this, &MainWindow::onRemoveClicked);
    connect(m_btnUpdate, &QPushButton::clicked, this, &MainWindow::onUpdateClicked);
    connect(m_btnReconnect, &QPushButton::clicked, this, &MainWindow::onReconnectClicked);
}


void MainWindow::onStationsChanged()
{
    QStringList names = m_radio->stationNames();
    m_listWidget->clear();
    m_listWidget->addItems(names);

    int last = m_stations->lastStationIndex();
    if (last >= 0 && last < m_listWidget->count()) {
        m_listWidget->setCurrentRow(last);
    }
}


void MainWindow::onVolumeChanged(int value)
{
    m_volumeSlider->setValue(value);
    m_volumeSpin->setValue(value);

}

void MainWindow::onAddClicked()
{
    StationDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        m_radio->addStation(dlg.station());
}

void MainWindow::onRemoveClicked()
{
    int idx = m_listWidget->currentRow();
    m_radio->removeStation(idx);
}

void MainWindow::onUpdateClicked()
{
    int idx = m_listWidget->currentRow();
    Station st = m_stations->stations().at(idx);
    StationDialog dlg(st, this);
    if (dlg.exec() == QDialog::Accepted)
        m_radio->updateStation(idx, dlg.station());
}

void MainWindow::onReconnectClicked()
{
    int idx = m_listWidget->currentRow();
    m_radio->selectStation(idx);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger:
            m_quickPopup->updateStations();

            m_quickPopup->setVolume(m_volumeSlider->value());
        {
            QPoint p = QCursor::pos();
            m_quickPopup->move(
              p.x() - m_quickPopup->width()/2,
              p.y() - m_quickPopup->height() - 10
            );
        }
            m_quickPopup->show();
            break;

        case QSystemTrayIcon::DoubleClick:
            showNormal();
            raise();
            activateWindow();
            break;

        default:
            break;
    }
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}
