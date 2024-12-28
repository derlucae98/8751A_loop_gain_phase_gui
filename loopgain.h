#ifndef LOOPGAIN_H
#define LOOPGAIN_H

#include <QMainWindow>
#include <QCloseEvent>
#include <hp8751a.h>
#include <QtCharts>
#include <QStateMachine>
#include <QState>



namespace Ui {
class Loopgain;
}

class Loopgain : public QMainWindow
{
    Q_OBJECT

public:
    explicit Loopgain(HP8751A *hp, QWidget *parent = nullptr);
    ~Loopgain();
    void closeEvent(QCloseEvent *event);
    HP8751A *hp = nullptr;

private:
    Ui::Loopgain *ui;

    void instrument_response(HP8751A::command_t cmd, QString resp, qint8 channel);

    struct trace_data_t {
        double frequency;
        double magnitude;
        double phase;
    };

    void init();
    void init_ui();
    void update_stimulus();
    void update_receiver();
    void update_phase_format();
    void start_sweep();
    void hold_sweep();
    void fit_trace(quint8 channel = 0);
    void get_stimulus();
    void get_magnitude_data();
    void get_phase_data();
    void unpack_raw_data();
    void disable_ui();
    void enable_ui();
    void init_plot();
    void plot_data();
    void poll_hold();
    void init_sweep_statemachine();

    void ui_start_sweep();
    void ui_stop_sweep();



    QString stimulus_raw;
    QString magnitude_raw;
    QString phase_raw;
    QVector<trace_data_t> trace_data;

    QChart *chart = nullptr;
    QChartView *chartView = nullptr;
    QVBoxLayout *layout = nullptr;
    QLineSeries *magnitude = nullptr;
    QLineSeries *phase = nullptr;
    QLogValueAxis *axisX = nullptr;
    QValueAxis *axisY = nullptr;
    QValueAxis *axisYPhase = nullptr;
    float magnitudeScale;
    float magnitudeRef;
    float phaseScale;
    float phaseRef;


private slots:
    void on_chart_customContextMenuRequested(const QPoint &pos);
    void on_btnHold_clicked();
    void on_btnExport_clicked();

signals:
    void responseOK(QPrivateSignal);
    void responseNOK(QPrivateSignal);
    void continueSweep(QPrivateSignal);
    void goIdle(QPrivateSignal);

};

#endif // LOOPGAIN_H
