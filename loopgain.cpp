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

    // Why?? Inside the slot instrument_response(), different commands are requested to the instrument.
    // This fucks up the memory of the last command inside the HP8751A object.
    // -> signal is emitted, slot is called directly, new command requested, slot returns, lastCommand is cleared. Lost track of last command
    // Therefore we call the instrument_response() slot asynchronously.
    // Better approach? I currently don't know...
    QObject::connect(hp, &HP8751A::instrument_response, this, [=](HP8751A::command_t cmd, QString resp, qint8 channel) {
        QMetaObject::invokeMethod(this, [=]() {instrument_response(cmd, resp, channel);}, Qt::QueuedConnection);
    });

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
            if (resp == "1\n") {
                ui->statusbar->showMessage("Instrument initialized.");
                enable_ui();
            }
            break;
        case HP8751A::CMD_POLL_HOLD:
            if (resp == "0\n") {
                poll_hold();
            } else {
                ui->statusbar->showMessage("Retrieving data...");
                fit_trace(0);
            }
            break;
        case HP8751A::CMD_START_SWEEP:
            poll_hold();
            break;

        case HP8751A::CMD_SET_STIMULUS:
            update_receiver();
            break;
        case HP8751A::CMD_SET_RECEIVER:
            start_sweep();
            break;
        case HP8751A::CMD_FIT_TRACE:
            if (resp == "1\n" && channel == 0) {
                fit_trace(1);
            } else if (resp == "1\n" && channel == 1) {
                get_stimulus();
            }
            break;
        case HP8751A::CMD_GET_STIMULUS:
            this->stimulus_raw = resp;
            get_magnitude_data();
            break;
        case HP8751A::CMD_GET_CHANNEL_DATA:
            if (channel == 0) {
                // Magnitude data
                magnitude_raw = resp;
                get_phase_data();
            } else if (channel == 1){
                // Phase data
                phase_raw = resp;
                unpack_raw_data();
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

void Loopgain::start_sweep()
{
    hp->start_sweep(ui->avgEn->isChecked(), ui->avgSweeps->currentText().toUInt());
    ui->statusbar->showMessage("Sweeping... Waiting for instrument.");
    disable_ui_sweep();
}

void Loopgain::fit_trace(quint8 channel)
{
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
    stimulus_raw.chop(1); //Remove \n
    magnitude_raw.chop(1); //Remove \n
    phase_raw.chop(1);

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
    enable_ui_sweep();
}

void Loopgain::disable_ui()
{
    ui->centralwidget->setEnabled(false);
}

void Loopgain::disable_ui_sweep()
{
    ui->btnSingle->setEnabled(false);
    ui->btnContinous->setEnabled(false);
    ui->btnExport->setEnabled(false);
    ui->btnGetTrace->setEnabled(false);
}

void Loopgain::enable_ui_sweep()
{
    ui->btnSingle->setEnabled(true);
    ui->btnContinous->setEnabled(true);
    ui->btnExport->setEnabled(true);
    ui->btnGetTrace->setEnabled(true);
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

    axisX->setMin(ui->startFreq->text().toDouble());
    axisX->setMax(ui->stopFreq->text().toDouble());

    double min = trace_data.at(0).magnitude;
    double max = trace_data.at(0).magnitude;
    //Get min and max value of the magnitude
    for (int i = 1; i < trace_data.size(); i++) {
        if (trace_data.at(i).magnitude < min) {
            min = trace_data.at(i).magnitude;
        }
        if (trace_data.at(i).magnitude > max) {
            max = trace_data.at(i).magnitude;
        }
    }

    qDebug() << "Magnitude min: " << min;
    qDebug() << "Magnitude max: " << max;

    axisY->setTitleText("Magnitude / dB");
    axisY->setLabelFormat("%i");
    axisY->setMin(std::floor(min - 2.5));
    axisY->setMax(std::ceil(max + 2.5));
    axisY->setTickType(QValueAxis::TicksDynamic);
    axisY->setTickAnchor(max);
    axisY->setTickInterval((max - min) / 5);
    axisY->setMinorTickCount(5);

    min = trace_data.at(0).phase;
    max = trace_data.at(0).phase;
    //Get min and max value of the phase
    for (int i = 1; i < trace_data.size(); i++) {
        if (trace_data.at(i).phase < min) {
            min = trace_data.at(i).phase;
        }
        if (trace_data.at(i).phase > max) {
            max = trace_data.at(i).phase;
        }
    }

    qDebug() << "phase min: " << min;
    qDebug() << "phase max: " << max;

    axisYPhase->setTitleText("Phase / °");
    axisYPhase->setLabelFormat("%i");
    axisYPhase->setMin(std::floor(min - 2.5)); //Round to nearest 5 °
    axisYPhase->setMax(std::ceil(max + 2.5)); //Round to nearest 5 °
    axisYPhase->setTickType(QValueAxis::TicksDynamic);
    axisYPhase->setTickAnchor(max);
    axisYPhase->setTickInterval((max - min) / 5);
    axisYPhase->setMinorTickCount(5);
}

void Loopgain::poll_hold()
{
    hp->poll_hold();
}


void Loopgain::on_btnSave_clicked()
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


void Loopgain::on_chart_customContextMenuRequested(const QPoint &pos)
{
    if (ui->chart->isEnabled()) {
        QMenu menu;
        menu.addAction("Save image");
        auto res = menu.exec(ui->chart->mapToGlobal(pos));

        if (res != nullptr) {

        }
    }

}

/*  Behavior of instrument controls:
 *  Hold button is disabled by default. It is only enabled when the instrument is currently sweeping (HOLD? returns 0)
 *  When continous sweep is selected, the single button is disabled. Hold button is enabled. Cont. button is checked.
 *      When cont. button is clicked again, it will finish the current sweep and will hold afterwards.
 *      Clicking the hold button during a sweep cancels the current sweep.
 *  When single sweep is selected, cont. button is disabled while the instrument is sweeping. Hold button is enabled.
 *  Enable export button and get trace button only when data has been received and instrument is in hold mode.
 *
 */
void Loopgain::on_btnContinous_toggled(bool checked)
{

}


void Loopgain::on_btnSingle_clicked()
{
    update_stimulus();
    disable_ui_sweep();
}


void Loopgain::on_btnHold_clicked()
{

}


void Loopgain::on_btnGetTrace_clicked()
{

}


void Loopgain::on_btnExport_clicked()
{

}

