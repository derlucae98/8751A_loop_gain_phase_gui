#ifndef LOOPGAIN_H
#define LOOPGAIN_H

#include <QMainWindow>
#include <QCloseEvent>
#include <prologixgpib.h>

namespace Ui {
class Loopgain;
}

class Loopgain : public QMainWindow
{
    Q_OBJECT

public:
    explicit Loopgain(PrologixGPIB *gpib, quint16 gpibId, QWidget *parent = nullptr);
    ~Loopgain();
    void closeEvent(QCloseEvent *event);
    PrologixGPIB *gpib = nullptr;
    quint16 gpibId;

private:
    Ui::Loopgain *ui;
};

#endif // LOOPGAIN_H
