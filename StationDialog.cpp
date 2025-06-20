#include "StationDialog.h"
#include "ui_StationDialog.h"

StationDialog::StationDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StationDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Новая станция"));
}

StationDialog::StationDialog(const Station &st, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StationDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Правка станции"));
    ui->nameEdit->setText(st.name);
    ui->urlEdit->setText(st.url);
}

StationDialog::~StationDialog() {
    delete ui;
}

Station StationDialog::station() const {
    Station st;
    st.name = ui->nameEdit->text();
    st.url  = ui->urlEdit->text();
    return st;
}