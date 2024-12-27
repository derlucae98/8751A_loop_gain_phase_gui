#include "hp8751a.h"

HP8751A::HP8751A(PrologixGPIB *gpib, quint16 gpibId, QObject *parent) : QObject(parent)
{
    this->gpib = gpib;
    this->gpibId = gpibId;
    QObject::connect(gpib, &PrologixGPIB::response, this, &HP8751A::gpib_response);
    respTimer = new QTimer(this);
    respTimer->setInterval(5000); // 5 second response timeout
    respTimer->setSingleShot(true);
    QObject::connect(respTimer, &QTimer::timeout, this, &HP8751A::resp_timeout);
    lastCommand = __CMD_NONE;
    channel = -1;
}

void HP8751A::identify()
{
    query_command(CMD_IDENTIFY, "*IDN?");
}

void HP8751A::init_transferfunction()
{
    QString commands;
    commands.append("POWE -20;"); // Source power -20 dBm
    commands.append("STAR 10;"); // Start frequency 10 Hz
    commands.append("STOP 1MHZ;"); // Stop frequency 1 MHz
    commands.append("POIN 201;"); // 201 points per sweep
    commands.append("ATTIA20DB;"); // Attenuator channel A 20 dB
    commands.append("ATTIR20DB;"); // Attenuator channel R 20 dB
    commands.append("IFBW 20;"); // 20 Hz receiver bandwidth
    commands.append("LOGFREQ;"); // log sweep
    commands.append("DUACON;"); // activate dual channel
    commands.append("SPLDON;"); // activate split display
    commands.append("HOLD;"); // Stop sweep
    commands.append("CHAN1;"); // select channel 1
    commands.append("AR;"); // Select A/R function
    commands.append("FMT LOGM;"); // Select format: log magnitude
    commands.append("CHAN2;"); // select channel 2
    commands.append("AR;"); // Select A/R function
    commands.append("FMT PHAS"); // Select format: phase
    send_command(CMD_INIT_TRANSFERFUNCTION, commands);
}

void HP8751A::init_impedance()
{
    //TODO
}

void HP8751A::set_stimulus(quint32 fStart, quint32 fStop, quint16 points, qint16 power, bool clearPowerTrip)
{
    QString commands;
    commands.append(QString("STAR %1;").arg(fStart));
    commands.append(QString("STOP %1;").arg(fStop));
    commands.append(QString("POIN %1;").arg(points));
    commands.append(QString("POWE %1").arg(power));
    if (clearPowerTrip) {
        commands.append(";CLEPTRIP");
    }
    send_command(CMD_SET_STIMULUS, commands);
}

void HP8751A::set_receiver(bool attenR, bool attenA, ifbw_t ifbw)
{
    QString commands;
    if (attenR) {
        commands.append("ATTIR20DB;");
    } else {
        commands.append("ATTIR0DB;");
    }
    if (attenA) {
        commands.append("ATTIA20DB;");
    } else {
        commands.append("ATTIA0DB;");
    }
    switch (ifbw) {
        case IFBW_2HZ:
            commands.append(QString("IFBW 2"));
            break;
        case IFBW_20HZ:
            commands.append(QString("IFBW 20"));
            break;
        case IFBW_200HZ:
            commands.append(QString("IFBW 200"));
            break;
        case IFBW_1KHZ:
            commands.append(QString("IFBW 1000"));
            break;
        case IFBW_4KHZ:
            commands.append(QString("IFBW 4000"));
            break;
        case IFBW_AUTO:
            commands.append(QString("IFBWAUTO"));
            break;
    }
    send_command(CMD_SET_RECEIVER, commands);
}

void HP8751A::poll_hold()
{
    query_command(CMD_POLL_HOLD, "HOLD?");
}

void HP8751A::start_sweep(bool avgEn, quint16 averFact)
{
    QString commands;

    if (avgEn) {
        commands.append("CHAN1;AVERON;");
        commands.append(QString("AVERFACT %1;").arg(averFact));
        commands.append("CHAN2;AVERON;");
        commands.append(QString("AVERFACT %1;").arg(averFact));
        commands.append(QString("NUMG %1").arg(averFact));
    } else {
        commands.append("CHAN1;AVEROFF;CHAN2;AVEROFF;");
        commands.append("SING");
    }
    send_command(CMD_START_SWEEP, commands);
}

void HP8751A::cancel_sweep()
{
    send_command(CMD_CANCEL_SWEEP, "HOLD");
}

void HP8751A::fit_trace(quint8 channel)
{
    QString commands;
    if (channel == 0) {
        commands.append("CHAN1;");
        commands.append("AUTO");
    } else {
        commands.append("CHAN2;");
        commands.append("AUTO;");
        commands.append("CHAN1");
    }
    send_command(CMD_FIT_TRACE, commands);
    this->channel = channel;
}

void HP8751A::get_stimulus()
{
    QString commands;
    commands.append("OUTPSTIM?");
    query_command(CMD_GET_STIMULUS, commands);
}

void HP8751A::get_channel_data(quint8 channel)
{
    QString commands;
    if (channel > 1) {
        channel = 1;
    }
    commands.append(QString("CHAN%1;").arg(channel + 1));
    commands.append("OUTPFORM?");
    query_command(CMD_GET_CHANNEL_DATA, commands);
    this->channel = channel;
}

void HP8751A::set_phase_format(quint8 channel, bool unwrapPhase)
{
    QString commands;

    if (channel > 1) {
        channel = 1;
    }

    commands.append(QString("CHAN%1;").arg(channel));

    if (unwrapPhase) {
        commands.append("FMT EXPP");
    } else {
        commands.append("FMT PHAS");
    }

    send_command(CMD_SET_PHASE_FORMAT, commands);
    this->channel = channel;
}

void HP8751A::gpib_response(QString resp)
{
    static QString respMerge;
    respMerge.append(resp);
    if (!resp.contains("\n")) {
        return;
    }
    emit instrument_response(lastCommand, respMerge, channel);

    respMerge.clear();
    channel = -1;
    lastCommand = __CMD_NONE;
    respTimer->stop();
}

void HP8751A::resp_timeout()
{
    qDebug() << "TIMEOUT";
    channel = -1;
    lastCommand = __CMD_NONE;
    emit response_timeout();
}

void HP8751A::send_command(command_t cmd, QString cmdString)
{
    QString temp;
    temp = cmdString + ";*OPC?";
    query_command(cmd, temp);
}

void HP8751A::query_command(command_t cmd, QString cmdString)
{
    if (!gpib) {
        return;
    }

    gpib->send_command(gpibId, cmdString);
    lastCommand = cmd;
    //respTimer->start();
}
