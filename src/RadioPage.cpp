#include "RadioPage.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QSlider>
#include <QSpinBox>

#include "../include/fluent_icons.h"
#include "IconButton.h"
using namespace fluent_icons;

RadioPage::RadioPage(StationManager* stations, AbstractPlayer* player, QWidget* parent)
    : QWidget(parent), m_stations(stations), m_player(player)
{
    setupUi();
    setupConnections();
    setVolume(m_player->volume());
    const QVector<Station> radioInit = m_stations->stationsForType(QStringLiteral("radio"));
    QStringList names;
    for (const auto& s : radioInit)
    {
        names << s.name;
    }
    setStations(names);
}

void RadioPage::setupUi()
{
    m_listWidget = new QListWidget;
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSpin = new QSpinBox;
    m_volumeSlider->setRange(0, 100);
    m_volumeSpin->setRange(0, 100);

    m_btnAdd =
        new IconButton(ic_fluent_add_circle_32_filled, 32, QColor("#FFF"), tr("Добавить"), this);
    m_btnRemove =
        new IconButton(ic_fluent_delete_32_filled, 32, QColor("#FFF"), tr("Удалить"), this);
    m_btnUpdate =
        new IconButton(ic_fluent_edit_32_filled, 32, QColor("#FFF"), tr("Изменить"), this);
    m_btnPrev = new IconButton(ic_fluent_previous_32_filled, 32, QColor("#FFF"), tr("Назад"), this);
    m_btnPlay = new IconButton(ic_fluent_play_circle_48_filled, 32, QColor("#FFF"),
                               tr("Воспроизвести"), this);
    m_btnNext = new IconButton(ic_fluent_next_32_filled, 32, QColor("#FFF"), tr("Далее"), this);
    m_btnMute =
        new IconButton(ic_fluent_speaker_2_32_filled, 32, QColor("#FFF"), tr("Заглушить"), this);
    m_btnReconnect = new IconButton(ic_fluent_arrow_clockwise_32_filled, 32, QColor("#FFF"),
                                    tr("Переподключить"), this);

    auto* stationButtons = new QHBoxLayout;
    stationButtons->addWidget(m_btnAdd);
    stationButtons->addWidget(m_btnUpdate);
    stationButtons->addWidget(m_btnRemove);
    stationButtons->addStretch();

    auto* stationPanel = new QWidget(this);
    stationPanel->setObjectName("stationPanel");

    auto* stationLay = new QVBoxLayout(stationPanel);  // Layout теперь в panel
    stationLay->addWidget(m_listWidget, 1);
    stationLay->addLayout(stationButtons);
    stationLay->setContentsMargins(0, 0, 0, 0);

    auto* controlLay = new QHBoxLayout;
    controlLay->addWidget(m_btnReconnect);
    controlLay->addWidget(m_btnPrev);
    controlLay->addWidget(m_btnPlay);
    controlLay->addWidget(m_btnNext);
    controlLay->addStretch();
    controlLay->addWidget(m_btnMute);
    controlLay->addWidget(m_volumeSpin);
    controlLay->addWidget(m_volumeSlider);

    auto* mainLay = new QVBoxLayout(this);
    mainLay->addWidget(
        stationPanel,
        1);  // Добавьте panel вместо stationLay (с stretch=1 для занятия пространства)
    mainLay->addLayout(controlLay);
    mainLay->setContentsMargins(0, 0, 0, 0);
}

void RadioPage::setupConnections()
{
    // === Обновление списка станций из StationManager ===
    connect(m_stations, &StationManager::stationsChanged, this,
            [this]()
            {
                QStringList names;
                for (const auto& s : m_stations->stations())
                {
                    if (s.type == QStringLiteral("radio"))
                    {  // Фільтр по типу
                        names << s.name;
                    }
                }
                setStations(names);
            });

    // === CRUD‑сигналы страницы вверх в MainWindow ===
    connect(m_btnAdd, &IconButton::clicked, this, &RadioPage::requestAdd);
    connect(m_btnRemove, &IconButton::clicked, this,
            [this]()
            {
                int idx = m_listWidget->currentRow();
                if (idx < 0 || idx >= m_stations->stations().size()) return;
                emit requestRemove(idx);
            });
    connect(m_btnUpdate, &IconButton::clicked, this,
            [this]()
            {
                int idx = m_listWidget->currentRow();
                if (idx < 0 || idx >= m_stations->stations().size()) return;
                emit requestUpdate(idx);
            });

    // === Выбор станции из списка ===
    connect(m_listWidget, &QListWidget::currentRowChanged, this, &RadioPage::playStation);

    // === Навигация и плейбек ===
    connect(m_btnPrev, &IconButton::clicked, this, &RadioPage::prev);
    connect(m_btnNext, &IconButton::clicked, this, &RadioPage::next);
    connect(m_btnPlay, &IconButton::clicked, this, [this]() { m_player->togglePlayback(); });
    connect(m_btnReconnect, &IconButton::clicked, this, &RadioPage::reconnect);

    // === Мьют ===
    connect(m_btnMute, &IconButton::clicked, this,
            [this]()
            {
                bool next = !m_player->isMuted();
                m_player->setMuted(next);
            });
    connect(m_player, &AbstractPlayer::mutedChanged, this, &RadioPage::setMuted);

    // === Громкость ===
    connect(m_volumeSlider, &QSlider::valueChanged, this, &RadioPage::onVolumeChanged);
    connect(m_volumeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &RadioPage::onVolumeChanged);
    connect(this, &RadioPage::volumeChanged, m_player, &AbstractPlayer::setVolume);
    connect(m_player, &AbstractPlayer::volumeChanged, this, &RadioPage::setVolume);

    connect(m_player, &AbstractPlayer::playbackStateChanged, this, &RadioPage::setPlaybackState,
            Qt::AutoConnection);
}

void RadioPage::setStations(const QStringList& names)
{
    m_listWidget->blockSignals(true);  // Блокируем сигналы
    m_listWidget->clear();
    m_listWidget->addItems(names);

    // Восстанавливаем выбор текущей станции
    if (m_currentStationIndex >= 0 && m_currentStationIndex < names.size())
    {
        m_listWidget->setCurrentRow(m_currentStationIndex);
    }
    m_listWidget->blockSignals(false);  // Разблокируем
}

void RadioPage::setCurrentStation(int index)
{
    if (index >= 0 && index < m_listWidget->count())
    {
        m_listWidget->setCurrentRow(index);
        m_currentStationIndex = index;
    }
}

void RadioPage::setPlaybackState(bool isPlaying)
{
    m_isPlaying = isPlaying;

    if (m_btnPlay.isNull())
    {
        return;
    }
    m_btnPlay->setGlyph(isPlaying ? ic_fluent_pause_circle_24_filled
                                  : ic_fluent_play_circle_48_filled);
}

void RadioPage::stopPlayback()
{
    if (m_player && m_isPlaying)
    {
        m_player->togglePlayback();
        setPlaybackState(false);
    }
}

void RadioPage::onVolumeChanged(int value)
{
    qDebug() << "[RadioPage] onVolumeChanged called with value:" << value;

    emit volumeChanged(value);
}

void RadioPage::setVolume(int value)
{
    qDebug() << "[RadioPage] Setting volume to:" << value;

    m_volumeSlider->blockSignals(true);
    m_volumeSpin->blockSignals(true);

    m_volumeSlider->setValue(value);
    m_volumeSpin->setValue(value);

    m_volumeSlider->blockSignals(false);
    m_volumeSpin->blockSignals(false);
}

void RadioPage::setMuted(bool muted)
{
    m_btnMute->setGlyph(muted ? ic_fluent_speaker_off_28_filled : ic_fluent_speaker_2_32_filled);
}