#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <prologixgpib.h>
#include <QDebug>
#include <QStateMachine>
#include <QState>
#include <QFinalState>
#include <QEventTransition>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_avgEn_stateChanged(int arg1);

    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;

    PrologixGPIB *gpib = nullptr;
    void request_instrument();
    void init_instrument();
    void instrument_init_commands(QVector<QString> &commands);
    quint8 instrument_gpib_id;
    void gpib_response(QString resp);
    quint16 currentIndex;
    QString lastCommand;

signals:
    void positive_response_from_instrument(); // Instrument responds with "1" when queried a *OPC? command
    void instrument_init_finished();
    void command_sent();
};
#endif // MAINWINDOW_H
