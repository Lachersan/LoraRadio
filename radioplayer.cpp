#include "radioplayer.h"
#include "StationDialog.h"


#include <QAudioOutput>
#include <QCloseEvent>
#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QVBoxLayout>
#include <QApplication>
#include <QAction>


RadioPlayer::RadioPlayer(StationManager *stations, QWidget *parent)
    : QWidget(parent),
      player(new QMediaPlayer(this)),
      audioOutput(new QAudioOutput(this)),
      settings("MyApp", "LoraRadio"),
      m_stations(stations)
{
    // Загрузка прошлой громкости
    int savedVol = settings.value("audio/volume", 5).toInt();
    audioOutput->setVolume(savedVol / 100.0);
    player->setAudioOutput(audioOutput);
    player->play();

    setupUi();
    setupTrayIcon();
    setupConnections();

    // Загружаем список и сразу выбираем первую
    refreshStationList();
    if (listWidget->count() > 0) {
        listWidget->setCurrentRow(0);         // вызовет onStationSelected()
    } else {
        // Если список пуст – можно показать предупреждение
        qDebug() << "Station list is empty!";
    }

    hide();
}


void RadioPlayer::setupUi()
{
    // Ползунок и спинбокс громкости
    volumeSlider = new QSlider(Qt::Horizontal);
    volumeSpin   = new QSpinBox;
    volumeSlider->setRange(0,100);
    volumeSpin->setRange(0,100);
    volumeLayout = new QHBoxLayout;
    volumeLayout->addWidget(volumeSlider);
    volumeLayout->addWidget(volumeSpin);
    volumeSlider->setValue(settings.value("audio/volume",5).toInt());
    volumeSpin->setValue(settings.value("audio/volume",5).toInt());

    // Список станций и форма управления
    listWidget = new QListWidget;
    btnAdd     = new QPushButton(tr("Добавить"));
    btnRemove  = new QPushButton(tr("Удалить"));
    btnUpdate  = new QPushButton(tr("Изменить"));
    btnReconnect = new QPushButton(tr("Переподключить"));




    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnRemove);
    btnLayout->addWidget(btnUpdate);
    btnLayout->addWidget(btnReconnect);

    QVBoxLayout *leftPanel = new QVBoxLayout;
    leftPanel->addWidget(listWidget);
    leftPanel->addLayout(btnLayout);

    // Собираем всё вместе
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addLayout(leftPanel);
    mainLayout->addLayout(volumeLayout);

    setLayout(mainLayout);
    setWindowTitle(tr("Lofi Radio Player"));
    setWindowIcon(QIcon(":/icons/icon.png"));
}

void RadioPlayer::setupTrayIcon()
{
    trayIcon = new QSystemTrayIcon(QIcon(":/icons/icon.png"), this);
    QMenu *menu = new QMenu;

    QAction *showAct = menu->addAction(tr("Показать"));
    QAction *quitAct = menu->addAction(tr("Выход"));

    connect(showAct, &QAction::triggered, this, &QWidget::showNormal);
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

    trayIcon->setContextMenu(menu);
    trayIcon->setToolTip(tr("Lofi Radio Player"));
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](auto reason){
        if (reason == QSystemTrayIcon::Trigger) {
            showNormal();
            raise(); activateWindow();
        }
    });
    trayIcon->show();
}

void RadioPlayer::setupConnections()
{
    connect(volumeSlider, &QSlider::valueChanged,
            this, &RadioPlayer::onVolumeChanged);
    connect(volumeSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RadioPlayer::onVolumeChanged);
    connect(listWidget, &QListWidget::currentRowChanged,
            this, &RadioPlayer::onStationSelected);
    connect(btnAdd,    &QPushButton::clicked, this, &RadioPlayer::onAddStation);
    connect(btnRemove, &QPushButton::clicked, this, &RadioPlayer::onRemoveStation);
    connect(btnUpdate, &QPushButton::clicked, this, &RadioPlayer::onUpdateStation);
    connect(m_stations, &StationManager::stationsChanged, this, &RadioPlayer::refreshStationList);
    connect(listWidget, &QListWidget::currentRowChanged, this,       &RadioPlayer::onStationSelected);
    connect(btnReconnect, &QPushButton::clicked, this,         &RadioPlayer::onReconnectStation);
}



void RadioPlayer::refreshStationList() {
    listWidget->clear();
    for (auto &st : m_stations->stations()) {
        listWidget->addItem(st.name);
    }
}

void RadioPlayer::onStationSelected(int row) {
    if (row < 0 || row >= m_stations->stations().size()) return;
    const auto &st = m_stations->stations().at(row);
    setStreamUrl(st.url);   // сюда player->play() уже входит
}


void RadioPlayer::onAddStation() {
    StationDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        Station st = dlg.station();
        m_stations->addStation(st);
        m_stations->save();
        refreshStationList();
        // выбираем добавленную
        listWidget->setCurrentRow(m_stations->stations().size()-1);
    }
}


void RadioPlayer::onRemoveStation() {
    int row = listWidget->currentRow();
    if (row < 0) return;
    m_stations->removeStation(row);
    m_stations->save();
}

void RadioPlayer::onUpdateStation() {
    int row = listWidget->currentRow();
    if (row < 0) return;

    // Существующая станция
    Station current = m_stations->stations().at(row);

    // Открываем диалог
    StationDialog dlg(current, this);
    if (dlg.exec() == QDialog::Accepted) {
        Station edited = dlg.station();
        // Обновляем через менеджер
        m_stations->updateStation(row, edited);
        m_stations->save();
        // Обновляем UI
        listWidget->item(row)->setText(edited.name);
        setStreamUrl(edited.url);
    }
}

void RadioPlayer::onReconnectStation()
{
    int row = listWidget->currentRow();
    if (row < 0 || row >= m_stations->stations().size()) {
        return; // нечего переподключать
    }
    // Получаем текущую станцию
    const Station &st = m_stations->stations().at(row);
    // Заново запускаем плеер на том же URL
    setStreamUrl(st.url);
}



void RadioPlayer::setStreamUrl(const QString &url) {
    player->stop();
    player->setSource(QUrl(url));
    player->play();
}


void RadioPlayer::onVolumeChanged(int value) {
    // Устанавливаем громкость (0.0–1.0)
    qreal vol = value / qreal(100);
    audioOutput->setVolume(vol);

    // Сохраняем новое значение
    settings.setValue("audio/volume", value);

    // Обновляем оба виджета
    if (volumeSlider->value() != value)
        volumeSlider->setValue(value);
    if (volumeSpin->value() != value)
        volumeSpin->setValue(value);
}

void RadioPlayer::closeEvent(QCloseEvent *event) {
    hide();
    event->ignore();
}