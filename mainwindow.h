#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <prologixgpib.h>
#include <QDebug>
#include <QStateMachine>
#include <QState>
#include <QFinalState>
#include <QTimer>
#include <QtCharts>
#include <math.h>

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
    void on_btnStart_clicked();


    void on_btnSave_clicked();

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
        ACTION_TRANSFER_MAGNITUDE,
        ACTION_TRANSFER_PHASE,
        ACTION_CANCEL_SWEEP
    }; //Keep track of the last GPIB action since it is asynchronous

    struct trace_data_t {
        double frequency;
        double magnitude;
        double phase;
    };

    last_action_t lastAction;

    Ui::MainWindow *ui;

    PrologixGPIB *gpib = nullptr;
    void request_instrument();
    void init_instrument();
    void instrument_init_commands(QVector<QString> &commands);
    quint8 instrument_gpib_id;
    void gpib_response(QString resp);
    void update_settings();
    void start_sweep();

    QTimer *pollHold = nullptr; //When sweeping, poll instrument to check if sweep has finished
    void pollHold_timeout();
    void fit_trace();
    void get_stimulus();
    void get_magnitude_data();
    void get_phase_data();
    void unpack_raw_data();
    void disable_ui();
    void enable_ui();
    QString stimulus_raw;
    QString magnitude_raw;
    QString phase_raw;
    QVector<trace_data_t> trace_data;

    QLineSeries *magnitude = nullptr;
    QLineSeries *phase = nullptr;
    QLogValueAxis *axisX = nullptr;
    QValueAxis *axisY = nullptr;
    QValueAxis *axisYPhase = nullptr;

    void plot_data();


signals:

};
#endif // MAINWINDOW_H
