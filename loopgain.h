#ifndef LOOPGAIN_H
#define LOOPGAIN_H

#include <QMainWindow>
#include <QCloseEvent>
#include <hp8751a.h>
#include <QtCharts>
#include <QStateMachine>
#include <QState>
#include <QMessageBox>


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

    struct trace_data_t {
        float frequency;
        float magnitude;
        float phase;
    };

    void init();
    void init_statemachine_sweep();
    void init_ui();

    void start_sweep();

    void disable_ui();
    void enable_ui();
    void init_plot();
    void plot_data();

    void update_parameters();


    void ui_start_sweep();
    void ui_stop_sweep();


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

    bool sweepRequested;

public slots:
    void instrument_initialized();
    void set_parameters_finished();
    void new_data(HP8751A::instrument_data_t data);
    void response_timeout();


private slots:
    void on_chart_customContextMenuRequested(const QPoint &pos);

    void on_btnExport_clicked();

    void on_aAutoscale_stateChanged(int arg1);

    void on_phiAutoscale_stateChanged(int arg1);

    void on_aref_valueChanged(double arg1);

    void on_ascale_valueChanged(double arg1);

    void on_phiref_valueChanged(double arg1);

    void on_phiscale_valueChanged(double arg1);


signals:
    void responseOK(QPrivateSignal);
    void responseNOK(QPrivateSignal);
    void continueSweep(QPrivateSignal);
    void goIdle(QPrivateSignal);

};

#endif // LOOPGAIN_H
