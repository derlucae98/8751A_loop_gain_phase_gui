#include "calibratedialog.h"
#include "ui_calibratedialog.h"

CalibrateDialog::CalibrateDialog(HP8751A *hp, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CalibrateDialog)
{
    ui->setupUi(this);
    this->setFixedSize(this->size());
    ui->lStatus->setText("Updating parameters...");
    disable_gui();
    this->hp = hp;
    QObject::connect(hp, &HP8751A::cal_done, this, &CalibrateDialog::cal_done);
}

CalibrateDialog::~CalibrateDialog()
{
    delete ui;
}

void CalibrateDialog::init()
{
    enable_gui();
    hp->init_cal();
}

void CalibrateDialog::on_btnOpen_clicked()
{
    if (hp) {
        hp->measure_cal_std(HP8751A::CAL_OPEN);
        ui->lStatus->setText("Sweeping...");
        lastCalStd = HP8751A::CAL_OPEN;
        disable_gui();
    }
}

void CalibrateDialog::on_btnShort_clicked()
{
    if (hp) {
        hp->measure_cal_std(HP8751A::CAL_SHORT);
        ui->lStatus->setText("Sweeping...");
        lastCalStd = HP8751A::CAL_SHORT;
        disable_gui();
    }
}

void CalibrateDialog::on_btnLoad_clicked()
{
    if (hp) {
        hp->measure_cal_std(HP8751A::CAL_LOAD);
        ui->lStatus->setText("Sweeping...");
        lastCalStd = HP8751A::CAL_LOAD;
        disable_gui();
    }
}

void CalibrateDialog::cal_done()
{
    enable_gui();
    switch (lastCalStd) {
    case HP8751A::CAL_OPEN:
        ui->lOpen->setStyleSheet("color: green;");
        break;
    case HP8751A::CAL_SHORT:
        ui->lShort->setStyleSheet("color: green;");
        break;
    case HP8751A::CAL_LOAD:
        ui->lLoad->setStyleSheet("color: green;");
        break;
    default:
        break;
    }
}

void CalibrateDialog::disable_gui()
{
    ui->btnOpen->setEnabled(false);
    ui->btnShort->setEnabled(false);
    ui->btnLoad->setEnabled(false);
    ui->buttonBox->setEnabled(false);
}

void CalibrateDialog::enable_gui()
{
    ui->lStatus->setText("Ready.");
    ui->btnOpen->setEnabled(true);
    ui->btnShort->setEnabled(true);
    ui->btnLoad->setEnabled(true);
    ui->buttonBox->setEnabled(true);
}

