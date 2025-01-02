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
    hp->init_function(HP8751A::PORT_AR, HP8751A::CONV_Z_REFL, HP8751A::FMT_LINM, HP8751A::PORT_AR, HP8751A::CONV_OFF, HP8751A::FMT_PHAS);
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
    axisXTop = new QLogValueAxis();
    axisXTop->setTitleText("Frequency / Hz");
    axisXTop->setLabelFormat("%g");
    axisXTop->setBase(10.0);
    axisXTop->setMinorTickCount(8);

    QChart *chartTop = nullptr;
    QChartView *chartViewTop = nullptr;
    QVBoxLayout *layoutTop = nullptr;
    axisYTop = new QValueAxis();
    top = new QLineSeries();
    chartTop = new QChart();

    chartTop->addSeries(top);
    chartTop->addAxis(axisXTop, Qt::AlignBottom);
    axisYTop->setTitleText("top / dB");
    axisYTop->setLabelFormat("%i");
    chartTop->addAxis(axisYTop, Qt::AlignLeft);
    top->attachAxis(axisXTop);
    top->attachAxis(axisYTop);
    top->setName(ui->view_top->currentText());

    chartViewTop = new QChartView(chartTop);
    chartViewTop->setRenderHint(QPainter::Antialiasing);
    layoutTop = new QVBoxLayout(ui->chart_top);
    layoutTop->addWidget(chartViewTop);

    axisXBot = new QLogValueAxis();
    axisXBot->setTitleText("Frequency / Hz");
    axisXBot->setLabelFormat("%g");
    axisXBot->setBase(10.0);
    axisXBot->setMinorTickCount(8);

    QChart *chartBot = nullptr;
    QChartView *chartViewBot = nullptr;
    QVBoxLayout *layoutBot = nullptr;
    axisYBot = new QValueAxis();
    bot = new QLineSeries();
    chartBot = new QChart();

    chartBot->addSeries(bot);
    chartBot->addAxis(axisXBot, Qt::AlignBottom);
    axisYBot->setTitleText("bot / °");
    axisYBot->setLabelFormat("%i");
    chartBot->addAxis(axisYBot, Qt::AlignLeft);
    bot->attachAxis(axisXBot);
    bot->attachAxis(axisYBot);
    bot->setName(ui->view_bot->currentText());

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

    axisXTop->setMin(data.stimulus.first());
    axisXTop->setMax(data.stimulus.last());

    axisXBot->setMin(data.stimulus.first());
    axisXBot->setMax(data.stimulus.last());

    double topScale;
    double topRefVal;
    if (ui->autoscale_top->isChecked()) {
        ui->topScale->setValue(this->topScale);
        ui->topRef->setValue(this->topRefVal);
        topScale = this->topScale;
        topRefVal = this->topRefVal;
    } else {
        topScale = ui->topScale->value();
        topRefVal = ui->topRef->value();
    }

    axisYTop->setTitleText("Magnitude / dB");
    axisYTop->setLabelFormat("%i");
    axisYTop->setMin(topRefVal - 5 * topScale);
    axisYTop->setMax(topRefVal + 5 * topScale);
    axisYTop->setTickType(QValueAxis::TicksDynamic);
    axisYTop->setTickAnchor(topRefVal);
    axisYTop->setTickInterval((axisYTop->max() - axisYTop->min()) / 10);
    axisYTop->setLabelFormat("%.2f");

    double botScale;
    double botRefVal;
    if (ui->autoscale_bot->isChecked()) {
        ui->botScale->setValue(this->botScale);
        ui->botRef->setValue(this->botRefVal);
        botScale = this->botScale;
        botRefVal = this->botRefVal;
    } else {
        botScale = ui->botScale->value();
        botRefVal = ui->botRef->value();
    }

    axisYBot->setTitleText("Phase / °");
    axisYBot->setLabelFormat("%i");
    axisYBot->setMin(botRefVal - 5 * botScale);
    axisYBot->setMax(botRefVal + 5 * botScale);
    axisYBot->setTickType(QValueAxis::TicksDynamic);
    axisYBot->setTickAnchor(topRefVal);
    axisYBot->setTickInterval((axisYBot->max() - axisYBot->min()) / 10);
    axisYBot->setLabelFormat("%.2f");
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
    topRefVal = data.channel1RefVal;
    botScale = data.channel2Scale;
    botRefVal = data.channel2RefVal;
}

void Impedance::response_timeout()
{
    QMessageBox::critical(this, "Connection timeout", "No response from instrument!");
    ui->statusbar->showMessage("No response from instrument!");
}

void Impedance::on_autoscale_top_stateChanged(int arg1)
{
    if (arg1) {
        ui->topScale->setEnabled(false);
        ui->topRef->setEnabled(false);
        ui->topScale->setValue(this->topScale);
        ui->topRef->setValue(this->topRefVal);
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
        ui->botRef->setValue(this->botRefVal);
    } else {
        ui->botScale->setEnabled(true);
        ui->botRef->setEnabled(true);
    }
}


void Impedance::on_topRef_valueChanged(double arg1)
{
    (void) arg1;
    plot_data();
}


void Impedance::on_topScale_valueChanged(double arg1)
{
    (void) arg1;
    plot_data();
}


void Impedance::on_botRef_valueChanged(double arg1)
{
    (void) arg1;
    plot_data();
}


void Impedance::on_botScale_valueChanged(double arg1)
{
    (void) arg1;
    plot_data();
}


void Impedance::on_view_top_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        ui->unitTopRef->setText("Ω");
        ui->unitTopScale->setText("Ω");
        break;
    case 1:
        ui->unitTopRef->setText("H");
        ui->unitTopScale->setText("H");
        break;
    case 2:
        ui->unitTopRef->setText("F");
        ui->unitTopScale->setText("F");
        break;
    case 3:
        ui->unitTopRef->setText("Ω");
        ui->unitTopScale->setText("Ω");
        break;
    case 4:
        ui->unitTopRef->setText("°");
        ui->unitTopScale->setText("°");
        break;
    }
}


void Impedance::on_view_bot_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        ui->unitBotRef->setText("Ω");
        ui->unitBotScale->setText("Ω");
        break;
    case 1:
        ui->unitBotRef->setText("H");
        ui->unitBotScale->setText("H");
        break;
    case 2:
        ui->unitBotRef->setText("F");
        ui->unitBotScale->setText("F");
        break;
    case 3:
        ui->unitBotRef->setText("Ω");
        ui->unitBotScale->setText("Ω");
        break;
    case 4:
        ui->unitBotRef->setText("°");
        ui->unitBotScale->setText("°");
        break;
    }
}

