#include "QuickControlPopup.h"
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "stationmanager.h"

QuickControlPopup::QuickControlPopup(StationManager *stations, QWidget *parent)
  : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    // наполовину прозрачный фон
    setWindowOpacity(1);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // создаём контролы
    listWidget   = new QListWidget(this);
    btnReconnect = new QPushButton(tr("Переподкл."), this);
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSpin = new QSpinBox(this);       // создаём спинбокс


    // заполняем список
    for (auto &st : stations->stations())
        listWidget->addItem(st.name);

    auto *volLayout = new QHBoxLayout;
    volLayout->setContentsMargins(0,0,0,0);
    volLayout->addWidget(volumeSlider);
    volLayout->addWidget(volumeSpin);

    // общий вертикальный лэйаут
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(5,5,5,5);
    lay->addWidget(listWidget);
    lay->addWidget(btnReconnect);
    lay->addLayout(volLayout);



    // синхронизируем ползунок и спинбокс:
    connect(volumeSlider, &QSlider::valueChanged,
            volumeSpin,   QOverload<int>::of(&QSpinBox::setValue));
    connect(volumeSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            volumeSlider, &QSlider::setValue);
    connect(volumeSlider, &QSlider::valueChanged,
        this,         &QuickControlPopup::volumeChanged);



    // сигналы наружу
    connect(listWidget, &QListWidget::currentRowChanged,
            this,       &QuickControlPopup::stationSelected);
    connect(btnReconnect, &QPushButton::clicked,
            this,       &QuickControlPopup::reconnectRequested);


}
void QuickControlPopup::setVolume(int value)
{
    // сразу обновляем оба контрола без сигнала volumeChanged
    volumeSlider->blockSignals(true);
    volumeSpin->blockSignals(true);

    volumeSlider->setValue(value);
    volumeSpin->setValue(value);

    volumeSpin->blockSignals(false);
    volumeSlider->blockSignals(false);
}

