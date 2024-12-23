#ifndef STARTDIALOG_H
#define STARTDIALOG_H

#include <QDialog>
#include "networksettingsdialog.h"
#include <prologixgpib.h>
#include <QMessageBox>

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
    quint8 instrument_gpib_id;
    QHostAddress addr;
    void gpib_response_slot(QString resp);
    void gpib_state(QAbstractSocket::SocketState state);
    void gpib_disconected();
    void request_instrument();

signals:
    void gpib_response(QString);
private slots:
    void on_btnRetry_clicked();
    void on_btnSettings_clicked();
};

#endif // STARTDIALOG_H
