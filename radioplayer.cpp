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

    // контекстное меню (правый клик)
    auto *menu = new QMenu;
    menu->addAction(tr("Показать"), this, &QWidget::showNormal);
    menu->addAction(tr("Выход"),    qApp,  &QCoreApplication::quit);
    trayIcon->setContextMenu(menu);
    trayIcon->show();

    // popup
    quickPopup = new QuickControlPopup(m_stations, this);
    quickPopup->setFixedSize(200, 150);

    connect(trayIcon, &QSystemTrayIcon::activated, this,
      [=](QSystemTrayIcon::ActivationReason reason){
        switch (reason) {
        case QSystemTrayIcon::Trigger:      // одинарный клик
                showQuickPopup();
                break;
            case QSystemTrayIcon::DoubleClick:  // двойной клик
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
    // ========== Основное окно ==========
    // Смена громкости из слайдера и spinbox
    connect(volumeSlider, &QSlider::valueChanged,
            this,         &RadioPlayer::onVolumeChanged);
    connect(volumeSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this,         &RadioPlayer::onVolumeChanged);

    // Выбор станции в списке
    connect(listWidget, &QListWidget::currentRowChanged,
            this,       &RadioPlayer::onStationSelected);

    // Кнопки управления станциями
    connect(btnAdd,       &QPushButton::clicked,
            this,         &RadioPlayer::onAddStation);
    connect(btnRemove,    &QPushButton::clicked,
            this,         &RadioPlayer::onRemoveStation);
    connect(btnUpdate,    &QPushButton::clicked,
            this,         &RadioPlayer::onUpdateStation);
    connect(btnReconnect, &QPushButton::clicked,
            this,         &RadioPlayer::onReconnectStation);

    // Обновление списка, когда StationManager меняет данные
    connect(m_stations, &StationManager::stationsChanged,
            this,        &RadioPlayer::refreshStationList);

    // ========== Системный трей ==========
    connect(trayIcon, &QSystemTrayIcon::activated, this,
            [=](QSystemTrayIcon::ActivationReason reason){
                switch (reason) {
                case QSystemTrayIcon::Trigger:   // одиночный клик
                    showQuickPopup();
                    break;
                case QSystemTrayIcon::DoubleClick:// двойной клик
                    showNormal();
                    raise(); activateWindow();
                    break;
                default: break;
                }
            });

    // ========== Быстрый popup ==========
    connect(quickPopup, &QuickControlPopup::stationSelected,
            this,       &RadioPlayer::onStationSelected);
    connect(quickPopup, &QuickControlPopup::reconnectRequested,
            this,       &RadioPlayer::onReconnectStation);
    connect(quickPopup, &QuickControlPopup::volumeChanged,
            this,       &RadioPlayer::onVolumeChanged);




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