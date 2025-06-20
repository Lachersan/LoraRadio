#pragma once

#include <QDialog>
#include "stationmanager.h"

namespace Ui {
    class StationDialog;
}

class StationDialog : public QDialog {
    Q_OBJECT

public:
    explicit StationDialog(QWidget *parent = nullptr);
    // конструктор для редактирования существующей станции
    explicit StationDialog(const Station &st, QWidget *parent = nullptr);
    ~StationDialog();

    // Получить отредактированные данные
    Station station() const;

private:
    Ui::StationDialog *ui;
};