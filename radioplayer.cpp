
#include "radioplayer.h"
#include <QWidget>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QDebug>

RadioPlayer::RadioPlayer(QWidget *parent)
    : QWidget(parent),
      player(new QMediaPlayer(this)),
      audioOutput(new QAudioOutput(this)),
      volumeSlider(new QSlider(Qt::Horizontal, this)),
      volumeSpin(new QSpinBox(this)),
      controlLayout(new QHBoxLayout),
      settings("MyApp", "LoraRadio")  // Инициализация QSettings
{
    // Загрузка сохранённого значения громкости (0–100)
    int savedVol = settings.value("audio/volume", 5).toInt();

    // Настройка плеера
    player->setAudioOutput(audioOutput);
    player->setSource(QUrl("https://stream.laut.fm/chillradioluka"));
    audioOutput->setVolume(savedVol / 100.0);  // стартовая громкость

    // Настройка ползунка и spinbox
    volumeSlider->setRange(0, 100);
    volumeSpin->setRange(0, 100);

    // Синхронизируем ползунок и поле
    connect(volumeSlider, &QSlider::valueChanged, this, &RadioPlayer::onVolumeChanged);
    connect(volumeSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RadioPlayer::onVolumeChanged);

    // Компоновка панели управления
    controlLayout->addWidget(volumeSlider);
    controlLayout->addWidget(volumeSpin);
    setLayout(controlLayout);

    // Установка начальных значений виджетов
    volumeSlider->setValue(savedVol);
    volumeSpin->setValue(savedVol);

    // Запуск плеера
    player->play();

    // --- системный трей ---
    trayIcon = new QSystemTrayIcon(QIcon(":/icons/icon.png"), this);
    QMenu *trayMenu = new QMenu(this);
    setWindowIcon(QIcon(":/icons/icon.png"));

    QAction *showAction = new QAction(tr("Показать"), this);
    connect(showAction, &QAction::triggered, this, &QWidget::showNormal);
    QAction *quitAction = new QAction(tr("Выход"), this);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    trayMenu->addAction(showAction);
    trayMenu->addAction(quitAction);
    trayIcon->setContextMenu(trayMenu);
    trayIcon->setToolTip(tr("Lofi Radio Player"));
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](auto reason){
        if (reason == QSystemTrayIcon::Trigger) {
            showNormal();
            raise();
            activateWindow();
        }
    });
    trayIcon->show();
    hide();
}

void RadioPlayer::onVolumeChanged(int value) {
    // Устанавливаем громкость (0.0–1.0)
    qreal vol = value / qreal(100);
    audioOutput->setVolume(vol);

    // Сохраняем новое значение
    settings.setValue("audio/volume", value);

    // Обновляем оба виджета
    if (volumeSlider->value() != value)
        volumeSlider->setValue(value);
    if (volumeSpin->value() != value)
        volumeSpin->setValue(value);
}

void RadioPlayer::closeEvent(QCloseEvent *event) {
    hide();
    event->ignore();
}
