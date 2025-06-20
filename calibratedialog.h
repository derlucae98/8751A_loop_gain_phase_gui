#ifndef CALIBRATEDIALOG_H
#define CALIBRATEDIALOG_H

#include <QDialog>
#include "hp8751a.h"

namespace Ui {
class CalibrateDialog;
}

class CalibrateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrateDialog(HP8751A *hp, QWidget *parent = nullptr);
    ~CalibrateDialog();
    void init();

private slots:
    void on_btnOpen_clicked();

    void on_btnShort_clicked();

    void on_btnLoad_clicked();

private:
    Ui::CalibrateDialog *ui;
    HP8751A *hp = nullptr;
    void cal_done();
    void disable_gui();
    void enable_gui();
    HP8751A::cal_std_t lastCalStd;
};



#endif // CALIBRATEDIALOG_H
