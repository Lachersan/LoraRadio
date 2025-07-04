#include "QuickControlPopup.h"
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include "stationmanager.h"

QuickControlPopup::QuickControlPopup(StationManager *stations, QWidget *parent)
  : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
  , m_stations(stations)
{
    setWindowOpacity(0.95);
    setAttribute(Qt::WA_ShowWithoutActivating);

    tabBar = new QTabBar(this);
    tabBar->addTab(tr("Radio"));
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
            this,       &QuickControlPopup::stationSelected);
    connect(btnReconnect, &QPushButton::clicked,
            this,         &QuickControlPopup::reconnectRequested);

    connect(m_stations, &StationManager::stationsChanged,
            this,       &QuickControlPopup::updateStations);

    updateStations();

    auto *volLayout = new QHBoxLayout;
    volLayout->setContentsMargins(0,0,0,0);
    volLayout->addWidget(volumeSlider);
    volLayout->addWidget(volumeSpin);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(5,5,5,5);
    lay->addWidget(tabBar);
    lay->addWidget(listWidget);
    lay->addWidget(btnReconnect);
    lay->addLayout(volLayout);
}

void QuickControlPopup::setCurrentStation(int index)
{
        if (index >= 0 && index < listWidget->count())
                listWidget->setCurrentRow(index);
}

void QuickControlPopup::updateStations()
{
        QSignalBlocker blocker(listWidget);

        int cur = listWidget->currentRow();

        listWidget->clear();
        for (auto &st : m_stations->stations())
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
