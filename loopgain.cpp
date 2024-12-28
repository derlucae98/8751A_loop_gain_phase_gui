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

    QObject::connect(hp, &HP8751A::instrument_response, this, &Loopgain::instrument_response);

    ui->btnHold->setEnabled(false);
    ui->btnExport->setEnabled(false);
    ui->ascale->setEnabled(false);
    ui->aref->setEnabled(false);
    ui->phiscale->setEnabled(false);
    ui->phiref->setEnabled(false);
    init_sweep_statemachine();
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

void Loopgain::instrument_response(HP8751A::command_t cmd, QString resp, qint8 channel)
{
    qDebug() << "CMD: " << cmd <<", Resp: " << resp << ", ch: " << channel;
    switch (cmd) {
        case HP8751A::CMD_INIT_TRANSFERFUNCTION:
            if (resp == "1") {
                ui->statusbar->showMessage("Instrument initialized.");
                enable_ui();
            }
            break;
        case HP8751A::CMD_POLL_HOLD:
            if (resp == "0") {
                emit responseNOK(QPrivateSignal());
            } else {
                emit responseOK(QPrivateSignal());
            }
            break;
        case HP8751A::CMD_START_SWEEP:
        case HP8751A::CMD_SET_STIMULUS:
        case HP8751A::CMD_SET_RECEIVER:
        case HP8751A::CMD_SET_PHASE_FORMAT:
            emit responseOK(QPrivateSignal());
            break;

        case HP8751A::CMD_CANCEL_SWEEP:
            emit responseOK(QPrivateSignal());
            break;

        case HP8751A::CMD_FIT_TRACE:
            {
                QStringList respSeparate = resp.split(";");
                if (channel == 0) {
                    magnitudeScale = respSeparate.at(0).toFloat();
                    magnitudeRef = respSeparate.at(1).toFloat();
                } else {
                    phaseScale = respSeparate.at(0).toFloat();
                    phaseRef = respSeparate.at(1).toFloat();
                }
                emit responseOK(QPrivateSignal());
                break;
            }

        case HP8751A::CMD_GET_STIMULUS:
            this->stimulus_raw = resp;
            emit responseOK(QPrivateSignal());
            break;
        case HP8751A::CMD_GET_CHANNEL_DATA:
            if (channel == 0) {
                // Magnitude data
                magnitude_raw = resp;
                emit responseOK(QPrivateSignal());
            } else if (channel == 1){
                // Phase data
                phase_raw = resp;
                emit responseOK(QPrivateSignal());
            }
            break;
    }
}

void Loopgain::init()
{
    disable_ui();
    ui->statusbar->showMessage("Initializing instrument...");
    hp->init_transferfunction();
    init_plot();
}

void Loopgain::init_ui()
{
    ui->btnExport->setEnabled(false);
    ui->btnHold->setEnabled(false);
}

void Loopgain::update_stimulus()
{
    hp->set_stimulus(ui->startFreq->text().toUInt(),
                     ui->stopFreq->text().toUInt(),
                     ui->numberOfPoints->currentText().toUInt(),
                     ui->outputPower->value(),
                     true);

}

void Loopgain::update_receiver()
{
    hp->set_receiver(ui->attenR->currentIndex(),
                     ui->attenA->currentIndex(),
                     static_cast<HP8751A::ifbw_t>(ui->ifBw->currentIndex()));
}

void Loopgain::update_phase_format()
{
    hp->set_phase_format(1, ui->unwrapPhase->isChecked());
}

void Loopgain::start_sweep()
{
    hp->start_sweep(ui->avgEn->isChecked(), ui->avgSweeps->currentText().toUInt());
}

void Loopgain::hold_sweep()
{
    ui->statusbar->showMessage("Cancelling sweep...");
    hp->cancel_sweep();
}

void Loopgain::fit_trace(quint8 channel)
{
    ui->btnGetTrace->setEnabled(false);
    ui->statusbar->showMessage("Retrieving data...");
    hp->fit_trace(channel);
}

void Loopgain::get_stimulus()
{
    hp->get_stimulus();
}

void Loopgain::get_magnitude_data()
{
    hp->get_channel_data(0);
}

void Loopgain::get_phase_data()
{
    hp->get_channel_data(1);
}

void Loopgain::unpack_raw_data()
{
    QStringList stimulus;
    stimulus = stimulus_raw.split(',');

    trace_data.resize(stimulus.size());

    QStringList magnitude;
    magnitude = magnitude_raw.split(',');

    QStringList phase;
    phase = phase_raw.split(',');

    //qDebug() << "Stimulus size: " << stimulus.size() << ", Trace size: " << trace.size();

    bool ok;
    for (int i = 0; i < stimulus.size(); i++) {
        trace_data[i].frequency = stimulus.at(i).toDouble(&ok);
        if (!ok) {
            qDebug() << "err parsing string for frequency at point " << i;
        }
        trace_data[i].magnitude = magnitude.at(2 * i).toDouble(&ok);
        if (!ok) {
            qDebug() << "err parsing string for magnitude at point " << i;
        }
        trace_data[i].phase = phase.at(2 * i).toDouble(&ok);
        if (!ok) {
            qDebug() << "err parsing string for phase at point " << i;
        }
        qDebug() << "f: " << trace_data.at(i).frequency << ", Magnitude: " << trace_data.at(i).magnitude << ", Phase: " << trace_data.at(i).phase;
    }

    enable_ui();
    plot_data();
    ui->statusbar->showMessage("Ready.");
}

void Loopgain::disable_ui()
{
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

    for (int i = 0; i < trace_data.length(); i++) {
        magnitudePoints.push_back({trace_data.at(i).frequency, trace_data.at(i).magnitude});
        phasePoints.push_back({trace_data.at(i).frequency, trace_data.at(i).phase});
    }

    magnitude->clear();
    magnitude->append(magnitudePoints);
    magnitude->setName("Magnitude");

    phase->clear();
    phase->append(phasePoints);
    phase->setName("Phase");

    axisX->setMin(trace_data.first().frequency);
    axisX->setMax(trace_data.last().frequency);

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

void Loopgain::poll_hold()
{
    hp->poll_hold();
}

void Loopgain::init_sweep_statemachine()
{
    QStateMachine *smSingleSweep = new QStateMachine(this);
    QState *sIdle = new QState();
    QState *sUpdateStimulus = new QState();
    QState *sUpdateReceiver = new QState();
    QState *sSetPhaseFormat = new QState();
    QState *sStartSweep = new QState();
    QState *sPollHold = new QState();
    QState *sFitTrace1 = new QState();
    QState *sFitTrace2 = new QState();
    QState *sGetStimulus = new QState();
    QState *sGetTrace1 = new QState();
    QState *sGetTrace2 = new QState();
    QState *sPlotData = new QState();
    QState *sHold = new QState();
    QState *sStop = new QState();

    QObject::connect(sUpdateStimulus, &QState::entered, this, &Loopgain::update_stimulus);
    QObject::connect(sUpdateReceiver, &QState::entered, this, &Loopgain::update_receiver);
    QObject::connect(sSetPhaseFormat, &QState::entered, this, &Loopgain::update_phase_format);
    QObject::connect(sStartSweep, &QState::entered, this, &Loopgain::start_sweep);
    QObject::connect(sPollHold, &QState::entered, this, &Loopgain::poll_hold);
    QObject::connect(sFitTrace1, &QState::entered, this, [=]{fit_trace(0);});
    QObject::connect(sFitTrace2, &QState::entered, this, [=]{fit_trace(1);});
    QObject::connect(sGetStimulus, &QState::entered, this, &Loopgain::get_stimulus);
    QObject::connect(sGetTrace1, &QState::entered, this, &Loopgain::get_magnitude_data);
    QObject::connect(sGetTrace2, &QState::entered, this, &Loopgain::get_phase_data);
    QObject::connect(sPlotData, &QState::entered, this, &Loopgain::unpack_raw_data);
    QObject::connect(sUpdateStimulus, &QState::entered, this, &Loopgain::ui_start_sweep);

    QObject::connect(sHold, &QState::entered, this, &Loopgain::hold_sweep);

    QObject::connect(sStop, &QState::entered, this, &Loopgain::ui_stop_sweep);
    QObject::connect(sStop, &QState::entered, this, [=] {
        if (ui->btnContinous->isChecked()) {
            emit continueSweep(QPrivateSignal());
        } else {
            emit goIdle(QPrivateSignal());
        }
    });

    sIdle->addTransition(ui->btnSingle, &QPushButton::clicked, sUpdateStimulus);
    sIdle->addTransition(ui->btnContinous, &QPushButton::toggled, sUpdateStimulus);
    sIdle->addTransition(ui->btnGetTrace, &QPushButton::clicked, sFitTrace1);

    sUpdateStimulus->addTransition(this, &Loopgain::responseOK, sUpdateReceiver);
    sUpdateStimulus->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sUpdateReceiver->addTransition(this, &Loopgain::responseOK, sSetPhaseFormat);
    sUpdateReceiver->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sSetPhaseFormat->addTransition(this, &Loopgain::responseOK, sStartSweep);
    sSetPhaseFormat->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sStartSweep->addTransition(this, &Loopgain::responseOK, sPollHold);
    sStartSweep->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sPollHold->addTransition(this, &Loopgain::responseOK, sFitTrace1);
    sPollHold->addTransition(this, &Loopgain::responseNOK, sPollHold);
    sPollHold->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sFitTrace1->addTransition(this, &Loopgain::responseOK, sFitTrace2);
    sFitTrace1->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sFitTrace2->addTransition(this, &Loopgain::responseOK, sGetStimulus);
    sFitTrace2->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sGetStimulus->addTransition(this, &Loopgain::responseOK, sGetTrace1);
    sGetStimulus->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sGetTrace1->addTransition(this, &Loopgain::responseOK, sGetTrace2);
    sGetTrace1->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sGetTrace2->addTransition(this, &Loopgain::responseOK, sPlotData);
    sGetTrace2->addTransition(ui->btnHold, &QPushButton::clicked, sHold);

    sHold->addTransition(this, &Loopgain::responseOK, sStop);

    sPlotData->addTransition(sPlotData, &QState::entered, sStop);

    sStop->addTransition(this, &Loopgain::goIdle, sIdle);
    sStop->addTransition(this, &Loopgain::continueSweep, sUpdateStimulus);


    smSingleSweep->addState(sUpdateStimulus);
    smSingleSweep->addState(sUpdateReceiver);
    smSingleSweep->addState(sSetPhaseFormat);
    smSingleSweep->addState(sStartSweep);
    smSingleSweep->addState(sPollHold);
    smSingleSweep->addState(sFitTrace1);
    smSingleSweep->addState(sFitTrace2);
    smSingleSweep->addState(sGetStimulus);
    smSingleSweep->addState(sGetTrace1);
    smSingleSweep->addState(sGetTrace2);
    smSingleSweep->addState(sPlotData);
    smSingleSweep->addState(sHold);
    smSingleSweep->addState(sStop);
    smSingleSweep->addState(sIdle);

    smSingleSweep->setInitialState(sIdle);
    smSingleSweep->start();
}

void Loopgain::ui_start_sweep()
{
    ui->statusbar->showMessage("Sweeping...");
    if (!ui->btnContinous->isChecked()) {
        // Single sweep mode selected
        ui->btnContinous->setEnabled(false);
    }
    ui->btnSingle->setEnabled(false);
    ui->btnHold->setEnabled(true);
    ui->btnExport->setEnabled(false);
    ui->btnGetTrace->setEnabled(false);

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
    ui->btnExport->setEnabled(true);
    ui->btnGetTrace->setEnabled(true);
    ui->btnHold->setEnabled(false);

    QList<QWidget*> grpSource = ui->grpSource->findChildren<QWidget*>();
    foreach (QWidget* w, grpSource) {
        w->setEnabled(true);
    }

    QList<QWidget*> grpReceive = ui->grpReceive->findChildren<QWidget*>();
    foreach (QWidget* w, grpReceive) {
        w->setEnabled(true);
    }
}

void Loopgain::on_chart_customContextMenuRequested(const QPoint &pos)
{
    if (ui->chart->isEnabled()) {
        QMenu menu;
        menu.addAction("Save image");
        auto res = menu.exec(ui->chart->mapToGlobal(pos));

        if (res != nullptr) {
            QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), "plot", tr("PNG-Files (*.png)"));
            chartView->grab().save(fileName + ".png");
        }
    }

}

/*  Behavior of instrument controls:
 *  Hold button is disabled by default. It is only enabled when the instrument is currently sweeping (HOLD? returns 0)
 *  When continous sweep is selected, the single button is disabled. Hold button is enabled. Cont. button is checked.
 *      When cont. button is clicked again, it will finish the current sweep and will hold afterwards.
 *      Clicking the hold button during a sweep cancels the current sweep.
 *  When single sweep is selected, cont. button is disabled while the instrument is sweeping. Hold button is enabled.
 *  Enable export button only when data has been received and instrument is in hold mode.
 *
 */

void Loopgain::on_btnHold_clicked()
{
    ui->btnContinous->setChecked(false);
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

    // Calculate complex number from magnitude and phase
    for (int i = 0; i < trace_data.size(); i++) {
        double magnitudeLin = std::pow(10, trace_data.at(i).magnitude / 20);
        double phaseRadian = trace_data.at(i).phase * M_PI / 180;
        double a = magnitudeLin * std::cos(phaseRadian);
        double b = magnitudeLin * std::sin(phaseRadian);
        complex.push_back(QString("%1%2%3j").arg(a, 0, 'E').arg(b > 0 ? "+" : "").arg(b, 0, 'E'));
    }

    //Write header
    out << "Frequency [Hz],Magnitude [dB],Phase [deg],complex number\r\n";

    for (int i = 0; i < trace_data.size(); i++) {
        out << QString("%1,%2,%3,%4\r\n").arg(trace_data.at(i).frequency, 0, 'E').arg(trace_data.at(i).magnitude, 0, 'E')
                   .arg(trace_data.at(i).phase, 0, 'E').arg(complex.at(i));
    }

    file.close();
    ui->statusbar->showMessage("File written!");
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
        phaseScale = this->phaseScale;
        phaseRef = this->phaseRef;
    } else {
        ui->phiscale->setEnabled(true);
        ui->phiref->setEnabled(true);
    }
}


void Loopgain::on_aref_valueChanged(double arg1)
{
    plot_data();
}


void Loopgain::on_ascale_valueChanged(double arg1)
{
    plot_data();
}


void Loopgain::on_phiref_valueChanged(double arg1)
{
    plot_data();
}


void Loopgain::on_phiscale_valueChanged(double arg1)
{
    plot_data();
}

