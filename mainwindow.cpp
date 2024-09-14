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
    gpib->init(addr);
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

