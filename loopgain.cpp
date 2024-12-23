#include "loopgain.h"
#include "ui_loopgain.h"

Loopgain::Loopgain(PrologixGPIB *gpib, quint16 gpibId, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Loopgain)
{
    ui->setupUi(this);
    this->gpib = gpib;
    this->gpibId = gpibId;
}

Loopgain::~Loopgain()
{
    delete ui;
}

void Loopgain::closeEvent(QCloseEvent *event)
{
    // Override close button, delete window and return to start dialog
    event->ignore();
    this->deleteLater();
}
