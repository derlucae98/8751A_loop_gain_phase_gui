#ifndef NETWORKSETTINGSDIALOG_H
#define NETWORKSETTINGSDIALOG_H

#include <QDialog>
#include <QHostAddress>

namespace Ui {
class NetworkSettingsDialog;
}

class NetworkSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkSettingsDialog(QWidget *parent = nullptr);
    ~NetworkSettingsDialog();
    void set_data(QHostAddress &ip, quint16 port, quint16 gpibId);
    void get_data(QHostAddress &ip, quint16 &port, quint16 &gpibId);

private:
    Ui::NetworkSettingsDialog *ui;
};

#endif // NETWORKSETTINGSDIALOG_H
