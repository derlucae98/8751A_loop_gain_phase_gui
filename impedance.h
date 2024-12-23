#ifndef IMPEDANCE_H
#define IMPEDANCE_H

#include <QMainWindow>
#include <QCloseEvent>
#include <prologixgpib.h>

namespace Ui {
class Impedance;
}

class Impedance : public QMainWindow
{
    Q_OBJECT

public:
    explicit Impedance(PrologixGPIB *gpib, quint16 gpibId, QWidget *parent = nullptr);
    ~Impedance();
    void closeEvent(QCloseEvent *event);
    PrologixGPIB *gpib = nullptr;
    quint16 gpibId;

private:
    Ui::Impedance *ui;
};

#endif // IMPEDANCE_H
