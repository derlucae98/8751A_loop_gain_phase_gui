#include "networksettingsdialog.h"
#include "ui_networksettingsdialog.h"

NetworkSettingsDialog::NetworkSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NetworkSettingsDialog)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::Window);
    this->setFixedSize(this->size());
    ui->ipAddr->setInputMask("000.000.000.000");
    ui->port->setInputMask("9999");
    ui->port->clear();
    ui->gpibId->setInputMask("99");
    ui->gpibId->clear();
}

NetworkSettingsDialog::~NetworkSettingsDialog()
{
    delete ui;
}
