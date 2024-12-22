#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //this->setFixedSize(this->size());



    gpib = new PrologixGPIB(this);
    QHostAddress addr = QHostAddress("192.168.178.153");
    QObject::connect(gpib, &PrologixGPIB::response, this, &MainWindow::gpib_response);
    QObject::connect(gpib, &PrologixGPIB::stateChanged, this, [=](QAbstractSocket::SocketState state) {
        qDebug() << state;
        switch (state) {
        case QAbstractSocket::ConnectingState:
            ui->statusbar->showMessage("Connecting to instrument...");
            break;
        case QAbstractSocket::UnconnectedState:
            ui->statusbar->showMessage("Could not connect to instrument!");
            break;
        case QAbstractSocket::ConnectedState:
            lastAction = ACTION_NO_ACTION;
            request_instrument();
            break;
        default:
            break;
        }
    });

    instrument_gpib_id = 17;
    gpib->init(addr);




}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::request_instrument()
{
    // Is the 8751A on the bus?
    lastAction = ACTION_INSTRUMENT_REQUEST;
    gpib->send_command(instrument_gpib_id, "*IDN?");
}

void MainWindow::init_instrument()
{
    QVector<QString> commands;
    instrument_init_commands(commands);
    QString cmdString;
    cmdString.clear();
    for (int i = 0; i < commands.length(); i++) {
        cmdString.append(commands.at(i));
    }

    gpib->send_command(instrument_gpib_id, cmdString);
    lastAction = ACTION_INSTRUMENT_INIT;
}


void MainWindow::gpib_response(QString resp)
{
    qDebug() << "Response for action " << lastAction << ": " << resp;
    switch (lastAction) {
    case ACTION_NO_ACTION:
        break;
    case ACTION_INSTRUMENT_REQUEST:
        if (resp.contains("8751A")) {
            ui->statusbar->showMessage("Instrument found. Initializing...");
            lastAction = ACTION_NO_ACTION;
            init_instrument();
        }
        break;
    case ACTION_INSTRUMENT_INIT:
        if (resp == "1\n") {
            lastAction = ACTION_NO_ACTION;
            ui->statusbar->showMessage("Instrument ready.");
            enable_ui();
            ui->btnStart->setEnabled(true);
        }
        break;
    case ACTION_SETTINGS_UPDATE:
        if (resp == "1\n") {
            lastAction = ACTION_NO_ACTION;
            start_sweep();
        }
        break;
    case ACTION_SWEEP_STARTED:
        if (resp == "1\n") {
            pollHold->start();
            lastAction = ACTION_NO_ACTION;
        }
        break;
    case ACTION_POLL_SWEEP_FINISH:
        if (resp == "1\n") {
            pollHold->stop();
            while(pollHold->isActive()); //Dirty wait for timer to finish
            lastAction = ACTION_NO_ACTION;
            fit_trace();
        }
        break;
    case ACTION_FIT_TRACE:
        if (resp == "1\n") {
            lastAction = ACTION_NO_ACTION;
            get_stimulus();
            ui->statusbar->showMessage("Retrieving data...");
        }
        break;
    case ACTION_TRANSFER_STIMULUS:
        stimulus_raw.append(resp);
        if (stimulus_raw.contains("\n")) {
            lastAction = ACTION_NO_ACTION;
            get_magnitude_data();
        }
        break;
    case ACTION_TRANSFER_MAGNITUDE:
        magnitude_raw.append(resp);
        if (magnitude_raw.contains("\n")) {
            lastAction = ACTION_NO_ACTION;
            get_phase_data();
        }
        break;
    case ACTION_TRANSFER_PHASE:
        phase_raw.append(resp);
        if (phase_raw.contains("\n")) {
            lastAction = ACTION_NO_ACTION;
            unpack_raw_data();
        }
        break;
    case ACTION_CANCEL_SWEEP:
        if (resp == "1\n") {
            enable_ui();
            ui->statusbar->showMessage("Sweep cancelled. Ready.");
        }
        break;
    }
}





