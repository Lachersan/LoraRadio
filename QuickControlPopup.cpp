#include "QuickControlPopup.h"
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include "stationmanager.h"

QuickControlPopup::QuickControlPopup(StationManager *stations, QWidget *parent)
  : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    // наполовину прозрачный фон
    setWindowOpacity(0.95);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // создаём контролы
    listWidget   = new QListWidget(this);
    btnReconnect = new QPushButton(tr("Переподкл."), this);
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);

    // заполняем список
    for (auto &st : stations->stations())
        listWidget->addItem(st.name);

    // раскладка
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(5,5,5,5);
    lay->addWidget(listWidget);
    lay->addWidget(btnReconnect);
    lay->addWidget(volumeSlider);

    // сигналы наружу
    connect(listWidget, &QListWidget::currentRowChanged,
            this,       &QuickControlPopup::stationSelected);
    connect(btnReconnect, &QPushButton::clicked,
            this,       &QuickControlPopup::reconnectRequested);
    connect(volumeSlider, &QSlider::valueChanged,
            this,       &QuickControlPopup::volumeChanged);
}
