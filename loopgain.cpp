#include "loopgain.h"
#include "ui_loopgain.h"

Loopgain::Loopgain(HP8751A *hp, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Loopgain)
{
    ui->setupUi(this);
    this->hp = hp;
    if (!this->hp) {
        this->deleteLater();
        return;
    }

    QObject::connect(hp, &HP8751A::new_data, this, &Loopgain::new_data);
    QObject::connect(hp, &HP8751A::response_timeout, this, &Loopgain::response_timeout);
    QObject::connect(hp, &HP8751A::instrument_initialized, this, &Loopgain::instrument_initialized);
    QObject::connect(hp, &HP8751A::set_parameters_finished, this, &Loopgain::set_parameters_finished);

    init();
}

Loopgain::~Loopgain()
{
    delete ui;
}

void Loopgain::closeEvent(QCloseEvent *event)
{
    // Override close button, delete window and return to start dialog
    event->ignore();
    this->deleteLater();
}

void Loopgain::init()
{
    sweepRequested = false;
    disable_ui();
    ui->statusbar->showMessage("Initializing instrument...");
    hp->init_function();
    init_plot();
}

void Loopgain::init_statemachine_sweep()
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

    QObject::connect(sIdle, &QState::entered, this, &Loopgain::ui_stop_sweep);
    sIdle->addTransition(ui->btnSingle, &QPushButton::clicked, sUpdateParameters);
    sIdle->addTransition(ui->btnContinous, &QPushButton::clicked, sUpdateParameters);

    QObject::connect(sUpdateParameters, &QState::entered, this, &Loopgain::ui_start_sweep);
    QObject::connect(sUpdateParameters, &QState::entered, this, &Loopgain::update_parameters);
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

    QObject::connect(sPlotData, &QState::entered, this, &Loopgain::plot_data);
    QObject::connect(sPlotData, &QState::entered, this, [=] {
        if (ui->btnContinous->isChecked()) {
            emit continueSweep(QPrivateSignal());
        } else {
            emit goIdle(QPrivateSignal());
        }
    });
    sPlotData->addTransition(this, &Loopgain::goIdle, sIdle);
    sPlotData->addTransition(this, &Loopgain::continueSweep, sStartSweep);

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

void Loopgain::init_ui()
{
    ui->btnExport->setEnabled(false);
    ui->btnHold->setEnabled(false);
}

void Loopgain::start_sweep()
{
    hp->request_sweep();
}

void Loopgain::disable_ui()
{
    ui->btnHold->setEnabled(false);
    ui->btnExport->setEnabled(false);
    ui->ascale->setEnabled(false);
    ui->aref->setEnabled(false);
    ui->phiscale->setEnabled(false);
    ui->phiref->setEnabled(false);
    ui->centralwidget->setEnabled(false);
}

void Loopgain::enable_ui()
{
    ui->centralwidget->setEnabled(true);
}

void Loopgain::init_plot()
{
    magnitude = new QLineSeries();
    magnitude->setName("Magnitude");

    phase = new QLineSeries();
    phase->setName("Phase");

    chart = new QChart();
    chart->addSeries(magnitude);
    chart->addSeries(phase);

    axisX = new QLogValueAxis();
    axisX->setTitleText("Frequency / Hz");
    axisX->setLabelFormat("%g");
    axisX->setBase(10.0);
    axisX->setMinorTickCount(8);
    chart->addAxis(axisX, Qt::AlignBottom);
    magnitude->attachAxis(axisX);
    phase->attachAxis(axisX);

    axisY = new QValueAxis();
    axisY->setTitleText("Magnitude / dB");
    axisY->setLabelFormat("%i");
    chart->addAxis(axisY, Qt::AlignLeft);
    magnitude->attachAxis(axisY);

    axisYPhase = new QValueAxis();
    axisYPhase->setTitleText("Phase / °");
    axisYPhase->setLabelFormat("%i");
    chart->addAxis(axisYPhase, Qt::AlignRight);
    phase->attachAxis(axisYPhase);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    layout = new QVBoxLayout(ui->chart);
    layout->addWidget(chartView);
}

void Loopgain::plot_data()
{
    // Prepare data for plot

    QList<QPointF> magnitudePoints;
    QList<QPointF> phasePoints;

    HP8751A::instrument_data_t data;
    hp->get_data(data);

    for (int i = 0; i < data.stimulus.length(); i++) {
        magnitudePoints.push_back({data.stimulus.at(i), data.channel1.at(i)});
        phasePoints.push_back({data.stimulus.at(i), data.channel2.at(i)});
    }

    magnitude->clear();
    magnitude->append(magnitudePoints);
    magnitude->setName("Magnitude");

    phase->clear();
    phase->append(phasePoints);
    phase->setName("Phase");

    axisX->setMin(data.stimulus.first());
    axisX->setMax(data.stimulus.last());

    double magnitudeScale;
    double magnitudeRef;
    if (ui->aAutoscale->isChecked()) {
        ui->ascale->setValue(this->magnitudeScale);
        ui->aref->setValue(this->magnitudeRef);
        magnitudeScale = this->magnitudeScale;
        magnitudeRef = this->magnitudeRef;
    } else {
        magnitudeScale = ui->ascale->value();
        magnitudeRef = ui->aref->value();
    }

    axisY->setTitleText("Magnitude / dB");
    axisY->setLabelFormat("%i");
    axisY->setMin(magnitudeRef - 5 * magnitudeScale);
    axisY->setMax(magnitudeRef + 5 * magnitudeScale);
    axisY->setTickType(QValueAxis::TicksDynamic);
    axisY->setTickAnchor(magnitudeRef);
    axisY->setTickInterval((axisY->max() - axisY->min()) / 10);

    double phaseScale;
    double phaseRef;
    if (ui->phiAutoscale->isChecked()) {
        ui->phiscale->setValue(this->phaseScale);
        ui->phiref->setValue(this->phaseRef);
        phaseScale = this->phaseScale;
        phaseRef = this->phaseRef;
    } else {
        phaseScale = ui->phiscale->value();
        phaseRef = ui->phiref->value();
    }

    axisYPhase->setTitleText("Phase / °");
    axisYPhase->setLabelFormat("%i");
    axisYPhase->setMin(phaseRef - 5 * phaseScale);
    axisYPhase->setMax(phaseRef + 5 * phaseScale);
    axisYPhase->setTickType(QValueAxis::TicksDynamic);
    axisYPhase->setTickAnchor(phaseRef);
    axisYPhase->setTickInterval((axisYPhase->max() - axisYPhase->min()) / 10);
}

void Loopgain::update_parameters()
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
    param.avgEn = ui->avgEn->isChecked();
    param.averFact = ui->avgSweeps->currentText().toUInt();
    hp->set_instrument_parameters(param);
}

void Loopgain::ui_start_sweep()
{
    ui->statusbar->showMessage("Updating parameters...");
    if (!ui->btnContinous->isChecked()) {
        // Single sweep mode selected
        ui->btnContinous->setEnabled(false);
    }
    ui->btnSingle->setEnabled(false);
    ui->btnHold->setEnabled(true);
    ui->btnExport->setEnabled(false);
    ui->aAutoscale->setEnabled(false);
    ui->phiAutoscale->setEnabled(false);

    QList<QWidget*> grpSource = ui->grpSource->findChildren<QWidget*>();
    foreach (QWidget* w, grpSource) {
        w->setEnabled(false);
    }

    QList<QWidget*> grpReceive = ui->grpReceive->findChildren<QWidget*>();
    foreach (QWidget* w, grpReceive) {
        w->setEnabled(false);
    }
}

void Loopgain::ui_stop_sweep()
{
    ui->statusbar->showMessage("Ready.");
    ui->btnSingle->setEnabled(true);
    ui->btnContinous->setEnabled(true);
    ui->btnContinous->setChecked(false);
    ui->btnExport->setEnabled(true);
    ui->btnHold->setEnabled(false);
    ui->aAutoscale->setEnabled(true);
    ui->phiAutoscale->setEnabled(true);

    QList<QWidget*> grpSource = ui->grpSource->findChildren<QWidget*>();
    foreach (QWidget* w, grpSource) {
        w->setEnabled(true);
    }

    QList<QWidget*> grpReceive = ui->grpReceive->findChildren<QWidget*>();
    foreach (QWidget* w, grpReceive) {
        w->setEnabled(true);
    }
}

void Loopgain::instrument_initialized()
{
    update_parameters();
}

void Loopgain::set_parameters_finished()
{
    static bool initialized = false;
    if (!initialized) {
        enable_ui();
        initialized = true;
        init_statemachine_sweep();
    }
}

void Loopgain::new_data(HP8751A::instrument_data_t data)
{
    magnitudeScale = data.channel1Scale;
    magnitudeRef = data.channel1Refpos;
    phaseScale = data.channel2Scale;
    phaseRef = data.channel2Refpos;
}

void Loopgain::response_timeout()
{
    QMessageBox::critical(this, "Connection timeout", "No response from instrument!");
}

void Loopgain::on_aAutoscale_stateChanged(int arg1)
{
    if (arg1) {
        ui->ascale->setEnabled(false);
        ui->aref->setEnabled(false);
        ui->ascale->setValue(this->magnitudeScale);
        ui->aref->setValue(this->magnitudeRef);
    } else {
        ui->ascale->setEnabled(true);
        ui->aref->setEnabled(true);
    }
}

void Loopgain::on_phiAutoscale_stateChanged(int arg1)
{
    if (arg1) {
        ui->phiscale->setEnabled(false);
        ui->phiref->setEnabled(false);
        ui->phiscale->setValue(this->phaseScale);
        ui->phiref->setValue(this->phaseRef);
    } else {
        ui->phiscale->setEnabled(true);
        ui->phiref->setEnabled(true);
    }
}

void Loopgain::on_btnExport_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), "data", tr("CSV-Files (*.csv)"));
    QFile file(fileName + ".csv");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->statusbar->showMessage("Could not open file!");
        return;
    }
    QTextStream out(&file);

    QVector<QString> complex;

    HP8751A::instrument_data_t data;
    hp->get_data(data);

    // Calculate complex number from magnitude and phase
    for (int i = 0; i < data.stimulus.size(); i++) {
        double magnitudeLin = std::pow(10, data.channel1.at(i) / 20);
        double phaseRadian = data.channel2.at(i) * M_PI / 180;
        double a = magnitudeLin * std::cos(phaseRadian);
        double b = magnitudeLin * std::sin(phaseRadian);
        complex.push_back(QString("%1%2%3j").arg(a, 0, 'E').arg(b > 0 ? "+" : "").arg(b, 0, 'E'));
    }

    //Write header
    out << "Frequency [Hz],Magnitude [dB],Phase [deg],complex number\r\n";

    for (int i = 0; i < data.stimulus.size(); i++) {
        out << QString("%1,%2,%3,%4\r\n").arg(data.stimulus.at(i), 0, 'E').arg(data.channel1.at(i), 0, 'E')
                   .arg(data.channel2.at(i), 0, 'E').arg(complex.at(i));
    }

    file.close();
    ui->statusbar->showMessage("File written!");
}

void Loopgain::on_aref_valueChanged(double arg1)
{
    (void) arg1;
    plot_data();
}


void Loopgain::on_ascale_valueChanged(double arg1)
{
    (void) arg1;
    plot_data();
}


void Loopgain::on_phiref_valueChanged(double arg1)
{
    (void) arg1;
    plot_data();
}


void Loopgain::on_phiscale_valueChanged(double arg1)
{
    (void) arg1;
    plot_data();
}

void Loopgain::on_chart_customContextMenuRequested(const QPoint &pos)
{
    if (ui->chart->isEnabled()) {
        QMenu menu;
        menu.addAction("Save image");
        auto res = menu.exec(ui->chart->mapToGlobal(pos));

        if (res != nullptr) {
            QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), "plot", tr("PNG-Files (*.png)"));
            if (!fileName.isEmpty()) {
                chartView->grab().save(fileName + ".png");
            }
        }
    }
}



