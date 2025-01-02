#ifndef IMPEDANCE_H
#define IMPEDANCE_H

#include <QMainWindow>
#include <QCloseEvent>
#include <hp8751a.h>
#include <QMessageBox>
#include <QtCharts>
#include <complex.h>

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
    void init();
    void init_statemachine_sweep();
    void init_plot();
    void disable_ui();
    void enable_ui();
    void ui_start_sweep();
    void ui_stop_sweep();
    void plot_data();
    void update_parameters();

    QLogValueAxis *axisXTop = nullptr;
    QLogValueAxis *axisXBot = nullptr;
    QValueAxis *axisYTop = nullptr;
    QValueAxis *axisYBot = nullptr;
    QLineSeries *top = nullptr;
    QLineSeries *bot = nullptr;

    float topScale;
    float topRefVal;
    float botScale;
    float botRefVal;

    QList<QPointF> magnitude;
    QList<QPointF> impedance;
    QList<QPointF> phase;
    QList<QPointF> inductance;
    QList<QPointF> capacitance;
    QList<QPointF> resistance;

    std::tuple<float, float> find_min_max(QList<QPointF> &points);
    float round_one_decimal(float value);


public slots:
    void instrument_initialized();
    void set_parameters_finished();
    void new_data(HP8751A::instrument_data_t data);
    void response_timeout();


signals:
    void continueSweep(QPrivateSignal);
    void goIdle(QPrivateSignal);
private slots:
    void on_autoscale_top_stateChanged(int arg1);
    void on_autoscale_bot_stateChanged(int arg1);
    void on_topRef_valueChanged(double arg1);
    void on_topScale_valueChanged(double arg1);
    void on_botRef_valueChanged(double arg1);
    void on_botScale_valueChanged(double arg1);
    void on_view_top_currentIndexChanged(int index);
    void on_view_bot_currentIndexChanged(int index);
};

#endif // IMPEDANCE_H
