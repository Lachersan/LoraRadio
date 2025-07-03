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
#include <QVBoxLayout>
#include <QSettings>
#include <QCursor>
#include <QCloseEvent>
#include <QLabel>
#include <QPainterPath>
using namespace fluent_icons;


MainWindow::MainWindow(StationManager *stations, QWidget *parent)
    : QMainWindow(parent),
      m_stations(stations),
      m_radio(new RadioPlayer(stations, this))

{

    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);


    this->setWindowOpacity(0.9);

    setupUi();
    setupTray();
    setupConnections();
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

    const QSize icoSize(32,32);

    m_btnClose = new IconButton(
    ic_fluent_share_screen_stop_28_filled,
    20,
    QColor("#FFF"),
    tr("Добавить"),
    this
    );

    m_btnMinimize = new IconButton(
    ic_fluent_window_multiple_20_filled,
    20,
    QColor("#FFF"),
    tr("Удалить"),
    this
    );

    m_btnAdd = new IconButton(
    ic_fluent_add_circle_32_filled,
    32,
    QColor("#FFF"),
    tr("Добавить"),
    this
    );

    m_btnRemove = new IconButton(
    ic_fluent_delete_32_filled,
        32,
        QColor("#FFF"),
        tr("Удалить"),
        this
    );

    m_btnUpdate = new IconButton(
    ic_fluent_edit_32_filled,
        32,
        QColor("#FFF"),
        tr("Изменить"),
        this
    );

    m_btnPrev = new IconButton(
    ic_fluent_previous_32_filled,
    32,
    QColor("#FFF"),
    tr("Добавить"),
    this
    );

    m_btnPlay = new IconButton(
    ic_fluent_play_circle_48_filled,
    32,
    QColor("#FFF"),
    tr("Добавить"),
    this
    );

    m_btnNext = new IconButton(
    ic_fluent_next_32_filled,
    32,
    QColor("#FFF"),
    tr("Добавить"),
    this
    );

    m_volumeMute = new IconButton(
    ic_fluent_speaker_2_32_filled,
        32,
        QColor("#FFF"),
        tr("Удалить"),
        this
    );



    m_btnReconnect = new IconButton(
        ic_fluent_arrow_clockwise_32_filled,
        32,
        QColor("#FFF"),
        tr("Переподключить"),
        this
    );

    auto *stationButtons = new QHBoxLayout;
    stationButtons->addWidget(m_btnAdd);
    stationButtons->addWidget(m_btnUpdate);
    stationButtons->addWidget(m_btnRemove);
    stationButtons->addStretch();
    m_btnClose->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_btnMinimize->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_btnClose->setProperty("role", "system");
    m_btnMinimize->setProperty("role", "system");


    auto *modeLayout = new QHBoxLayout;
    modeLayout->addStretch();
    modeLayout->addWidget(m_btnMinimize);
    modeLayout->addWidget(m_btnClose);
    modeLayout->setSpacing(4);




    auto *modePanel = new QWidget;
    modePanel->setLayout(modeLayout);
    modePanel->setFixedHeight(32);


    auto *stationPanel = new QVBoxLayout;
    stationPanel->addWidget(m_listWidget, 1);
    stationPanel->addLayout(stationButtons);

    auto *centerPanel = new QWidget;
    centerPanel->setLayout(stationPanel);


    auto *controlLayout = new QHBoxLayout;
    controlLayout->addWidget(m_btnReconnect);
    controlLayout->addWidget(m_btnPrev);
    controlLayout->addWidget(m_btnPlay);
    controlLayout->addWidget(m_btnNext);
    controlLayout->addStretch();
    controlLayout->addWidget(m_volumeMute);
    controlLayout->addWidget(m_volumeSpin);
    controlLayout->addWidget(m_volumeSlider);

    auto *controlPanel = new QWidget;
    controlPanel->setLayout(controlLayout);

    auto *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(modePanel);
    mainLayout->addWidget(centerPanel, 1);
    mainLayout->addWidget(controlPanel);

    auto *central = new QWidget;
    central->setLayout(mainLayout);
    setCentralWidget(central);
    mainLayout->setContentsMargins(0,0,0,0 );


    centerPanel->setStyleSheet("background-color: #000000;");


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
    connect(m_btnClose,    &QPushButton::clicked, this, &MainWindow::close);
    connect(m_btnMinimize, &QPushButton::clicked, this, &MainWindow::showMinimized);

    connect(m_radio, &RadioPlayer::stationsChanged, this,    &MainWindow::onStationsChanged);
    connect(m_listWidget, &QListWidget::currentRowChanged, m_radio, &RadioPlayer::selectStation);

    connect(m_volumeSlider, &QSlider::valueChanged, m_radio, &RadioPlayer::changeVolume);
    connect(m_volumeSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_radio, &RadioPlayer::changeVolume);
    connect(m_radio, &RadioPlayer::volumeChanged, this,   &MainWindow::onVolumeChanged);

    connect(m_btnAdd, &QPushButton::clicked, this, &MainWindow::onAddClicked);
    connect(m_btnRemove, &QPushButton::clicked, this, &MainWindow::onRemoveClicked);
    connect(m_btnUpdate, &QPushButton::clicked, this, &MainWindow::onUpdateClicked);

    connect(m_btnReconnect, &QPushButton::clicked, this, &MainWindow::onReconnectClicked);
    connect(m_btnPlay, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(m_btnNext,      &QPushButton::clicked, this, &MainWindow::onNextClicked);
    connect(m_btnPrev,      &QPushButton::clicked, this, &MainWindow::onPrevClicked);

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

void MainWindow::onPrevClicked()
{
    int index = m_listWidget->currentRow();
    if (index > 0)
    {
        m_listWidget->setCurrentRow(index - 1);
        onPlayClicked();
    }
}

void MainWindow::onPlayClicked()
{
    m_radio->togglePlayback();
}

void MainWindow::onNextClicked()
{
    int index = m_listWidget->currentRow();
    if (index < m_listWidget->count() - 1)
    {
        m_listWidget->setCurrentRow(index + 1);
        onPlayClicked();
    }
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

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        move(event->globalPos() - m_dragPosition);
}



void MainWindow::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}
