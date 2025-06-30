#include "radioplayer.h"
#include "StationDialog.h"
#include "IconButton.h"

#include "AutoStartRegistry.h"
#include "fluent_icons.h"


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
#include <QApplication>
#include <QAction>
#include <QCursor>

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

    currentVolume = settings.value("audio/volume", 50).toInt();

    // 2) апликуем её в audioOutput, в основной UI и в mini-окно
    audioOutput->setVolume(currentVolume / 100.0);
    volumeSlider->setValue(currentVolume);
    volumeSpin->setValue(currentVolume);
    // quickPopup пока скрыто, но строим его заранее
    quickPopup->setVolume(currentVolume);



    // Загружаем список и сразу выбираем первую
    refreshStationList();
    int last = m_stations->lastStationIndex();   // новый метод в StationManager
    if (last >= 0 && last < listWidget->count()) {
        listWidget->setCurrentRow(last);
    } else if (listWidget->count() > 0) {
        listWidget->setCurrentRow(0);
    }else {
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


    // Список станций
    listWidget = new QListWidget;

    // Размер иконок и цвета
    const QSize icoSize(24, 24);
    QColor addCol       = QColor("#4caf50");
    QColor removeCol    = QColor("#e53935");
    QColor updateCol    = QColor("#0288d1");
    QColor reconnectCol = QColor("#ffb300");

    btnAdd = new IconButton(
    fluent_icons::ic_fluent_add_24_filled,
    24,
    QColor("#4caf50"),
    tr("Добавить станцию"),
    this);

    btnRemove = new IconButton(
        "trash",                    // эквивалент fa::fa_trash
        icoSize,
        {{"color", "#505050"}},
        tr("Удалить станцию"),
        this
    );

    btnUpdate = new IconButton(
        "edit",                     // эквивалент fa::fa_edit
        icoSize,
        {{"color", "#505050"}},
        tr("Изменить станцию"),
        this
    );

    btnReconnect = new IconButton(
        "sync-alt",                 // эквивалент fa::fa_sync_alt
        icoSize,
        {{"color", "#505050"}},
        tr("Переподключить"),
        this
    );


    // Размещаем кнопки в строке
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnRemove);
    btnLayout->addWidget(btnUpdate);
    btnLayout->addWidget(btnReconnect);

    // Левая панель со списком и кнопками
    QVBoxLayout *leftPanel = new QVBoxLayout;
    leftPanel->addWidget(listWidget);
    leftPanel->addLayout(btnLayout);

    // Главное окно: слева список+кнопки, справа громкость
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

    // создаём контекстное меню
    auto *menu = new QMenu;

    // пункт автозапуска
    auto *autostartAction = new QAction(tr("Автозапуск"), this);
    autostartAction->setCheckable(true);
    bool autostartEnabled = settings.value("autostart/enabled", false).toBool();
    autostartAction->setChecked(autostartEnabled);
    connect(autostartAction, &QAction::toggled, this, [=](bool enabled){
        settings.setValue("autostart/enabled", enabled);
        AutoStartRegistry::setEnabled(enabled);
    });

    // добавляем действия в меню
    menu->addAction(tr("Показать"), this, &QWidget::showNormal);
    menu->addAction(autostartAction);  // ← теперь переменная уже существует
    menu->addSeparator();
    menu->addAction(tr("Выход"), qApp, &QCoreApplication::quit);

    trayIcon->setContextMenu(menu);
    trayIcon->show();

    // popup
    quickPopup = new QuickControlPopup(m_stations, this);
    quickPopup->setFixedSize(200, 150);

    connect(trayIcon, &QSystemTrayIcon::activated, this,
        [=](QSystemTrayIcon::ActivationReason reason){
            switch (reason) {
                case QSystemTrayIcon::Trigger:
                    showQuickPopup();
                    break;
                case QSystemTrayIcon::DoubleClick:
                    quickPopup->hide();
                    showNormal();
                    raise(); activateWindow();
                    break;
                default:
                    break;
            }
        });
}


void RadioPlayer::showQuickPopup()
{
    if (!quickPopup) return;

    // Обновим список на всякий случай
    quickPopup->updateStations();

    // 1) сразу берём готовую currentVolume
    quickPopup->setVolume(currentVolume);

    // 2) позиционируем и показываем
    QPoint p = QCursor::pos();
    quickPopup->move(p.x() - quickPopup->width()/2,
                     p.y() - quickPopup->height() - 10);
    quickPopup->show();
    quickPopup->raise();
    quickPopup->setFocus(Qt::MouseFocusReason);
}

void RadioPlayer::setupConnections()
{
    setupVolumeConnections();
    setupStationConnections();
    setupStationManagerConnections();
    setupControlButtonsConnections();
    setupTrayConnections();
    setupQuickPopupConnections();
}

void RadioPlayer::setupVolumeConnections()
{
    connect(volumeSlider, &QSlider::valueChanged,
            this, &RadioPlayer::onVolumeChanged);
    connect(volumeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RadioPlayer::onVolumeChanged);
}

void RadioPlayer::setupStationConnections()
{
    connect(listWidget, &QListWidget::currentRowChanged,
            this,       &RadioPlayer::onStationSelected);

    connect(listWidget, &QListWidget::currentRowChanged,
            m_stations, &StationManager::setLastStationIndex);
}

void RadioPlayer::setupControlButtonsConnections()
{
    connect(btnAdd,       &QPushButton::clicked, this, &RadioPlayer::onAddStation);
    connect(btnRemove,    &QPushButton::clicked, this, &RadioPlayer::onRemoveStation);
    connect(btnUpdate,    &QPushButton::clicked, this, &RadioPlayer::onUpdateStation);
    connect(btnReconnect, &QPushButton::clicked, this, &RadioPlayer::onReconnectStation);
}

void RadioPlayer::setupStationManagerConnections()
{
    connect(m_stations, &StationManager::stationsChanged,
            this,        &RadioPlayer::refreshStationList);
}

void RadioPlayer::setupTrayConnections()
{
    connect(trayIcon, &QSystemTrayIcon::activated, this,
        [=](QSystemTrayIcon::ActivationReason reason){
            if (reason == QSystemTrayIcon::Trigger)
                showQuickPopup();
            else if (reason == QSystemTrayIcon::DoubleClick) {
                quickPopup->hide();
                showNormal(); raise(); activateWindow();
            }
        });
}

void RadioPlayer::setupQuickPopupConnections()
{
    connect(quickPopup, &QuickControlPopup::stationSelected,   this, &RadioPlayer::onStationSelected);
    connect(quickPopup, &QuickControlPopup::reconnectRequested, this, &RadioPlayer::onReconnectStation);
    connect(quickPopup, &QuickControlPopup::volumeChanged,      this, &RadioPlayer::onVolumeChanged);
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
    // сохраняем новый уровень
    currentVolume = value;
    settings.setValue("audio/volume", currentVolume);

    // апликуем в плеер
    audioOutput->setVolume(currentVolume / 100.0);

    // синхронизируем главный UI
    volumeSlider->setValue(currentVolume);
    volumeSpin->setValue(currentVolume);

    // если мини-окно открыто — обновляем и его
    if (quickPopup->isVisible())
        quickPopup->setVolume(currentVolume);


}

void RadioPlayer::closeEvent(QCloseEvent *event) {
    hide();
    event->ignore();
}
void RadioPlayer::onAutostartToggled(bool enabled)
{
    settings.setValue("autostart/enabled", enabled);
    AutoStartRegistry::setEnabled(enabled);
}

