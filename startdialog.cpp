#include "startdialog.h"
#include "ui_startdialog.h"

StartDialog::StartDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StartDialog)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::Window);
    this->setFixedSize(this->size());

    if (!QFile::exists("config.ini")) {
        write_default_settings();
    }



    ui->status->setText("Connecting to instrument...");
    ui->btnRetry->setEnabled(false);
    ui->btnLoopgain->setEnabled(false);
    ui->btnImpedance->setEnabled(false);

    gpib = new PrologixGPIB(this);
    QObject::connect(gpib, &PrologixGPIB::stateChanged, this, &StartDialog::gpib_state);
    QObject::connect(gpib, &PrologixGPIB::disconnected, this, &StartDialog::gpib_disconected);

    read_settings();

    hp = new HP8751A(gpib, gpibId, this);
    QObject::connect(hp, &HP8751A::instrument_identification, this, &StartDialog::instrument_identification);
    QObject::connect(hp, &HP8751A::response_timeout, this, &StartDialog::instrument_response_timeout);
}

StartDialog::~StartDialog()
{
    hp->deleteLater();
    gpib->deleteLater();
    delete ui;
}

void StartDialog::instrument_identification(QString idn)
{
    if (idn.contains("8751A")) {
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
    hp->identify();
}

void StartDialog::write_default_settings()
{
    QSettings settings("config.ini", QSettings::IniFormat);

    QHostAddress addr = QHostAddress("192.168.178.153");
    quint16 port = 1234;
    quint16 id = 17;

    settings.beginGroup("Network");
    settings.setValue("IP-Address", addr.toString());
    settings.setValue("Port", port);
    settings.setValue("GPIB-ID", id);
    settings.endGroup();
    settings.sync();
}

void StartDialog::write_settings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Network");
    settings.setValue("IP-Address", this->addr.toString());
    settings.setValue("Port", this->port);
    settings.setValue("GPIB-ID", this->gpibId);
    settings.endGroup();
    settings.sync();
}

void StartDialog::read_settings()
{
    QSettings settings("config.ini", QSettings::IniFormat);

    QString addr;
    addr = settings.value("Network/IP-Address").toString();
    this->addr.setAddress(addr);
    this->port = settings.value("Network/Port").toUInt();
    this->gpibId = settings.value("Network/GPIB-ID").toUInt();

    gpib->init(this->addr, this->port);
}

void StartDialog::on_btnRetry_clicked()
{
    gpib->init(this->addr, this->port);
}


void StartDialog::on_btnSettings_clicked()
{
    NetworkSettingsDialog nw;
    nw.set_data(this->addr, this->port, this->gpibId);
    int ret = nw.exec();
    if (ret == QDialog::Accepted) {
        // save new settings
        nw.get_data(this->addr, this->port, this->gpibId);
        write_settings();
    }
}


void StartDialog::on_btnLoopgain_clicked()
{
    loopgain = new Loopgain(hp, this);
    QObject::connect(loopgain, &Loopgain::destroyed, this, [=] {
        this->show();
    });

    loopgain->show();
    this->hide();
}


void StartDialog::on_btnImpedance_clicked()
{
    impedance = new Impedance(hp, this);
    QObject::connect(impedance, &Impedance::destroyed, this, [=] {
        this->show();
    });

    impedance->show();
    this->hide();
}

