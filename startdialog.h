#ifndef STARTDIALOG_H
#define STARTDIALOG_H

#include <QDialog>
#include "networksettingsdialog.h"
#include <prologixgpib.h>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include "loopgain.h"
#include "impedance.h"
#include "hp8751a.h"

namespace Ui {
class StartDialog;
}

class StartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StartDialog(QWidget *parent = nullptr);
    ~StartDialog();

private:
    Ui::StartDialog *ui;
    PrologixGPIB *gpib = nullptr;
    HP8751A *hp = nullptr;
    quint16 gpibId;
    QHostAddress addr;
    quint16 port;
    void gpib_response_slot(HP8751A::command_t cmd, QString resp);
    void gpib_state(QAbstractSocket::SocketState state);
    void gpib_disconected();
    void request_instrument();
    void write_default_settings();
    void write_settings();
    void read_settings();
    Loopgain *loopgain = nullptr;
    Impedance *impedance = nullptr;

signals:
    void instrument_response(HP8751A::command_t cmd, QString resp);
    void instrument_response_timeout();

private slots:
    void on_btnRetry_clicked();
    void on_btnSettings_clicked();
    void on_btnLoopgain_clicked();
    void on_btnImpedance_clicked();
};

#endif // STARTDIALOG_H
