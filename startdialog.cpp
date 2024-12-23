#include "startdialog.h"
#include "ui_startdialog.h"

StartDialog::StartDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StartDialog)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::Window);
    this->setFixedSize(this->size());

    ui->status->setText("Connecting to instrument...");
    ui->btnRetry->setEnabled(false);
    ui->btnLoopgain->setEnabled(false);
    ui->btnImpedance->setEnabled(false);

    gpib = new PrologixGPIB(this);
    addr = QHostAddress("192.168.178.153");
    QObject::connect(gpib, &PrologixGPIB::response, this, &StartDialog::gpib_response);
    QObject::connect(gpib, &PrologixGPIB::response, this, &StartDialog::gpib_response_slot);
    QObject::connect(gpib, &PrologixGPIB::stateChanged, this, &StartDialog::gpib_state);
    QObject::connect(gpib, &PrologixGPIB::disconnected, this, &StartDialog::gpib_disconected);

    instrument_gpib_id = 17;
    gpib->init(addr);

}

StartDialog::~StartDialog()
{
    delete ui;
}

void StartDialog::gpib_response_slot(QString resp)
{
    if (resp.contains("8751A")) {
        ui->status->setText("Instrument found!");
        ui->btnRetry->setEnabled(false);
        ui->btnLoopgain->setEnabled(true);
        ui->btnImpedance->setEnabled(true);
    } else {
        ui->status->setText("Instrument not found!");
    }
}

void StartDialog::gpib_state(QAbstractSocket::SocketState state)
{
    qDebug() << state;
    switch (state) {
    case QAbstractSocket::ConnectingState:
        ui->status->setText("Connecting to instrument...");
        break;
    case QAbstractSocket::UnconnectedState:
        ui->status->setText("Could not connect to instrument!");
        ui->btnRetry->setEnabled(true);
        ui->btnLoopgain->setEnabled(false);
        ui->btnImpedance->setEnabled(false);
        break;
    case QAbstractSocket::ConnectedState:
        ui->status->setText("Instrument connected!");
        request_instrument();
        break;
    default:
        break;
    }
}

void StartDialog::gpib_disconected()
{
    ui->status->setText("Connection lost!");
    ui->btnRetry->setEnabled(true);
    ui->btnLoopgain->setEnabled(false);
    ui->btnImpedance->setEnabled(false);
}

void StartDialog::request_instrument()
{
    // Is the 8751A on the bus?
    ui->status->setText("Requesting instrument...");
    gpib->send_command(instrument_gpib_id, "*IDN?");
}

void StartDialog::on_btnRetry_clicked()
{
    gpib->init(addr);
}


void StartDialog::on_btnSettings_clicked()
{
    NetworkSettingsDialog nw;
    int ret = nw.exec();
    if (ret == QDialog::Accepted) {
        // save new settings
    }
}

