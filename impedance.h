#ifndef IMPEDANCE_H
#define IMPEDANCE_H

#include <QMainWindow>
#include <QCloseEvent>
#include <hp8751a.h>

namespace Ui {
class Impedance;
}

class Impedance : public QMainWindow
{
    Q_OBJECT

public:
    explicit Impedance(HP8751A *hp, QWidget *parent = nullptr);
    ~Impedance();
    void closeEvent(QCloseEvent *event);
    HP8751A *hp = nullptr;

private:
    Ui::Impedance *ui;
};

#endif // IMPEDANCE_H
