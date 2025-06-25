// StationDialog.h
#pragma once

#include <QDialog>
#include "stationmanager.h"    // где определён struct Station

namespace Ui {
    class StationDialog;
}

class StationDialog : public QDialog {
    Q_OBJECT

public:
    // Конструктор для «Создать новую»
    explicit StationDialog(QWidget *parent = nullptr);
    // Конструктор для «Правка существующей»
    explicit StationDialog(const Station &st, QWidget *parent = nullptr);
    ~StationDialog();

    // Получить данные, введённые пользователем
    Station station() const;

private:
    Ui::StationDialog *ui;
};
