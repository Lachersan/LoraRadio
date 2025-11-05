#include "QuickControlPopup.h"
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include "StationManager.h"

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

QuickControlPopup::QuickControlPopup(StationManager *stations, QWidget *parent)
  : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
  , m_stations(stations)
{
    setWindowOpacity(0.95);
    setAttribute(Qt::WA_ShowWithoutActivating);

    tabBar = new QTabBar(this);
    tabBar->addTab(tr("Radio"));
    tabBar->addTab(tr("YouTube"));
    tabBar->setExpanding(true);
    tabBar->setDrawBase(false);

    listWidget   = new QListWidget(this);
    btnReconnect = new QPushButton(tr("Переподкл."), this);
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);

    volumeSpin = new QSpinBox(this);
    volumeSpin->setRange(0, 100);
    volumeSpin->setFixedWidth(50);

    connect(volumeSlider, &QSlider::valueChanged,
            volumeSpin,   QOverload<int>::of(&QSpinBox::setValue));
    connect(volumeSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            volumeSlider, &QSlider::setValue);
    connect(volumeSlider, &QSlider::valueChanged,
            this,         &QuickControlPopup::volumeChanged);

    connect(listWidget, &QListWidget::currentRowChanged,
            this,       &QuickControlPopup::onStationSelected);
    connect(btnReconnect, &QPushButton::clicked,
            this,         &QuickControlPopup::reconnectRequested);

    connect(m_stations, &StationManager::stationsChanged,
            this,       &QuickControlPopup::updateStations);

    connect(tabBar, &QTabBar::currentChanged,
            this,   &QuickControlPopup::onTabChanged);

    updateStations();

    // Новый горизонтальный layout для кнопки reconnect и громкости (для компактности)
    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(0,0,0,0);
    bottomLayout->addWidget(btnReconnect);
    bottomLayout->addWidget(volumeSlider);
    bottomLayout->addWidget(volumeSpin);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(5,5,5,5);
    lay->addWidget(tabBar);
    lay->addWidget(listWidget);
    lay->addLayout(bottomLayout);  // Вместо отдельных btn и vol
}

void QuickControlPopup::onTabChanged(int index)
{
    m_currentTab = index;
    updateStations();
}

void QuickControlPopup::onStationSelected(int localIdx)
{
    if (localIdx < 0) return;

    QString type = (m_currentTab == 0) ? QStringLiteral("radio") : QStringLiteral("youtube");
    int global = globalIndexFromLocal(m_stations, type, localIdx);

    if (global >= 0) {
        emit stationSelected(global);
    } else {
        qWarning() << "[QuickControlPopup] Cannot map local index" << localIdx << "for type" << type;
    }
}

void QuickControlPopup::setCurrentStation(int globalIdx)
{
    if (globalIdx < 0 || globalIdx >= m_stations->stations().size()) return;

    const auto &st = m_stations->stations().at(globalIdx);
    QString type = st.type;
    int tabIdx = (type == "radio") ? 0 : (type == "youtube") ? 1 : -1;
    if (tabIdx < 0) return;

    if (tabIdx != m_currentTab) {
        tabBar->setCurrentIndex(tabIdx);
        // onTabChanged will call updateStations()
    }

    int local = localIndexFromGlobal(m_stations, globalIdx);

    if (local >= 0 && local < listWidget->count()) {
        listWidget->setCurrentRow(local);
    }
}

void QuickControlPopup::updateStations()
{
    QSignalBlocker blocker(listWidget);

    int cur = listWidget->currentRow();

    listWidget->clear();

    QString type = (m_currentTab == 0) ? QStringLiteral("radio") : QStringLiteral("youtube");
    auto stations = m_stations->stationsForType(type);

    for (auto &st : stations)
        listWidget->addItem(st.name);

    if (cur >= 0 && cur < listWidget->count())
        listWidget->setCurrentRow(cur);
}


void QuickControlPopup::setVolume(int value)
{
    volumeSlider->blockSignals(true);
    volumeSpin->  blockSignals(true);

    volumeSlider->setValue(value);
    volumeSpin->  setValue(value);

    volumeSpin->  blockSignals(false);
    volumeSlider->blockSignals(false);
}