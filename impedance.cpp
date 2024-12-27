#include "impedance.h"
#include "ui_impedance.h"

Impedance::Impedance(HP8751A *hp, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Impedance)
{
    ui->setupUi(this);
    this->hp = hp;
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
