#include "MainWindow.h"
#include "RadioPage.h"
#include "YouTubePage.h"

#include "StationDialog.h"
#include "AutoStartRegistry.h"
#include "IconButton.h"
#include "../include/fluent_icons.h"
#include <QSettings>
#include <QLabel>
#include <QMenu>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QSlider>
#include <QSpinBox>
#include <QMessageBox>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QCursor>
#include <QScreen>
#include <QDebug>
using namespace fluent_icons;

namespace {
    // Найти глобальный индекс N-го элемента типа `type` (nLocal) в общем списке.
// Возвращает -1 если не найден.
    static int globalIndexFromLocal(const StationManager* mgr, const QString& type, int nLocal) {
        if (!mgr || nLocal < 0) return -1;
        int local = 0;
        const auto &all = mgr->stations();
        for (int i = 0; i < all.size(); ++i) {
            if (all.at(i).type == type) {
                if (local == nLocal) return i;
                ++local;
            }
        }
        return -1;
    }

    // Найти локальный индекс (внутри типа) для глобального индекса.
// Возвращает -1 если глобальный индекс неверный.
    static int localIndexFromGlobal(const StationManager* mgr, int globalIdx) {
        if (!mgr) return -1;
        const auto &all = mgr->stations();
        if (globalIdx < 0 || globalIdx >= all.size()) return -1;
        const QString type = all.at(globalIdx).type;
        int local = 0;
        for (int i = 0; i < globalIdx; ++i)
            if (all.at(i).type == type) ++local;
        return local;
    }
}


MainWindow::MainWindow(StationManager* stations,
                       AbstractPlayer* player,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_stations(stations)
    , m_player(player)
{
    setupUi();
    setupTray();
    setupConnections();

    QMetaObject::invokeMethod(this, [this]() {
    m_isInitializing = false;
}, Qt::QueuedConnection);

    modeTabBar->setCurrentIndex(0);
    m_lastModeIndex = modeTabBar->currentIndex();

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");

    m_lastMode = settings.value("lastMode", 0).toInt();
    modeTabBar->setCurrentIndex(m_lastMode);
    modeStack->setCurrentIndex(m_lastMode);

    // Загружаем список (в очередь, чтобы не блокировать конструктор)
    QMetaObject::invokeMethod(m_stations, "load", Qt::QueuedConnection);

    // Восстанавливаем последний индекс для текущего типа (по вкладке)
    QString type = (modeStack && modeStack->currentIndex() == 0) ? QStringLiteral("radio") : QStringLiteral("youtube");
    int lastLocal = m_stations->lastStationIndex(type);
    QMetaObject::invokeMethod(this, [this, lastLocal, type]() {
        int global = globalIndexFromLocal(m_stations, type, lastLocal);
        if (global >= 0) {
            playStation(global);
            m_currentGlobalIdx = global;  // Добавьте
        }
    }, Qt::QueuedConnection);

    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);

    this->setWindowOpacity(0.9);
    setWindowTitle("LoraRadio");
    setWindowIcon(QIcon(":/icons/image/icon.png"));
    resize(700, 480);
}


void MainWindow::setupUi()
{
    m_btnClose = new IconButton(ic_fluent_dismiss_20_filled, 20, QColor("#FFF"), tr("Закрыть"), this);
    m_btnClose->setObjectName("btnClose");
    m_btnMinimize = new IconButton(ic_fluent_line_horizontal_1_20_filled, 20, QColor("#FFF"), tr("Свернуть"), this);
    m_btnMinimize->setObjectName("btnMinimize");

    m_btnClose->setProperty("role", "system");
    m_btnMinimize->setProperty("role", "system");

    modeTabBar = new QTabBar;
    modeTabBar->addTab(tr("Radio"));
    modeTabBar->addTab(tr("YouTube"));
    modeTabBar->setExpanding(false);
    modeTabBar->setMovable(false);

    m_langMenu = new QMenu(this);
    QAction *a_en = m_langMenu->addAction(tr("English"));
    QAction *a_ru = m_langMenu->addAction(tr("Русский"));
    a_en->setData("en"); a_ru->setData("ru");
    m_langGroup = new QActionGroup(this);
    m_langGroup->setExclusive(true);
    m_langGroup->addAction(a_en);
    m_langGroup->addAction(a_ru);
    QToolButton *m_btnLang = new QToolButton(this);
    m_btnLang->setText(tr("Язык"));
    m_btnLang->setPopupMode(QToolButton::InstantPopup);
    m_btnLang->setMenu(m_langMenu);
    m_btnLang->setToolTip(tr("Выберите язык"));

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(modeTabBar);
    topLayout->addStretch();
    topLayout->addWidget(m_btnLang);
    topLayout->addWidget(m_btnMinimize);
    topLayout->addWidget(m_btnClose);
    topLayout->setSpacing(4);
    topLayout->setContentsMargins(2,0,2,0);

    QWidget *topPanel = new QWidget(this);
    topPanel->setLayout(topLayout);
    topPanel->setFixedHeight(32);

    // Страницы
    radioPage = new RadioPage(m_stations, m_player, this);
    ytPage    = new YouTubePage(m_stations, m_player, this);
    modeStack = new QStackedWidget;
    modeStack->addWidget(radioPage);
    modeStack->addWidget(ytPage);

    // Собираем всё вместе в вертикальный лэйаут
    auto *mainLay = new QVBoxLayout;
    mainLay->addWidget(topPanel);
    mainLay->addWidget(modeStack, 1);

    auto *central = new QWidget;
    central->setLayout(mainLay);
    setCentralWidget(central);

    // Сигнал переключения вкладки
    connect(modeTabBar, &QTabBar::currentChanged,
            modeStack, &QStackedWidget::setCurrentIndex);
    modeTabBar->setCurrentIndex(0);

    qDebug() << "setupUi";
}

void MainWindow::setupTray()
{
    m_trayIcon = new QSystemTrayIcon(
        QIcon(":/icons/image/icon.png"), this);
    QMenu* menu = new QMenu(this);

    menu->addAction(tr("Показать"), this, &MainWindow::showNormal);

    m_autostartAction = menu->addAction(tr("Автозапуск"));
    m_autostartAction->setCheckable(true);
    {
        QSettings initSettings(
            QSettings::IniFormat,
            QSettings::UserScope,
            "MyApp",
            "LoraRadio"
        );
        bool enabled = initSettings.value("autostart/enabled", false).toBool();
        m_autostartAction->setChecked(enabled);
    }

    // При переключении — снова локально пишем в INI и sync()
    connect(m_autostartAction, &QAction::toggled, this, [=](bool on){
        QSettings s(
            QSettings::IniFormat,
            QSettings::UserScope,
            "MyApp",
            "LoraRadio"
        );
        s.setValue("autostart/enabled", on);
        s.sync();
        AutoStartRegistry::setEnabled(on);
    });
    qDebug() << "setupTray";

    menu->addSeparator();
    menu->addAction(tr("Выход"), qApp, &QCoreApplication::quit);

    m_trayIcon->setContextMenu(menu);
    m_trayIcon->show();

    m_quickPopup = new QuickControlPopup(m_stations, this);
    m_quickPopup->setFixedSize(250, 300);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger: {
            if (m_quickPopup->isVisible()) {
                m_quickPopup->hide();
            }
            else {
                QPoint global = QCursor::pos();
                int x = global.x() - m_quickPopup->width()  / 2;
                int y = global.y() - m_quickPopup->height() - 10; // чуть выше курсора

                QRect screen = QGuiApplication::screenAt(global)->availableGeometry();
                x = qBound(screen.left(), x, screen.right() - m_quickPopup->width());
                y = qBound(screen.top(),  y, screen.bottom() - m_quickPopup->height());

                m_quickPopup->move(x, y);
                m_quickPopup->show();
                m_quickPopup->raise();
                m_quickPopup->activateWindow();
            }
            break;
        }

        case QSystemTrayIcon::DoubleClick:
            this->showNormal();
            this->raise();
            this->activateWindow();
            break;

        default:
            break;
    }
}


void MainWindow::setupConnections()
{
    connect(m_btnClose,    &IconButton::clicked, this, &MainWindow::close);
    connect(m_btnMinimize, &IconButton::clicked, this, &MainWindow::showMinimized);
    // === Language Switching ===
    connect(m_langGroup, &QActionGroup::triggered, this, &MainWindow::switchLanguage);
    // === Tab Switching ===
    connect(modeTabBar, &QTabBar::currentChanged, this, &MainWindow::onModeChanged);
    connect(modeTabBar, &QTabBar::currentChanged, modeStack, &QStackedWidget::setCurrentIndex);
    // === Station CRUD (RadioPage → MainWindow)
    connect(radioPage, &RadioPage::playStation, this, &MainWindow::onRadioPlayRequested);
    connect(radioPage, &RadioPage::requestAdd, this, &MainWindow::onAddClicked);
    connect(radioPage, &RadioPage::requestRemove, this, &MainWindow::onRemoveClicked);
    connect(radioPage, &RadioPage::requestUpdate, this, &MainWindow::onUpdateClicked);

    // 1) При изменениях в StationManager обновляем YouTubePage (показываем только youtube)
connect(m_stations, &StationManager::stationsChanged, this, [this]() {
    const QVector<Station> yt = m_stations->stationsForType(QStringLiteral("youtube"));
    if (ytPage) ytPage->setStations(yt);
});

const QVector<Station> ytInit = m_stations->stationsForType(QStringLiteral("youtube"));
if (ytPage) ytPage->setStations(ytInit);

connect(ytPage, &YouTubePage::requestAdd, this, [this]() {
    StationDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        Station st = dlg.station();
        st.type = QStringLiteral("youtube"); // важно: принудительно указываем тип
        m_stations->addStation(st);
        m_stations->save(); // сохранит и вызовет stationsChanged -> обновит ytPage
    }
});

    connect(ytPage, &YouTubePage::requestRemove, this, [this](int localIdx) {
    const QString type = QStringLiteral("youtube");
    const int global = globalIndexFromLocal(m_stations, type, localIdx);

    if (global < 0) {
        qWarning() << "[MainWindow] YouTube remove: cannot map local index" << localIdx;
        return;
    }

    // ДОБАВИТЬ: Диалог подтверждения удаления
    const Station& station = m_stations->stations().at(global);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Подтверждение удаления"));
    msgBox.setText(tr("Удалить станцию \"%1\"?").arg(station.name));
    msgBox.setInformativeText(tr("Это действие нельзя отменить."));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setIcon(QMessageBox::Warning);

    if (msgBox.exec() == QMessageBox::Yes) {
        m_stations->removeStation(global);
        m_stations->save();
    }
});

connect(ytPage, &YouTubePage::requestUpdate, this, [this](int localIdx) {
    const QString type = QStringLiteral("youtube");
    const int global = globalIndexFromLocal(m_stations, type, localIdx);
    if (global < 0) {
        qWarning() << "[MainWindow] YouTube update: cannot map local index" << localIdx;
        return;
    }
    Station orig = m_stations->stations().at(global);
    StationDialog dlg(orig, this);
    if (dlg.exec() == QDialog::Accepted) {
        Station updated = dlg.station();
        updated.type = type; // сохраняем тип как youtube
        m_stations->updateStation(global, updated);
        m_stations->save();
    }
});


    // === Кнопки управления (prev/next/reconnect)
    connect(radioPage, &RadioPage::prev, this, &MainWindow::onPrevClicked);
    connect(radioPage, &RadioPage::next, this, &MainWindow::onNextClicked);
    connect(radioPage, &RadioPage::reconnectRequested, this, &MainWindow::onReconnectClicked);
    // === Громкость и mute
    connect(m_player, &AbstractPlayer::volumeChanged, radioPage, &RadioPage::setVolume);
    connect(m_player, &AbstractPlayer::mutedChanged, radioPage, &RadioPage::setMuted);
    connect(m_player, &AbstractPlayer::volumeChanged, this, &MainWindow::onPlayerVolumeChanged);

    connect(m_player, &AbstractPlayer::volumeChanged,
            ytPage,    &YouTubePage::setVolume);
    connect(m_player, &AbstractPlayer::mutedChanged,
            ytPage,    &YouTubePage::setMuted);
    // === Playback status
    connect(m_player, &AbstractPlayer::playbackStateChanged,
            radioPage, &RadioPage::setPlaybackState);
    connect(radioPage, &RadioPage::playStation,
        this, &MainWindow::onRadioPlayRequested);
    connect(m_player, &AbstractPlayer::playbackStateChanged,
            ytPage,    &YouTubePage::setPlaybackState);
    connect(m_player, &AbstractPlayer::playbackStateChanged,
            this,      &MainWindow::onPlaybackStateChanged);
    // === Volume controls (radio → player)
    connect(radioPage, &RadioPage::volumeChanged,
            m_player,   &AbstractPlayer::setVolume);
    // === YouTube Controls


    connect(ytPage, &YouTubePage::playRequested, this, [this](const QString &url) {
    if (m_isInitializing) {
        qDebug() << "[MainWindow] Ignored YouTube playRequested during init";
        return;
    }
    if (m_player) {
        m_player->stop();
        m_player->play(url);

        // Поиск local index
        const QVector<Station> yt = m_stations->stationsForType("youtube");
        int local = -1;
        for (int i = 0; i < yt.size(); ++i) {
            if (yt.at(i).url == url) {
                local = i;
                break;
            }
        }
        if (local >= 0) {
            const Station& st = yt.at(local);

            // Исправление: обновляем индекс ПЕРЕД установкой громкости
            m_currentGlobalIdx = globalIndexFromLocal(m_stations, "youtube", local);

            m_player->setVolume(st.volume);
            qDebug() << "[MainWindow] Setting station volume for" << st.url << ":" << st.volume;

            m_stations->setLastStationIndex(local, "youtube");
            qDebug() << "[MainWindow] Updated last index for youtube to" << local;
        } else {
            qWarning() << "[MainWindow] URL not in youtube stations, reconnect may not work as expected";
        }
    }
});

    connect(ytPage, &YouTubePage::prevRequested,      this, &MainWindow::onPrevClicked);
    connect(ytPage, &YouTubePage::nextRequested,      this, &MainWindow::onNextClicked);
    connect(ytPage, &YouTubePage::reconnectRequested, this, &MainWindow::onReconnectClicked);

    connect(ytPage, &YouTubePage::volumeChanged,
            m_player, &AbstractPlayer::setVolume);
    // === Трей и быстрый доступ
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this,       &MainWindow::onTrayActivated);
    connect(m_quickPopup, &QuickControlPopup::stationSelected,
            this,       &MainWindow::playStation);
    connect(m_quickPopup, &QuickControlPopup::reconnectRequested,
            this,       &MainWindow::onReconnectClicked);
    connect(m_quickPopup, &QuickControlPopup::volumeChanged,
            m_player,    &AbstractPlayer::setVolume);
    connect(m_player, &AbstractPlayer::volumeChanged,
            m_quickPopup, &QuickControlPopup::setVolume);
}


void MainWindow::playStation(int idx)
{
    if (m_isInitializing) {
        qDebug() << "[MainWindow] playStation ignored during init";
        return;
    }

    const auto& list = m_stations->stations();
    if (idx < 0 || idx >= list.size()) {
        qWarning() << "[MainWindow] playStation: invalid index" << idx;
        return;
    }
    const Station& st = list.at(idx);
    qDebug() << "[MainWindow] Playing station:" << st.name << st.url;

    m_player->play(st.url);

    // Исправление: обновляем индекс ПЕРЕД установкой громкости
    m_currentGlobalIdx = idx;

    // Устанавливаем громкость после обновления индекса
    m_player->setVolume(st.volume);
    qDebug() << "[MainWindow] Setting station volume for" << st.url << ":" << st.volume;

    // Сохраняем *локальный* индекс в настройках для данного типа
    int localIdx = localIndexFromGlobal(m_stations, idx);
    if (localIdx >= 0) {
        m_stations->setLastStationIndex(localIdx, st.type);
    }
}

void MainWindow::onPlaybackStateChanged(bool isPlaying)
{
    if (radioPage)
        radioPage->setPlaybackState(isPlaying);

    if (ytPage)
        ytPage->setPlaybackState(isPlaying);

}

void MainWindow::onPlayClicked()
{
    m_player->togglePlayback();
}

void MainWindow::onRadioPlayRequested(int localIdx) {
    if (localIdx < 0) return;
    const QString type = QStringLiteral("radio");
    const int global = globalIndexFromLocal(m_stations, type, localIdx);
    if (global < 0) return;
    const Station& st = m_stations->stations().at(global);

    m_player->play(st.url);

    // Исправление: обновляем индекс ПЕРЕД установкой громкости
    m_currentGlobalIdx = global;

    m_player->setVolume(st.volume);
    qDebug() << "[MainWindow] Setting station volume for" << st.url << ":" << st.volume;

    m_stations->setLastStationIndex(localIdx, type);
}

void MainWindow::onPrevClicked() {
    QString type = (modeStack && modeStack->currentIndex() == 0) ? QStringLiteral("radio") : QStringLiteral("youtube");
    int local = m_stations->lastStationIndex(type);
    qDebug() << "[Prev] lastLocalIndex =" << local << "type=" << type;
    if (local > 0) {
        local--;
        int global = globalIndexFromLocal(m_stations, type, local);
        if (global >= 0) {
            m_stations->setLastStationIndex(local, type);
            const auto& st = m_stations->stations().at(global);
            m_player->play(st.url);
            m_currentGlobalIdx = global;
            m_player->setVolume(st.volume);
            qDebug() << "[MainWindow] Setting station volume for" << st.url << ":" << st.volume;
            if (type == "radio") {
                radioPage->setCurrentStation(local);
            } else {
                ytPage->setCurrentStation(local);  // Assuming similar slot added to YouTubePage
            }
        }
    } else {
        qWarning() << "[Prev] Cannot go before first station of type" << type;
    }
}

void MainWindow::onNextClicked() {
    QString type = (modeStack && modeStack->currentIndex() == 0) ? QStringLiteral("radio") : QStringLiteral("youtube");
    int local = m_stations->lastStationIndex(type);
    qDebug() << "[Next] lastLocalIndex =" << local << "type=" << type;
    int countLocal = m_stations->stationsForType(type).size();
    if (local < 0 && countLocal > 0) {
        local = 0;
    }
    if (local >= 0 && local + 1 < countLocal) {
        local++;
        int global = globalIndexFromLocal(m_stations, type, local);
        if (global >= 0) {
            m_stations->setLastStationIndex(local, type);
            const auto& st = m_stations->stations().at(global);
            m_player->play(st.url);

            // Исправление: обновляем индекс ПЕРЕД установкой громкости
            m_currentGlobalIdx = global;

            m_player->setVolume(st.volume);
            qDebug() << "[MainWindow] Setting station volume for" << st.url << ":" << st.volume;

            if (type == "radio") {
                radioPage->setCurrentStation(local);
            } else {
                ytPage->setCurrentStation(local);
            }
        }
    } else {
        qWarning() << "[Next] Cannot go past last station of type" << type;
    }
}

void MainWindow::onReconnectClicked() {
    QString type = (modeStack->currentIndex() == 0) ? "radio" : "youtube";
    int local = m_stations->lastStationIndex(type);
    qDebug() << "[MainWindow] Reconnect: type=" << type << "lastLocalIndex=" << local;
    if (local < 0) {
        qWarning() << "[MainWindow] reconnect: no last index for type" << type;
        return;
    }
    int global = globalIndexFromLocal(m_stations, type, local);
    if (global < 0 || global >= m_stations->stations().size()) {
        qWarning() << "[MainWindow] reconnect invalid global index" << global;
        return;
    }
    const auto& st = m_stations->stations().at(global);
    qDebug() << "[MainWindow] Reconnect playing URL:" << st.url;

    m_player->play(st.url);

    // Исправление: обновляем индекс ПЕРЕД установкой громкости
    m_currentGlobalIdx = global;

    m_player->setVolume(st.volume);
    qDebug() << "[MainWindow] Setting station volume for" << st.url << ":" << st.volume;
}

void MainWindow::onVolumeChanged(int value)
{
    m_volumeSlider->setValue(value);
    m_volumeSpin->setValue(value);
}

void MainWindow::onPlayerVolumeChanged(int value)
{
    if (m_isInitializing || m_currentGlobalIdx < 0) return;

    Station st = m_stations->stations().at(m_currentGlobalIdx);
    qDebug() << "[MainWindow] onPlayerVolumeChanged: idx=" << m_currentGlobalIdx << "url=" << st.url << "old_volume=" << st.volume << "new=" << value;

    if (st.volume == value) return;

    st.volume = value;
    m_stations->saveStationVolume(st);
    m_stations->updateStation(m_currentGlobalIdx, st);
}

void MainWindow::onVolumeMuteClicked()
{
    bool now = !m_player->isMuted();
    m_player->setMuted(now);
    m_volumeMute->setGlyph(
        now
        ? ic_fluent_speaker_off_28_filled
        : ic_fluent_speaker_2_32_filled
    );
}

void MainWindow::onAddClicked()
{
    StationDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_stations->addStation(dlg.station());
        m_stations->save();
    }
}

void MainWindow::onRemoveClicked(int idx)
{
    const auto& list = m_stations->stations();
    if (idx < 0 || idx >= list.size()) {
        qWarning() << "[MainWindow] onRemoveClicked: invalid index" << idx;
        return;
    }

    const Station& station = list.at(idx);

    // Диалог подтверждения удаления
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Подтверждение удаления"));
    msgBox.setText(tr("Удалить станцию \"%1\"?").arg(station.name));
    msgBox.setInformativeText(tr("Это действие нельзя отменить."));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setIcon(QMessageBox::Warning);


    if (msgBox.exec() == QMessageBox::Yes) {
        m_stations->removeStation(idx);
        m_stations->save();
    }
}

void MainWindow::onUpdateClicked(int idx)
{
    const auto& list = m_stations->stations();
    if (idx < 0 || idx >= list.size()) {
        qWarning() << "[MainWindow] onUpdateClicked: invalid index" << idx;
        return;
    }
    StationDialog dlg(list.at(idx), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_stations->updateStation(idx, dlg.station());
        m_stations->save();
    }
}

void MainWindow::onModeChanged(int newIndex)
{
    if (newIndex == m_lastModeIndex) return;

    // Останавливаем плеер на странице, с которой уходим
    if (m_lastModeIndex == 0) {
        // уходили с Radio
        if (radioPage) radioPage->stopPlayback();
    } else if (m_lastModeIndex == 1) {
        // уходили с YouTube
        if (ytPage) ytPage->stopPlayback();
    }

    // Обновляем индекс последней активной вкладки
    m_lastModeIndex = newIndex;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    settings.setValue("lastMode", newIndex);
    settings.sync();
}

void MainWindow::switchLanguage(QAction* action)
{
    const QString langCode = action->data().toString();
    QSettings settings(
        QSettings::IniFormat,
        QSettings::UserScope,
        "MyApp",
        "LoraRadio"
    );
    settings.setValue("language", langCode);
    settings.sync();

    QMessageBox::information(
        this,
        tr("Язык изменен"),
        tr("Перезапустите приложения для смены языка")
    );
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
    QMainWindow::mouseMoveEvent(event);
}


void MainWindow::closeEvent(QCloseEvent* event)
{
    hide();
    m_trayIcon->show();
    event->ignore();
}

void MainWindow::onYouTubePlayRequested(const QString &url)
{
    qWarning() << "[MainWindow] YouTube play requested:" << url;

    if (url.isEmpty()) {
        qWarning() << "[MainWindow] Invalid YouTube URL!";
        return;
    }

    // Останавливаем всё, что сейчас играет (радио или ютуб)
    m_player->stop();

    // Запускаем YouTube напрямую
    m_player->play(url);
}