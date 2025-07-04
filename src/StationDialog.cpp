// StationDialog.cpp
#include "StationDialog.h"
#include "ui_StationDialog.h"
#include <QPushButton>

StationDialog::StationDialog(QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::StationDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Новая станция"));

    // Кнопки OK/Cancel
    connect(ui->buttonOk,     &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->buttonCancel, &QPushButton::clicked, this, &QDialog::reject);
}

StationDialog::StationDialog(const Station &st, QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::StationDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Правка станции"));
    // Заполняем поля из переданной станции
    ui->nameEdit->setText(st.name);
    ui->urlEdit->setText(st.url);

    connect(ui->buttonOk,     &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->buttonCancel, &QPushButton::clicked, this, &QDialog::reject);
}

StationDialog::~StationDialog()
{
    delete ui;
}

Station StationDialog::station() const
{
    Station st;
    st.name = ui->nameEdit->text();
    st.url  = ui->urlEdit->text();
    return st;
}
