#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setFixedSize(this->size());

    ui->labelAveraging->setEnabled(false);
    ui->avgSweeps->setEnabled(false);
    disable_ui();
    ui->btnStart->setEnabled(false);
    ui->btnSave->setEnabled(false);

    gpib = new PrologixGPIB(this);
    QHostAddress addr = QHostAddress("192.168.178.153");
    QObject::connect(gpib, &PrologixGPIB::response, this, &MainWindow::gpib_response);
    QObject::connect(gpib, &PrologixGPIB::stateChanged, this, [=](QAbstractSocket::SocketState state) {
        qDebug() << state;
        switch (state) {
        case QAbstractSocket::ConnectingState:
            ui->statusbar->showMessage("Connecting to instrument...");
            break;
        case QAbstractSocket::UnconnectedState:
            ui->statusbar->showMessage("Could not connect to instrument!");
            break;
        case QAbstractSocket::ConnectedState:
            lastAction = ACTION_NO_ACTION;
            request_instrument();
            break;
        default:
            break;
        }
    });

    instrument_gpib_id = 17;
    gpib->init(addr);


    pollHold = new QTimer(this);
    pollHold->setInterval(500);
    QObject::connect(pollHold, &QTimer::timeout, this, &MainWindow::pollHold_timeout);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::request_instrument()
{
    // Is the 8751A on the bus?
    lastAction = ACTION_INSTRUMENT_REQUEST;
    gpib->send_command(instrument_gpib_id, "*IDN?");
}

void MainWindow::init_instrument()
{
    QVector<QString> commands;
    instrument_init_commands(commands);
    QString cmdString;
    cmdString.clear();
    for (int i = 0; i < commands.length(); i++) {
        cmdString.append(commands.at(i));
    }

    gpib->send_command(instrument_gpib_id, cmdString);
    lastAction = ACTION_INSTRUMENT_INIT;
}

void MainWindow::instrument_init_commands(QVector<QString> &commands)
{
    //commands.push_back("PRES;*OPC?");
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

void MainWindow::gpib_response(QString resp)
{
    qDebug() << "Response for action " << lastAction << ": " << resp;
    switch (lastAction) {
    case ACTION_NO_ACTION:
        break;
    case ACTION_INSTRUMENT_REQUEST:
        if (resp.contains("8751A")) {
            ui->statusbar->showMessage("Instrument found. Initializing...");
            lastAction = ACTION_NO_ACTION;
            init_instrument();
        }
        break;
    case ACTION_INSTRUMENT_INIT:
        if (resp == "1\n") {
            lastAction = ACTION_NO_ACTION;
            ui->statusbar->showMessage("Instrument ready.");
            enable_ui();
            ui->btnStart->setEnabled(true);
        }
        break;
    case ACTION_SETTINGS_UPDATE:
        if (resp == "1\n") {
            lastAction = ACTION_NO_ACTION;
            start_sweep();
        }
        break;
    case ACTION_SWEEP_STARTED:
        if (resp == "1\n") {
            lastAction = ACTION_NO_ACTION;
        }
        break;
    case ACTION_POLL_SWEEP_FINISH:
        if (resp == "1\n") {
            pollHold->stop();
            while(pollHold->isActive()); //Dirty wait for timer to finish
            lastAction = ACTION_NO_ACTION;
            fit_trace();
        }
        break;
    case ACTION_FIT_TRACE:
        if (resp == "1\n") {
            lastAction = ACTION_NO_ACTION;
            get_stimulus();
            ui->statusbar->showMessage("Retrieving data...");
        }
        break;
    case ACTION_TRANSFER_STIMULUS:
        stimulus_raw.append(resp);
        if (stimulus_raw.contains("\n")) {
            lastAction = ACTION_NO_ACTION;
            get_magnitude_data();
        }
        break;
    case ACTION_TRANSFER_MAGNITUDE:
        magnitude_raw.append(resp);
        if (magnitude_raw.contains("\n")) {
            lastAction = ACTION_NO_ACTION;
            get_phase_data();
        }
        break;
    case ACTION_TRANSFER_PHASE:
        phase_raw.append(resp);
        if (phase_raw.contains("\n")) {
            lastAction = ACTION_NO_ACTION;
            unpack_raw_data();
        }
        break;
    case ACTION_CANCEL_SWEEP:
        if (resp == "1\n") {
            enable_ui();
            ui->statusbar->showMessage("Sweep cancelled. Ready.");
        }
        break;
    }
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
    pollHold->start();
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

    magnitude = new QLineSeries();
    magnitude->append(magnitudePoints);
    magnitude->setPointLabelsVisible(false);
    magnitude->setPointLabelsFormat("(@xPoint, @yPoint)");
    magnitude->setName("Magnitude");

    phase = new QLineSeries();
    phase->append(phasePoints);
    phase->setPointLabelsVisible(false);
    phase->setPointLabelsFormat("(@xPoint, @yPoint)");
    phase->setName("Phase");

    QChart *chart = new QChart();
    chart->addSeries(magnitude);
    chart->addSeries(phase);

    axisX = new QLogValueAxis();
    axisX->setTitleText("Frequency / Hz");
    axisX->setLabelFormat("%g");
    axisX->setBase(10.0);
    axisX->setMinorTickCount(8);
    axisX->setMin(ui->startFreq->text().toDouble());
    axisX->setMax(ui->stopFreq->text().toDouble());
    chart->addAxis(axisX, Qt::AlignBottom);
    magnitude->attachAxis(axisX);
    phase->attachAxis(axisX);

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

    axisY = new QValueAxis();
    axisY->setTitleText("Magnitude / dB");
    axisY->setLabelFormat("%i");
    axisY->setMin(std::floor(min - 2.5));
    axisY->setMax(std::ceil(max + 2.5));
    axisY->setTickType(QValueAxis::TicksDynamic);
    axisY->setTickAnchor(max);
    axisY->setTickInterval((max - min) / 5);
    axisY->setMinorTickCount(5);
    chart->addAxis(axisY, Qt::AlignLeft);
    magnitude->attachAxis(axisY);

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

    axisYPhase = new QValueAxis();
    axisYPhase->setTitleText("Phase / °");
    axisYPhase->setLabelFormat("%i");
    axisYPhase->setMin(std::floor(min - 2.5)); //Round to nearest 5 °
    axisYPhase->setMax(std::ceil(max + 2.5)); //Round to nearest 5 °
    axisYPhase->setTickType(QValueAxis::TicksDynamic);
    axisYPhase->setTickAnchor(max);
    axisYPhase->setTickInterval((max - min) / 5);
    axisYPhase->setMinorTickCount(5);
    chart->addAxis(axisYPhase, Qt::AlignRight);
    phase->attachAxis(axisYPhase);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    QVBoxLayout *layout = new QVBoxLayout(ui->chart);
    layout->addWidget(chartView);
}


void MainWindow::on_btnStart_clicked()
{
    if (ui->btnStart->text() == "Start") {
        update_settings();
        lastAction = ACTION_SETTINGS_UPDATE;
        // Sweep will be started asynchronous after settings update was successful
    } else {
        pollHold->stop();
        while (pollHold->isActive());
        gpib->send_command(instrument_gpib_id, "HOLD;*OPC?");
        lastAction = ACTION_CANCEL_SWEEP;
    }
}


void MainWindow::on_btnSave_clicked()
{

}

