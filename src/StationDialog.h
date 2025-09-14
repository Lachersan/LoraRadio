#pragma once

#include <QDialog>
#include "StationManager.h"

namespace Ui {
    class StationDialog;
}

class StationDialog : public QDialog {
    Q_OBJECT

public:
    explicit StationDialog(QWidget *parent = nullptr);
    explicit StationDialog(const Station &st, QWidget *parent = nullptr);
    ~StationDialog();

    Station station() const;

private:
    Ui::StationDialog *ui;
};
