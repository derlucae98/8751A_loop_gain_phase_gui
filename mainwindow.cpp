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
    QString cmdString;
    cmdString.clear();
    for (int i = 0; i < commands.length(); i++) {
        cmdString.append(commands.at(i));
    }

    gpib->send_command(instrument_gpib_id, cmdString);
    lastAction = ACTION_INSTRUMENT_INIT;
}

void MainWindow::instrument_init_commands(QVector<QString> &commands)
{
    //commands.push_back("PRES;*OPC?");
    commands.push_back("POWE -20;");
    commands.push_back("STAR 10;");
    commands.push_back("STOP 1MHZ;");
    commands.push_back("POIN 201;");
    commands.push_back("ATTIA20DB;");
    commands.push_back("ATTIR20DB;");
    commands.push_back("IFBW 20;");
    commands.push_back("LOGFREQ;");
    commands.push_back("DUACON;");
    commands.push_back("SPLDON;");
    commands.push_back("HOLD;");
    commands.push_back("CHAN1;");
    commands.push_back("AR;");
    commands.push_back("FMT LOGM;");
    commands.push_back("CHAN2;");
    commands.push_back("AR;");
    commands.push_back("FMT EXPP;");
    commands.push_back("*OPC?");
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
        if (resp == "1\n") {
            lastAction = ACTION_NO_ACTION;
            ui->instrumentState->setText("Instrument initialized.");
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
            lastAction = ACTION_NO_ACTION;
            pollHold->start();
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
        }
        break;
    case ACTION_TRANSFER_STIMULUS:
        if (not resp.contains("\n")) {
            stimulus_raw.append(resp);
        } else {
            lastAction = ACTION_NO_ACTION;
            get_trace_data();
        }
        break;
    case ACTION_TRANSFER_DATA:
        if (not resp.contains("\n")) {
            trace_raw.append(resp);
        } else {
            lastAction = ACTION_NO_ACTION;
            unpack_raw_data();
        }
        break;
    }
}

void MainWindow::update_settings()
{
    QString instrumentSettings;
    instrumentSettings.clear();
    instrumentSettings.append(QString("STAR %1;").arg(ui->startFreq->text()));
    instrumentSettings.append(QString("STOP %1;").arg(ui->stopFreq->text()));
    instrumentSettings.append(QString("POIN %1;").arg(ui->numberOfPoints->currentText()));
    instrumentSettings.append(QString("POWE %1;").arg(ui->outputPower->value()));
    if (ui->attenA->currentIndex() == 0) {
        instrumentSettings.append("ATTIA0DB;");
    } else {
        instrumentSettings.append("ATTIA20DB;");
    }
    if (ui->attenR->currentIndex() == 0) {
        instrumentSettings.append("ATTIR0DB;");
    } else {
        instrumentSettings.append("ATTIR20DB;");
    }
    QString ifbw = ui->ifBw->currentText();
    ifbw.chop(3); //Remove " Hz" unit
    instrumentSettings.append(QString("IFBW %1;").arg(ifbw));
    instrumentSettings.append("*OPC?");

    gpib->send_command(instrument_gpib_id, instrumentSettings);
}

void MainWindow::start_sweep()
{
    QString commands;
    commands.clear();

    if (not ui->avgEn->isChecked()) {
        // Single sweep when averaging is off
        commands.append("AVEROFF;");
        commands.append("SING;");
        commands.append("*OPC?");
        gpib->send_command(instrument_gpib_id, commands);
    } else {
        // n number of sweeps for averaging factor of n
        commands.append("AVERON;");
        commands.append(QString("AVERFACT %1;").arg(ui->avgSweeps->currentText()));
        commands.append(QString("NUMG %1;").arg(ui->avgSweeps->currentText()));
        commands.append("*OPC?");
        gpib->send_command(instrument_gpib_id, commands);
    }
    lastAction = ACTION_SWEEP_STARTED;
}

void MainWindow::pollHold_timeout()
{
    QString commands;
    commands.clear();
    commands.append("HOLD?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_POLL_SWEEP_FINISH;
}

void MainWindow::fit_trace()
{
    QString commands;
    commands.clear();
    // Auto scale both channels. Not relevant for getting the trace data but for viewing on the instrument screen
    commands.append("CHAN1;");
    commands.append("AUTO;");
    commands.append("CHAN2");
    commands.append("AUTO;");
    commands.append("CHAN1;");
    commands.append("*OPC?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_FIT_TRACE;
}

void MainWindow::get_stimulus()
{
    // Get stimulus frequencies
    QString commands;
    commands.clear();
    commands.append("OUTPSTIM?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_TRANSFER_STIMULUS;
    stimulus_raw.clear();
}

void MainWindow::get_trace_data()
{
    // Get trace data of Channel 1 in Format Real+jImag
    QString commands;
    commands.clear();
    commands.append("OUTPFORM?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_TRANSFER_DATA;
    trace_raw.clear();
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

