#include "YouTubePage.h"
#include "../include/fluent_icons.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QSlider>
#include <QSpinBox>
#include "IconButton.h"
using namespace fluent_icons;

YouTubePage::YouTubePage(AbstractPlayer* player, QWidget* parent)
    : QWidget(parent)
    , m_player(player)
{
    setupUi();
    setupConnections();

    int initVol = 50;
    if (m_player) initVol = m_player->volume();
    setVolume(initVol);
}

void YouTubePage::setupUi()
{
    // URL input + search button
    m_urlEdit   = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(tr("Введите YouTube-URL"));
    m_btnSearch = new IconButton(ic_fluent_search_28_filled, 28, QColor("#FFF"), tr("Поиск/Воспроизведение"), this);

    // Results list
    m_resultList = new QListWidget(this);

    // CRUD buttons under the list (same as RadioPage)
    m_btnAdd    = new IconButton(ic_fluent_add_circle_32_filled, 32, QColor("#FFF"), tr("Добавить"), this);
    m_btnUpdate = new IconButton(ic_fluent_edit_32_filled, 32, QColor("#FFF"), tr("Изменить"), this);
    m_btnRemove = new IconButton(ic_fluent_delete_32_filled, 32, QColor("#FFF"), tr("Удалить"), this);

    // Playback controls (same order as RadioPage)
    m_btnReconnect = new IconButton(ic_fluent_arrow_clockwise_32_filled, 32, QColor("#FFF"), tr("Переподключить"), this);
    m_btnPrev      = new IconButton(ic_fluent_previous_32_filled,    32, QColor("#FFF"), tr("Назад"), this);
    m_btnPlay      = new IconButton(ic_fluent_play_circle_48_filled, 32, QColor("#FFF"), tr("Воспроизвести"), this);
    m_btnNext      = new IconButton(ic_fluent_next_32_filled,        32, QColor("#FFF"), tr("Далее"), this);
    m_btnMute      = new IconButton(ic_fluent_speaker_2_32_filled,   32, QColor("#FFF"), tr("Заглушить"), this);

    // Volume
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSpin   = new QSpinBox(this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSpin->setRange(0, 100);

    // --- Layouts ---
    // Search row
    auto *searchLay = new QHBoxLayout;
    searchLay->addWidget(m_urlEdit, 1);
    searchLay->addWidget(m_btnSearch);

    // CRUD buttons row under list (same placement as RadioPage stationButtons)
    auto *crudLay = new QHBoxLayout;
    crudLay->addWidget(m_btnAdd);
    crudLay->addWidget(m_btnUpdate);
    crudLay->addWidget(m_btnRemove);
    crudLay->addStretch();

    // Center: search + list + crud buttons
    auto *centerLay = new QVBoxLayout;
    centerLay->addLayout(searchLay);
    centerLay->addWidget(m_resultList, 1);
    centerLay->addLayout(crudLay);

    // Control row (same order as RadioPage)
    auto *controlLay = new QHBoxLayout;
    controlLay->addWidget(m_btnReconnect);
    controlLay->addWidget(m_btnPrev);
    controlLay->addWidget(m_btnPlay);
    controlLay->addWidget(m_btnNext);
    controlLay->addStretch();
    controlLay->addWidget(m_btnMute);
    controlLay->addWidget(m_volumeSpin);
    controlLay->addWidget(m_volumeSlider);

    auto *mainLay = new QVBoxLayout(this);
    mainLay->addLayout(centerLay);
    mainLay->addLayout(controlLay);

    // If no player — disable playback controls
    if (!m_player) {
        m_btnPlay->setEnabled(false);
        m_btnPrev->setEnabled(false);
        m_btnNext->setEnabled(false);
        m_btnReconnect->setEnabled(false);
        m_btnMute->setEnabled(false);
        m_volumeSpin->setEnabled(false);
        m_volumeSlider->setEnabled(false);
    }
}

void YouTubePage::setupConnections()
{


    // Search button
    connect(m_btnSearch, &QPushButton::clicked, this, [this]() {
        const QString query = m_urlEdit->text().trimmed();
        if (!query.isEmpty()) {
            qDebug() << "[YouTubePage] Search requested:" << query;
            emit searchRequested(query);
        } else {
            qWarning() << "[YouTubePage] Empty search query";
        }
    });

    // When current item changes — emit playRequested (selection via keyboard or list navigation)
    connect(m_resultList, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem *current, QListWidgetItem *previous) {
            Q_UNUSED(previous);
            if (!current) return;
            QString url = current->data(Qt::UserRole).toString();
            if (!url.isEmpty()) {
                qDebug() << "[YouTubePage] currentItemChanged -> playRequested:" << url;
                emit playRequested(url);
            }
    });

    // Also handle explicit clicks (single click) — user asked "при выборе в списке — оно играло"
    connect(m_resultList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        QString url = it->data(Qt::UserRole).toString();
        if (!url.isEmpty()) {
            qDebug() << "[YouTubePage] itemClicked -> playRequested:" << url;
            emit playRequested(url);
        }
    });

    // Double-click should also play (common UX)
    connect(m_resultList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        QString url = it->data(Qt::UserRole).toString();
        if (!url.isEmpty()) {
            qDebug() << "[YouTubePage] itemDoubleClicked -> playRequested:" << url;
            emit playRequested(url);
        }
    });

    connect(m_btnAdd, &IconButton::clicked, this, &YouTubePage::requestAdd);

    // Remove — берем текущую строку и валидируем относительно количества элементов в списке
    connect(m_btnRemove, &IconButton::clicked, this, [this]() {
        int idx = m_resultList->currentRow();
        if (idx < 0 || idx >= m_resultList->count()) {
            qWarning() << "[YouTubePage] requestRemove: invalid idx" << idx;
            return;
        }
        emit requestRemove(idx);
    });

    // Update — аналогично Remove
    connect(m_btnUpdate, &IconButton::clicked, this, [this]() {
        int idx = m_resultList->currentRow();
        if (idx < 0 || idx >= m_resultList->count()) {
            qWarning() << "[YouTubePage] requestUpdate: invalid idx" << idx;
            return;
        }
        emit requestUpdate(idx);
    });

    // CRUD buttons
    connect(m_btnPlay, &IconButton::clicked, this, [this]() {
        qDebug() << "Кнопка Play нажата";
        if (m_player) {
            m_player->togglePlayback();
        } else {
            qWarning() << "Плеер не инициализирован";
        }
    });

    auto getSelectedIndex = [this]() -> int {
        int idx = m_resultList->currentRow();
        if (idx >= 0) return idx;
        // fallback: selectedItems()
        const auto sel = m_resultList->selectedItems();
        if (!sel.isEmpty()) return m_resultList->row(sel.first());
        return -1;
    };

    connect(m_btnRemove, &IconButton::clicked, this, [this, getSelectedIndex]() {
        int idx = getSelectedIndex();
        if (idx < 0) {
            qWarning() << "[YouTubePage] requestRemove: no selection";
            return;
        }
        qDebug() << "[YouTubePage] requestRemove(" << idx << ")";
        emit requestRemove(idx);
    });

    connect(m_btnUpdate, &IconButton::clicked, this, [this, getSelectedIndex]() {
        int idx = getSelectedIndex();
        if (idx < 0) {
            qWarning() << "[YouTubePage] requestUpdate: no selection";
            return;
        }
        qDebug() << "[YouTubePage] requestUpdate(" << idx << ")";
        emit requestUpdate(idx);
    });

    // Авто-включение/выключение кнопок Update/Remove в зависимости от selection
    auto selectionChangedHandler = [this]() {
        bool hasSelection = (m_resultList->currentRow() >= 0) || (!m_resultList->selectedItems().isEmpty());
        m_btnRemove->setEnabled(hasSelection);
        m_btnUpdate->setEnabled(hasSelection);
    };
    // initial state
    selectionChangedHandler();
    connect(m_resultList->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, selectionChangedHandler);
    connect(m_resultList, &QListWidget::currentRowChanged, this, selectionChangedHandler);

    // Playback controls
    connect(m_btnPlay, &IconButton::clicked, this, [this]() {
        if (m_player) m_player->togglePlayback();
    });

    connect(m_btnPrev, &IconButton::clicked, this, &YouTubePage::prevRequested);
    connect(m_btnNext, &IconButton::clicked, this, &YouTubePage::nextRequested);
    connect(m_btnReconnect, &IconButton::clicked, this, &YouTubePage::reconnectRequested);

    // Mute
    connect(m_btnMute, &IconButton::clicked, this, [this]() {
        if (!m_player) return;
        bool next = !m_player->isMuted();
        m_player->setMuted(next);
    });
    if (m_player) {
        connect(m_player, &AbstractPlayer::mutedChanged, this, &YouTubePage::setMuted);
    }

    // Volume: UI -> onVolumeChanged
    connect(m_volumeSlider, &QSlider::valueChanged, this, &YouTubePage::onVolumeChanged);
    connect(m_volumeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &YouTubePage::onVolumeChanged);

    // Emit volumeChanged to let external logic (player) apply it
    connect(this, &YouTubePage::volumeChanged, m_player, &AbstractPlayer::setVolume);

    // Player -> UI updates
    if (m_player) {
        connect(m_player, &AbstractPlayer::volumeChanged, this, &YouTubePage::setVolume);
        connect(m_player, &AbstractPlayer::playbackStateChanged,
                this, &YouTubePage::setPlaybackState, Qt::AutoConnection);
    }
}

void YouTubePage::setStations(const QVector<Station>& stations)
{
    m_resultList->clear();
    // Отключаем сигналы selectionModel на время заполнения списка, чтобы не срабатывать лишний раз
    auto model = m_resultList->selectionModel();
    if (model) model->blockSignals(true);

    for (const Station &s : stations) {
        QListWidgetItem *it = new QListWidgetItem(s.name.isEmpty() ? s.url : s.name);
        it->setData(Qt::UserRole, s.url); // url будет использоваться при double-click / play
        // Дополнительно можно хранить display-type/id если понадобится
        m_resultList->addItem(it);
    }

    // восстановим сигналы и обновим кнопки
    if (model) model->blockSignals(false);
    // синхронизируем состояние кнопок (удаление/изменение)
    bool hasSelection = (m_resultList->currentRow() >= 0) || (!m_resultList->selectedItems().isEmpty());
    m_btnRemove->setEnabled(hasSelection);
    m_btnUpdate->setEnabled(hasSelection);

    if (!stations.isEmpty()) m_resultList->setCurrentRow(0);
}

void YouTubePage::onVolumeChanged(int value)
{
    qDebug() << "[YouTubePage] onVolumeChanged:" << value;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    settings.setValue("volume", value);
    settings.sync();

    // Update UI without feedback loop
    setVolume(value);

    // Let connected slots (e.g. player) handle the actual change
    emit volumeChanged(value);
}

void YouTubePage::setVolume(int value)
{
    if (m_volumeSlider->value() != value) {
        bool old = m_volumeSlider->blockSignals(true);
        m_volumeSlider->setValue(value);
        m_volumeSlider->blockSignals(old);
    }
    if (m_volumeSpin->value() != value) {
        bool old = m_volumeSpin->blockSignals(true);
        m_volumeSpin->setValue(value);
        m_volumeSpin->blockSignals(old);
    }
}

void YouTubePage::setPlaybackState(bool isPlaying)
{
    m_isPlaying = isPlaying;
    if (!m_btnPlay) return;

    // If playing -> show Pause glyph, otherwise Play glyph
    m_btnPlay->setGlyph(
        isPlaying
          ? ic_fluent_pause_circle_24_filled
          : ic_fluent_play_circle_48_filled
    );
}

void YouTubePage::stopPlayback()
{
    if (m_player && m_isPlaying) {
        m_player->togglePlayback();
        setPlaybackState(false);
    }
}

void YouTubePage::setMuted(bool muted)
{
    if (!m_btnMute) return;

    m_btnMute->setGlyph(
        muted
          ? ic_fluent_speaker_off_28_filled
          : ic_fluent_speaker_2_32_filled
    );
}
