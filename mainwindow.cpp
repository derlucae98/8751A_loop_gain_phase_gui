#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->labelAveraging->setEnabled(false);
    ui->avgSweeps->setEnabled(false);

    gpib = new PrologixGPIB(this);
    QHostAddress addr = QHostAddress("192.168.178.153");
    QObject::connect(gpib, &PrologixGPIB::response, this, &MainWindow::gpib_response);
    gpib->init(addr);

    instrument_gpib_id = 17;

    lastAction = ACTION_NO_ACTION;
    request_instrument();

    pollHold = new QTimer(this);
    pollHold->setInterval(500);
    QObject::connect(pollHold, &QTimer::timeout, this, &MainWindow::pollHold_timeout);

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_avgEn_stateChanged(int arg1)
{
    ui->labelAveraging->setEnabled(arg1);
    ui->avgSweeps->setEnabled(arg1);
}


void MainWindow::on_pushButton_clicked()
{
    gpib->send_command(17, ui->lineEdit->text());
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
    send_command_list(commands);
    lastAction = ACTION_INSTRUMENT_INIT;
}

void MainWindow::instrument_init_commands(QVector<QString> &commands)
{
    //commands.push_back("PRES;*OPC?");
    commands.push_back("POWE -20;*OPC?");
    commands.push_back("STAR 10;*OPC?");
    commands.push_back("STOP 1MHZ;*OPC?");
    commands.push_back("POIN 201;*OPC?");
    commands.push_back("ATTIA20DB;*OPC?");
    commands.push_back("ATTIR20DB;*OPC?");
    commands.push_back("IFBW 20;*OPC?");
    commands.push_back("LOGFREQ;*OPC?");
    commands.push_back("DUACON;*OPC?");
    commands.push_back("SPLDON;*OPC?");
    commands.push_back("HOLD;*OPC?");
    commands.push_back("CHAN1;*OPC?");
    commands.push_back("AR;*OPC?");
    commands.push_back("FMT LOGM;*OPC?");
    commands.push_back("CHAN2;*OPC?");
    commands.push_back("AR;*OPC?");
    commands.push_back("FMT EXPP;*OPC?");
}

void MainWindow::gpib_response(QString resp)
{
    qDebug() << "Response from action " << lastAction << ": " << resp;
    // Raw GPIB responses. Commands followed by the *OPC? command are handled seperately, See send_command_list_finished()
    switch (lastAction) {
    case ACTION_NO_ACTION:
        break;
    case ACTION_INSTRUMENT_REQUEST:
        if (resp.contains("8751A")) {
            ui->instrumentState->setText("Instrument found.");
            lastAction = ACTION_NO_ACTION;
            init_instrument();
        }
        break;
    case ACTION_INSTRUMENT_INIT:
    case ACTION_SETTINGS_UPDATE:
    case ACTION_SWEEP_STARTED:
    case ACTION_POLL_SWEEP_FINISH:
    case ACTION_FIT_TRACE:
        if (resp == "1\n") {
            emit positive_response_from_instrument();
        }
        break;
    case ACTION_TRANSFER_STIMULUS:
        stimulus_raw.clear();
        stimulus_raw = resp;
        lastAction = ACTION_NO_ACTION;
        //get_trace_data();
        break;
    case ACTION_TRANSFER_DATA:
        trace_raw.clear();
        trace_raw = resp;
        lastAction = ACTION_NO_ACTION;
        //unpack_raw_data();
        break;
    }
}

void MainWindow::update_settings()
{
    instrumentSettings.clear();
    instrumentSettings.push_back(QString("STAR %1;*OPC?").arg(ui->startFreq->text()));
    instrumentSettings.push_back(QString("STOP %1;*OPC?").arg(ui->stopFreq->text()));
    instrumentSettings.push_back(QString("POIN %1;*OPC?").arg(ui->numberOfPoints->currentText()));
    instrumentSettings.push_back(QString("POWE %1;*OPC?").arg(ui->outputPower->value()));
    if (ui->attenA->currentIndex() == 0) {
        instrumentSettings.push_back("ATTIA0DB;*OPC?");
    } else {
        instrumentSettings.push_back("ATTIA20DB;*OPC?");
    }
    if (ui->attenR->currentIndex() == 0) {
        instrumentSettings.push_back("ATTIR0DB;*OPC?");
    } else {
        instrumentSettings.push_back("ATTIR20DB;*OPC?");
    }
    QString ifbw = ui->ifBw->currentText();
    ifbw.chop(3); //Remove " Hz" unit
    instrumentSettings.push_back(QString("IFBW %1;*OPC?").arg(ifbw));

    send_command_list(instrumentSettings);
}

void MainWindow::send_command_list(QVector<QString> &commands)
{
    qDebug() << "Sending command list with commands: " << commands << "for action " << lastAction;
    currentIndex = 0;

    QStateMachine *machine = new QStateMachine;
    QState *s_sendCommand = new QState();
    QState *s_response = new QState();
    QFinalState *finish = new QFinalState();


    s_response->addTransition(this, &MainWindow::positive_response_from_instrument, s_sendCommand);
    s_sendCommand->addTransition(this, &MainWindow::command_sent, s_response);
    s_sendCommand->addTransition(this, &MainWindow::instrument_init_finished, finish);

    machine->addState(s_sendCommand);
    machine->addState(s_response);
    machine->addState(finish);
    machine->setInitialState(s_sendCommand);

    QObject::connect(s_sendCommand, &QState::entered, this, [=]{
        if (currentIndex < commands.size()) {
            gpib->send_command(instrument_gpib_id, commands.at(currentIndex));
            emit command_sent();
        } else {
            emit instrument_init_finished();
        }
    });

    QObject::connect(s_response, &QState::entered, this, [&]{
        currentIndex++;
    });

    QObject::connect(finish, &QState::entered, this, [=]{
        send_command_list_finished();
        machine->deleteLater();
        s_sendCommand->deleteLater();
        s_response->deleteLater();
        finish->deleteLater();
    });

    machine->start();
}

void MainWindow::send_command_list_finished()
{
    qDebug() << "Send command list finished for action:" << lastAction;
    switch (lastAction) {
    case ACTION_NO_ACTION:
        break;
    case ACTION_INSTRUMENT_REQUEST:
        break;
    case ACTION_INSTRUMENT_INIT:
        lastAction = ACTION_NO_ACTION;
        ui->instrumentState->setText("Instrument initialized.");
        break;
    case ACTION_SETTINGS_UPDATE:
        lastAction = ACTION_NO_ACTION;
        start_sweep();
        break;
    case ACTION_SWEEP_STARTED:
        lastAction = ACTION_NO_ACTION;
        pollHold->start();
        break;
    case ACTION_POLL_SWEEP_FINISH:
        pollHold->stop();
        while(pollHold->isActive()); //Dirty wait for timer to finish
        lastAction = ACTION_NO_ACTION;
        fit_trace();
        break;
    case ACTION_FIT_TRACE:
        lastAction = ACTION_NO_ACTION;
        get_stimulus();
        break;
    case ACTION_TRANSFER_STIMULUS:
    case ACTION_TRANSFER_DATA:
        break;
    }
}

void MainWindow::start_sweep()
{
    QVector<QString> commands;

    if (not ui->avgEn->isChecked()) {
        // Single sweep when averaging is off
        commands.push_back("AVEROFF;*OPC?");
        commands.push_back("SING;*OPC?");
        send_command_list(commands);
    } else {
        // n number of sweeps for averaging factor of n
        commands.push_back("AVERON;*OPC?");
        commands.push_back(QString("AVERFACT %1;*OPC?").arg(ui->avgSweeps->currentText()));
        commands.push_back(QString("NUMG %1;*OPC?").arg(ui->avgSweeps->currentText()));
        send_command_list(commands);
    }
    lastAction = ACTION_SWEEP_STARTED;
}

void MainWindow::pollHold_timeout()
{
    QVector<QString> commands;
    commands.push_back("HOLD?");
    send_command_list(commands);
    lastAction = ACTION_POLL_SWEEP_FINISH;
}

void MainWindow::fit_trace()
{
    QVector<QString> commands;
    // Auto scale both channels. Not relevant for getting the trace data but for viewing on the instrument screen
    commands.push_back("CHAN1;AUTO;CHAN2;AUTO;CHAN1;*OPC?");
    send_command_list(commands);
    lastAction = ACTION_FIT_TRACE;
}

void MainWindow::get_stimulus()
{
    // Get stimulus frequencies
    QVector<QString> commands;
    commands.push_back("OUTPSTIM?");
    send_command_list(commands);
    lastAction = ACTION_TRANSFER_STIMULUS;
}

void MainWindow::get_trace_data()
{
    // Get trace data of Channel 1 in Format Real+jImag
    QVector<QString> commands;
    commands.push_back("OUTPFORM?");
    send_command_list(commands);
    lastAction = ACTION_TRANSFER_DATA;
}

void MainWindow::unpack_raw_data()
{
    trace_data.clear();

}


void MainWindow::on_btnStart_clicked()
{
    update_settings();
    lastAction = ACTION_SETTINGS_UPDATE;
    // Sweep will be started asynchronous after settings update was successful
}

