#include "impedance.h"
#include "ui_impedance.h"

Impedance::Impedance(HP8751A *hp, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Impedance)
{
    ui->setupUi(this);
    this->hp = hp;
    if (!this->hp) {
        this->deleteLater();
        return;
    }

    QObject::connect(hp, &HP8751A::new_data, this, &Impedance::new_data);
    QObject::connect(hp, &HP8751A::response_timeout, this, &Impedance::response_timeout);
    QObject::connect(hp, &HP8751A::instrument_initialized, this, &Impedance::instrument_initialized);
    QObject::connect(hp, &HP8751A::set_parameters_finished, this, &Impedance::set_parameters_finished);

    init();
}

Impedance::~Impedance()
{
    delete ui;
}

void Impedance::closeEvent(QCloseEvent *event)
{
    // Override close button, delete window and return to start dialog
    event->ignore();
    this->deleteLater();
}

void Impedance::init()
{
    disable_ui();
    ui->statusbar->showMessage("Initializing instrument...");
    hp->init_function(HP8751A::PORT_AR, HP8751A::CONV_Y_REFL, HP8751A::PORT_AR, HP8751A::CONV_OFF);
    init_plot();
}

void Impedance::init_statemachine_sweep()
{
    /*  Behavior of instrument controls:
     *  Hold button is disabled by default. It is only enabled when the instrument is currently sweeping (HOLD? returns 0)
     *  When continous sweep is selected, the single button is disabled. Hold button is enabled. Cont. button is checked.
     *      When cont. button is clicked again, it will finish the current sweep and will hold afterwards.
     *      Clicking the hold button during a sweep cancels the current sweep.
     *  When single sweep is selected, cont. button is disabled while the instrument is sweeping. Hold button is enabled.
     *  Enable export button only when data has been received and instrument is in hold mode.
     *
     */

    // This state machine handles the UI behavior and controls the instrument via methods of the HP8751A object
    QStateMachine *smSweep = new QStateMachine(this);
    QState *sIdle = new QState();
    QState *sUpdateParameters = new QState();
    QState *sStartSweep = new QState();
    QState *sWaitForData = new QState();
    QState *sPlotData = new QState();
    QState *sHold = new QState();

    QObject::connect(sIdle, &QState::entered, this, &Impedance::ui_stop_sweep);
    sIdle->addTransition(ui->btnSingle, &QPushButton::clicked, sUpdateParameters);
    sIdle->addTransition(ui->btnContinuous, &QPushButton::clicked, sUpdateParameters);

    QObject::connect(sUpdateParameters, &QState::entered, this, &Impedance::ui_start_sweep);
    QObject::connect(sUpdateParameters, &QState::entered, this, &Impedance::update_parameters);
    sUpdateParameters->addTransition(hp, &HP8751A::set_parameters_finished, sStartSweep);
    sUpdateParameters->addTransition(ui->btnHold, &QPushButton::clicked, sHold);
    sUpdateParameters->addTransition(hp, &HP8751A::response_timeout, sIdle);

    QObject::connect(sStartSweep, &QState::entered, hp, &HP8751A::request_sweep);
    QObject::connect(sStartSweep, &QState::entered, this, [=] {
        ui->statusbar->showMessage("Sweeping...");
    });
    sStartSweep->addTransition(hp, &HP8751A::retrieving_data, sWaitForData);
    sStartSweep->addTransition(ui->btnHold, &QPushButton::clicked, sHold);
    sStartSweep->addTransition(hp, &HP8751A::response_timeout, sIdle);

    QObject::connect(sWaitForData, &QState::entered, this, [=] {
        ui->statusbar->showMessage("Retrieving data...");
    });
    sWaitForData->addTransition(hp, &HP8751A::new_data, sPlotData);
    sWaitForData->addTransition(ui->btnHold, &QPushButton::clicked, sHold);
    sWaitForData->addTransition(hp, &HP8751A::response_timeout, sIdle);

    QObject::connect(sPlotData, &QState::entered, this, &Impedance::plot_data);
    QObject::connect(sPlotData, &QState::entered, this, [=] {
        if (ui->btnContinuous->isChecked()) {
            emit continueSweep(QPrivateSignal());
        } else {
            emit goIdle(QPrivateSignal());
        }
    });
    sPlotData->addTransition(this, &Impedance::goIdle, sIdle);
    sPlotData->addTransition(this, &Impedance::continueSweep, sStartSweep);

    QObject::connect(sHold, &QState::entered, this, [=] {
        ui->statusbar->showMessage("Cancelling sweep...");
    });
    QObject::connect(sHold, &QState::entered, hp, &HP8751A::request_cancel);
    sHold->addTransition(hp, &HP8751A::sweep_cancelled, sIdle);

    smSweep->addState(sIdle);
    smSweep->addState(sUpdateParameters);
    smSweep->addState(sStartSweep);
    smSweep->addState(sWaitForData);
    smSweep->addState(sPlotData);
    smSweep->addState(sHold);
    smSweep->setInitialState(sIdle);
    smSweep->start();
}

void Impedance::init_plot()
{
    axisX = new QLogValueAxis();
    axisX->setTitleText("Frequency / Hz");
    axisX->setLabelFormat("%g");
    axisX->setBase(10.0);
    axisX->setMinorTickCount(8);

    QChart *chartTop = nullptr;
    QChartView *chartViewTop = nullptr;
    QVBoxLayout *layoutTop = nullptr;
    axisYTop = new QValueAxis();
    top = new QLineSeries();
    chartTop = new QChart();

    chartTop->addSeries(top);
    chartTop->addAxis(axisX, Qt::AlignBottom);
    top->attachAxis(axisX);
    top->attachAxis(axisYTop);
    top->setName(ui->view_top->currentText());

    axisYTop->setTitleText("top / dB");
    axisYTop->setLabelFormat("%i");
    chartTop->addAxis(axisYTop, Qt::AlignLeft);

    chartViewTop = new QChartView(chartTop);
    chartViewTop->setRenderHint(QPainter::Antialiasing);
    layoutTop = new QVBoxLayout(ui->chart_top);
    layoutTop->addWidget(chartViewTop);

    QChart *chartBot = nullptr;
    QChartView *chartViewBot = nullptr;
    QVBoxLayout *layoutBot = nullptr;
    axisYBot = new QValueAxis();
    bot = new QLineSeries();
    chartBot = new QChart();

    chartBot->addSeries(bot);
    chartBot->addAxis(axisX, Qt::AlignBottom);
    bot->attachAxis(axisX);
    bot->attachAxis(axisYBot);
    bot->setName(ui->view_bot->currentText());

    axisYBot->setTitleText("bot / °");
    axisYBot->setLabelFormat("%i");
    chartBot->addAxis(axisYBot, Qt::AlignLeft);

    chartViewBot = new QChartView(chartBot);
    chartViewBot->setRenderHint(QPainter::Antialiasing);
    layoutBot = new QVBoxLayout(ui->chart_bot);
    layoutBot->addWidget(chartViewBot);
}

void Impedance::disable_ui()
{
    ui->btnHold->setEnabled(false);
    ui->btnExport->setEnabled(false);
    ui->topScale->setEnabled(false);
    ui->topRef->setEnabled(false);
    ui->botScale->setEnabled(false);
    ui->botRef->setEnabled(false);
    ui->centralwidget->setEnabled(false);
}

void Impedance::enable_ui()
{
    ui->centralwidget->setEnabled(true);
}

void Impedance::ui_start_sweep()
{
    ui->statusbar->showMessage("Updating parameters...");
    if (!ui->btnContinuous->isChecked()) {
        // Single sweep mode selected
        ui->btnContinuous->setEnabled(false);
    }
    ui->btnSingle->setEnabled(false);
    ui->btnHold->setEnabled(true);
    ui->btnExport->setEnabled(false);
    ui->autoscale_top->setEnabled(false);
    ui->autoscale_bot->setEnabled(false);

    QList<QWidget*> grpSource = ui->grpSource->findChildren<QWidget*>();
    foreach (QWidget* w, grpSource) {
        w->setEnabled(false);
    }

    QList<QWidget*> grpReceive = ui->grpReceive->findChildren<QWidget*>();
    foreach (QWidget* w, grpReceive) {
        w->setEnabled(false);
    }
}

void Impedance::ui_stop_sweep()
{
    ui->statusbar->showMessage("Ready.");
    ui->btnSingle->setEnabled(true);
    ui->btnContinuous->setEnabled(true);
    ui->btnContinuous->setChecked(false);
    ui->btnExport->setEnabled(true);
    ui->btnHold->setEnabled(false);
    ui->autoscale_top->setEnabled(true);
    ui->autoscale_bot->setEnabled(true);

    QList<QWidget*> grpSource = ui->grpSource->findChildren<QWidget*>();
    foreach (QWidget* w, grpSource) {
        w->setEnabled(true);
    }

    QList<QWidget*> grpReceive = ui->grpReceive->findChildren<QWidget*>();
    foreach (QWidget* w, grpReceive) {
        w->setEnabled(true);
    }
}

void Impedance::plot_data()
{
    // Prepare data for plot

    QList<QPointF> topPoints;
    QList<QPointF> botPoints;

    HP8751A::instrument_data_t data;
    hp->get_data(data);

    for (int i = 0; i < data.stimulus.length(); i++) {
        topPoints.push_back({data.stimulus.at(i), data.channel1.at(i)});
        botPoints.push_back({data.stimulus.at(i), data.channel2.at(i)});
    }

    top->clear();
    top->append(topPoints);
    top->setName(ui->view_top->currentText());

    bot->clear();
    bot->append(botPoints);
    bot->setName(ui->view_bot->currentText());

    axisX->setMin(data.stimulus.first());
    axisX->setMax(data.stimulus.last());

    double topScale;
    double topRef;
    if (ui->autoscale_top->isChecked()) {
        ui->topScale->setValue(this->topScale);
        ui->topRef->setValue(this->topRef);
        topScale = this->topScale;
        topRef = this->topRef;
    } else {
        topScale = ui->topScale->value();
        topRef = ui->topRef->value();
    }

    axisYTop->setTitleText("Magnitude / dB");
    axisYTop->setLabelFormat("%i");
    axisYTop->setMin(topRef - 5 * topScale);
    axisYTop->setMax(topRef + 5 * topScale);
    axisYTop->setTickType(QValueAxis::TicksDynamic);
    axisYTop->setTickAnchor(topRef);
    axisYTop->setTickInterval((axisYTop->max() - axisYTop->min()) / 10);

    double botScale;
    double botRef;
    if (ui->autoscale_bot->isChecked()) {
        ui->botScale->setValue(this->botScale);
        ui->botRef->setValue(this->botRef);
        botScale = this->botScale;
        botRef = this->botRef;
    } else {
        botScale = ui->botScale->value();
        botRef = ui->botRef->value();
    }

    axisYBot->setTitleText("Phase / °");
    axisYBot->setLabelFormat("%i");
    axisYBot->setMin(botRef - 5 * botScale);
    axisYBot->setMax(botRef + 5 * botScale);
    axisYBot->setTickType(QValueAxis::TicksDynamic);
    axisYBot->setTickAnchor(botRef);
    axisYBot->setTickInterval((axisYBot->max() - axisYBot->min()) / 10);
}

void Impedance::update_parameters()
{
    HP8751A::instrument_parameters_t param;
    param.fStart = ui->startFreq->text().toUInt();
    param.fStop = ui->stopFreq->text().toUInt();
    param.points = ui->numberOfPoints->currentText().toUInt();
    param.power = ui->outputPower->value();
    param.clearPowerTrip = true;
    param.attenR = ui->attenR->currentIndex();
    param.attenA = ui->attenA->currentIndex();
    param.ifbw = static_cast<HP8751A::ifbw_t>(ui->ifBw->currentIndex());
    param.unwrapPhase = ui->unwrapPhase->isChecked();
    param.averFact = 1;
    param.avgEn = false;
    hp->set_instrument_parameters(param);
}

void Impedance::instrument_initialized()
{
    update_parameters();
}

void Impedance::set_parameters_finished()
{
    static bool initialized = false;
    if (!initialized) {
        enable_ui();
        initialized = true;
        init_statemachine_sweep();
    }
}

void Impedance::new_data(HP8751A::instrument_data_t data)
{
    topScale = data.channel1Scale;
    topRef = data.channel1Refpos;
    botScale = data.channel2Scale;
    botRef = data.channel2Refpos;
}

void Impedance::response_timeout()
{
    QMessageBox::critical(this, "Connection timeout", "No response from instrument!");
}

void Impedance::on_autoscale_top_stateChanged(int arg1)
{
    if (arg1) {
        ui->topScale->setEnabled(false);
        ui->topRef->setEnabled(false);
        ui->topScale->setValue(this->topScale);
        ui->topRef->setValue(this->topRef);
    } else {
        ui->topScale->setEnabled(true);
        ui->topRef->setEnabled(true);
    }
}


void Impedance::on_autoscale_bot_stateChanged(int arg1)
{
    if (arg1) {
        ui->botScale->setEnabled(false);
        ui->botRef->setEnabled(false);
        ui->botScale->setValue(this->botScale);
        ui->botRef->setValue(this->botRef);
    } else {
        ui->botScale->setEnabled(true);
        ui->botRef->setEnabled(true);
    }
}

