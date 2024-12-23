#include "impedance.h"
#include "ui_impedance.h"

Impedance::Impedance(PrologixGPIB *gpib, quint16 gpibId, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Impedance)
{
    ui->setupUi(this);
    this->gpib = gpib;
    this->gpibId = gpibId;
}

Impedance::~Impedance()
{
    delete ui;
}

void Impedance::closeEvent(QCloseEvent *event)
{
    // Override close button, delete window and return to start dialog
    event->ignore();
    this->deleteLater();
}
