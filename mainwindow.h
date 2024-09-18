#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <prologixgpib.h>
#include <QDebug>
#include <QStateMachine>
#include <QState>
#include <QFinalState>
#include <QTimer>

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
    void on_btnStart_clicked();


private:

    enum last_action_t{
        ACTION_NO_ACTION,
        ACTION_INSTRUMENT_REQUEST,
        ACTION_INSTRUMENT_INIT,
        ACTION_SETTINGS_UPDATE,
        ACTION_SWEEP_STARTED,
        ACTION_POLL_SWEEP_FINISH,
        ACTION_FIT_TRACE,
        ACTION_TRANSFER_STIMULUS,
        ACTION_TRANSFER_DATA
    }; //Keep track of the last GPIB action since it is asynchronous

    struct trace_data_t {
        float frequency;
        float real;
        float imaginary;
    };

    last_action_t lastAction;

    Ui::MainWindow *ui;

    PrologixGPIB *gpib = nullptr;
    void request_instrument();
    void init_instrument();
    void instrument_init_commands(QVector<QString> &commands);
    quint8 instrument_gpib_id;
    void gpib_response(QString resp);
    quint16 currentIndex;
    void update_settings();
    void send_command_list(QVector<QString> &commands);
    void send_command_list_finished();
    void start_sweep();
    QVector<QString> instrumentSettings;
    QTimer *pollHold = nullptr; //When sweeping, poll instrument to check if sweep has finished
    void pollHold_timeout();
    void fit_trace();
    void get_stimulus();
    void get_trace_data();
    void unpack_raw_data();
    QString stimulus_raw;
    QString trace_raw;
    QVector<trace_data_t> trace_data;

signals:
    void positive_response_from_instrument(); // Instrument responds with "1" when queried a *OPC? command
    void instrument_init_finished();
    void command_sent();
};
#endif // MAINWINDOW_H
