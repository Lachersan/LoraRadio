
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QWidget>
#include <QCloseEvent>


class RadioPlayer : public QWidget {
    Q_OBJECT

public:
    RadioPlayer() {
        // Инициализация плеера и аудио вывода
        audioOutput = new QAudioOutput(this);
        player = new QMediaPlayer(this);
        player->setAudioOutput(audioOutput);

        // Установим потоковое радио (можно заменить на любое другое)
        player->setSource(QUrl("https://stream.laut.fm/chillradioluka"));
        audioOutput->setVolume(0.01);
        player->play();

        // Системный трей
        trayIcon = new QSystemTrayIcon(QIcon(":/icons/icon.png"), this);
        QMenu *trayMenu = new QMenu(this);

        QAction *showAction = new QAction("Показать", this);
        connect(showAction, &QAction::triggered, this, &QWidget::showNormal);

        QAction *quitAction = new QAction("Выход", this);
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

        trayMenu->addAction(showAction);
        trayMenu->addAction(quitAction);
        trayIcon->setContextMenu(trayMenu);
        trayIcon->setToolTip("Lofi Radio Player");

        connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                this->showNormal();
                this->raise();
                this->activateWindow();
            }
        });

        trayIcon->show();
    }

protected:
    void closeEvent(QCloseEvent *event) override {
        this->hide();
        event->ignore();
    }

private:
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    QSystemTrayIcon *trayIcon;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    RadioPlayer radio;
    return app.exec();
}
#include "main.moc"