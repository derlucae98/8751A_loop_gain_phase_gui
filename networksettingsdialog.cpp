#include "networksettingsdialog.h"
#include "ui_networksettingsdialog.h"

NetworkSettingsDialog::NetworkSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NetworkSettingsDialog)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::Window);
    this->setFixedSize(this->size());
    ui->ipAddr->setInputMask("000.000.000.000;_");
    ui->port->setInputMask("9999");
    ui->port->clear();
    ui->gpibId->setInputMask("99");
    ui->gpibId->clear();
}

NetworkSettingsDialog::~NetworkSettingsDialog()
{
    delete ui;
}

void NetworkSettingsDialog::set_data(QHostAddress &ip, quint16 port, quint16 gpibId)
{
    ui->ipAddr->setText(ip.toString());
    ui->port->setText(QString::number(port));
    ui->gpibId->setText(QString::number(gpibId));
}

void NetworkSettingsDialog::get_data(QHostAddress &ip, quint16 &port, quint16 &gpibId)
{
    ip.setAddress(ui->ipAddr->text());
    port = ui->port->text().toUInt();
    gpibId = ui->gpibId->text().toUInt();
}
