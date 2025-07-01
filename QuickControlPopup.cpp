#include "QuickControlPopup.h"
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "stationmanager.h"

QuickControlPopup::QuickControlPopup(StationManager *stations, QWidget *parent)
  : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
  , m_stations(stations)
{
    setWindowOpacity(0.95);
    setAttribute(Qt::WA_ShowWithoutActivating);

    listWidget   = new QListWidget(this);
    btnReconnect = new QPushButton(tr("Переподкл."), this);
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);

    volumeSpin = new QSpinBox(this);
    volumeSpin->setRange(0, 100);
    volumeSpin->setFixedWidth(50);

    // синхронизируем слайдер и спинбокс
    connect(volumeSlider, &QSlider::valueChanged,
            volumeSpin,   QOverload<int>::of(&QSpinBox::setValue));
    connect(volumeSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            volumeSlider, &QSlider::setValue);
    connect(volumeSlider, &QSlider::valueChanged,
            this,         &QuickControlPopup::volumeChanged);

    // список
    connect(listWidget, &QListWidget::currentRowChanged,
            this,       &QuickControlPopup::stationSelected);
    connect(btnReconnect, &QPushButton::clicked,
            this,         &QuickControlPopup::reconnectRequested);

    // подключаем сигнал менеджера → обновление списка
    connect(m_stations, &StationManager::stationsChanged,
            this,       &QuickControlPopup::updateStations);

    // один раз заполним
    updateStations();

    // раскладка
    auto *volLayout = new QHBoxLayout;
    volLayout->setContentsMargins(0,0,0,0);
    volLayout->addWidget(volumeSlider);
    volLayout->addWidget(volumeSpin);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(5,5,5,5);
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
        // блокируем все сигналы от listWidget, в том числе currentRowChanged
        QSignalBlocker blocker(listWidget);

        // запомним старый индекс
        int cur = listWidget->currentRow();

        listWidget->clear();
        for (auto &st : m_stations->stations())
                listWidget->addItem(st.name);

        // восстановим выделение без триггера сигнала
        if (cur >= 0 && cur < listWidget->count())
                listWidget->setCurrentRow(cur);
        // blocker автоматически разблокируется при выходе из функции
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
