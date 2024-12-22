#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::init_loopgain()
{
    ui->labelAveraging->setEnabled(false);
    ui->avgSweeps->setEnabled(false);
    disable_ui();
    ui->btnStart->setEnabled(false);
    ui->btnSave->setEnabled(false);
    init_plot();
    ui->chart->setEnabled(false);
    ui->btnGetTrace->setEnabled(false);

    pollHold = new QTimer(this);
    pollHold->setInterval(500);
    QObject::connect(pollHold, &QTimer::timeout, this, &MainWindow::pollHold_timeout);
}


void MainWindow::instrument_init_commands(QVector<QString> &commands)
{
    commands.push_back("POWE -20;");
    commands.push_back("STAR 10;");
    commands.push_back("STOP 1MHZ;");
    commands.push_back("POIN 201;");
    commands.push_back("ATTIA20DB;");
    commands.push_back("ATTIR20DB;");
    commands.push_back("IFBW 20;");
    commands.push_back("LOGFREQ;");
    commands.push_back("DUACON;");
    commands.push_back("SPLDON;");
    commands.push_back("HOLD;");
    commands.push_back("CHAN1;");
    commands.push_back("AR;");
    commands.push_back("FMT LOGM;");
    commands.push_back("CHAN2;");
    commands.push_back("AR;");
    commands.push_back("FMT EXPP;");
    commands.push_back("*OPC?");
}


void MainWindow::update_settings()
{
    QString instrumentSettings;
    instrumentSettings.clear();
    instrumentSettings.append(QString("STAR %1;").arg(ui->startFreq->text()));
    instrumentSettings.append(QString("STOP %1;").arg(ui->stopFreq->text()));
    instrumentSettings.append(QString("POIN %1;").arg(ui->numberOfPoints->currentText()));
    instrumentSettings.append(QString("POWE %1;").arg(ui->outputPower->value()));
    if (ui->attenA->currentIndex() == 0) {
        instrumentSettings.append("ATTIA0DB;");
    } else {
        instrumentSettings.append("ATTIA20DB;");
    }
    if (ui->attenR->currentIndex() == 0) {
        instrumentSettings.append("ATTIR0DB;");
    } else {
        instrumentSettings.append("ATTIR20DB;");
    }
    QString ifbw = ui->ifBw->currentText();
    ifbw.chop(3); //Remove " Hz" unit
    instrumentSettings.append(QString("IFBW %1;").arg(ifbw));
    instrumentSettings.append("CLEPTRIP;");
    instrumentSettings.append("*OPC?");

    gpib->send_command(instrument_gpib_id, instrumentSettings);
}

void MainWindow::start_sweep()
{
    QString commands;
    commands.clear();

    if (not ui->avgEn->isChecked()) {
        // Single sweep when averaging is off
        commands.append("CHAN1;AVEROFF;CHAN2;AVEROFF;");
        commands.append("SING;");
        commands.append("*OPC?");
        gpib->send_command(instrument_gpib_id, commands);
    } else {
        // n number of sweeps for averaging factor of n
        commands.append("CHAN1;AVERON;");
        commands.append(QString("AVERFACT %1;").arg(ui->avgSweeps->currentText()));
        commands.append("CHAN2;AVERON;");
        commands.append(QString("AVERFACT %1;").arg(ui->avgSweeps->currentText()));
        commands.append(QString("NUMG %1;").arg(ui->avgSweeps->currentText()));
        commands.append("*OPC?");
        gpib->send_command(instrument_gpib_id, commands);
    }
    lastAction = ACTION_SWEEP_STARTED;
    ui->btnSave->setEnabled(false);
    disable_ui();
    ui->btnStart->setText("Stop");
    ui->statusbar->showMessage("Sweeping... Waiting for instrument.");
}

void MainWindow::pollHold_timeout()
{
    QString commands;
    commands.clear();
    commands.append("HOLD?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_POLL_SWEEP_FINISH;
}

void MainWindow::fit_trace()
{
    QString commands;
    commands.clear();
    // Auto scale both channels. Not relevant for getting the trace data but for viewing on the instrument screen
    commands.append("CHAN1;");
    commands.append("AUTO;");
    commands.append("CHAN2;");
    commands.append("AUTO;");
    commands.append("CHAN1;");
    commands.append("*OPC?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_FIT_TRACE;
}

void MainWindow::get_stimulus()
{
    // Get stimulus frequencies
    QString commands;
    commands.clear();
    commands.append("OUTPSTIM?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_TRANSFER_STIMULUS;
    stimulus_raw.clear();
}

void MainWindow::get_magnitude_data()
{
    // Get trace data of Channel 1 in dB
    QString commands;
    commands.clear();
    commands.append("OUTPFORM?;CHAN2;*OPC?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_TRANSFER_MAGNITUDE;
    magnitude_raw.clear();
}

void MainWindow::get_phase_data()
{
    // Get trace data of Channel 2 in Format degree
    QString commands;
    commands.clear();
    commands.append("OUTPFORM?");
    gpib->send_command(instrument_gpib_id, commands);
    lastAction = ACTION_TRANSFER_PHASE;
    phase_raw.clear();
}

void MainWindow::unpack_raw_data()
{
    stimulus_raw.chop(1); //Remove \n
    magnitude_raw.chop(3); //Remove ;1\n
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
    ui->btnSave->setEnabled(true);
    ui->btnGetTrace->setEnabled(true);
}

void MainWindow::disable_ui()
{
    QList<QWidget*> grpSource = ui->grpSource->findChildren<QWidget*>();
    foreach (QWidget* w, grpSource) {
        w->setEnabled(false);
    }

    QList<QWidget*> grpReceive = ui->grpReceive->findChildren<QWidget*>();
    foreach (QWidget* w, grpReceive) {
        w->setEnabled(false);
    }

    ui->chart->setEnabled(false);
    ui->btnGetTrace->setEnabled(false);
}

void MainWindow::enable_ui()
{
    QList<QWidget*> grpSource = ui->grpSource->findChildren<QWidget*>();
    foreach (QWidget* w, grpSource) {
        w->setEnabled(true);
    }

    QList<QWidget*> grpReceive = ui->grpReceive->findChildren<QWidget*>();
    foreach (QWidget* w, grpReceive) {
        w->setEnabled(true);
    }
    ui->btnStart->setText("Start");

    ui->chart->setEnabled(true);
}

void MainWindow::init_plot()
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

void MainWindow::plot_data()
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


void MainWindow::on_btnStart_clicked()
{
    if (ui->btnStart->text() == "Start") {
        update_settings();
        lastAction = ACTION_SETTINGS_UPDATE;
        // Sweep will be started asynchronously after settings update was successful
    } else {
        pollHold->stop();
        while (pollHold->isActive());
        gpib->send_command(instrument_gpib_id, "HOLD;*OPC?");
        lastAction = ACTION_CANCEL_SWEEP;
    }
}


void MainWindow::on_btnSave_clicked()
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


void MainWindow::on_chart_customContextMenuRequested(const QPoint &pos)
{
    if (ui->chart->isEnabled()) {
        QMenu menu;
        menu.addAction("Save image");
        auto res = menu.exec(ui->chart->mapToGlobal(pos));

        if (res != nullptr) {

        }
    }

}
