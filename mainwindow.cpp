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
    request_instrument();

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
    lastCommand = "*IDN?";
    gpib->send_command(instrument_gpib_id, lastCommand);
}

void MainWindow::init_instrument()
{
    currentIndex = 0;

    QVector<QString> commands;
    instrument_init_commands(commands);

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
        qDebug() << "Send command. Index:" << currentIndex;
        if (currentIndex < commands.size()) {
            gpib->send_command(instrument_gpib_id, commands.at(currentIndex));
            emit command_sent();
        } else {
            emit instrument_init_finished();
        }
    });

    QObject::connect(s_response, &QState::entered, this, [&]{
        qDebug() << "Response!";
        currentIndex++;
    });

    QObject::connect(finish, &QState::entered, this, [=]{
        qDebug() << "Init finished!";
        machine->deleteLater();
    });

    machine->start();
}

void MainWindow::instrument_init_commands(QVector<QString> &commands)
{
    commands.push_back("PRES;*OPC?");
    commands.push_back("POWE -20;*OPC?");
    commands.push_back("STAR 20;*OPC?");
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
    if (lastCommand == "*IDN?" && resp.contains("8751A")) {
        qDebug() << "Instrument:" << resp;
        lastCommand.clear();
        init_instrument();
    }
    if (resp == "1\n") {
        emit positive_response_from_instrument();
    }
}

