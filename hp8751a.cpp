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
    nextCmd = true;

    sendCmdTimer = new QTimer(this);
    sendCmdTimer->setInterval(1);
    QObject::connect(sendCmdTimer, &QTimer::timeout, this, &HP8751A::send_timer_timeout);

}

void HP8751A::identify()
{
    enqueue_cmd(CMD_IDENTIFY, "*IDN?", -1, CMD_TYPE_QUERY);
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
    enqueue_cmd(CMD_INIT_TRANSFERFUNCTION, commands, -1, CMD_TYPE_COMMAND);
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
    enqueue_cmd(CMD_SET_STIMULUS, commands, -1, CMD_TYPE_COMMAND);
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
    enqueue_cmd(CMD_SET_RECEIVER, commands, -1, CMD_TYPE_COMMAND);
}

void HP8751A::poll_hold()
{
    enqueue_cmd(CMD_POLL_HOLD, "HOLD?", -1, CMD_TYPE_QUERY);
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
    enqueue_cmd(CMD_START_SWEEP, commands, -1, CMD_TYPE_COMMAND);
}

void HP8751A::cancel_sweep()
{
    enqueue_cmd(CMD_CANCEL_SWEEP, "HOLD", -1, CMD_TYPE_COMMAND);
}

void HP8751A::fit_trace(quint8 channel)
{
    QString commands;
    if (channel == 0) {
        commands.append("CHAN1;");
        commands.append("AUTO;");
        commands.append("SCAL?;");
        commands.append("REFV?");
    } else {
        commands.append("CHAN2;");
        commands.append("AUTO;");
        commands.append("SCAL?;");
        commands.append("REFV?;");
        commands.append("CHAN1");
    }
    enqueue_cmd(CMD_FIT_TRACE, commands, (qint8)channel, CMD_TYPE_QUERY);
}

void HP8751A::get_stimulus()
{
    QString commands;
    commands.append("OUTPSTIM?");
    enqueue_cmd(CMD_GET_STIMULUS, commands, -1, CMD_TYPE_QUERY);
}

void HP8751A::get_channel_data(quint8 channel)
{
    QString commands;
    if (channel > 1) {
        channel = 1;
    }
    commands.append(QString("CHAN%1;").arg(channel + 1));
    commands.append("OUTPFORM?");
    enqueue_cmd(CMD_GET_CHANNEL_DATA, commands, (qint8)channel, CMD_TYPE_QUERY);
}

void HP8751A::set_phase_format(quint8 channel, bool unwrapPhase)
{
    QString commands;

    if (channel > 1) {
        channel = 1;
    }

    commands.append(QString("CHAN%1;").arg(channel + 1));

    if (unwrapPhase) {
        commands.append("FMT EXPP");
    } else {
        commands.append("FMT PHAS");
    }

    enqueue_cmd(CMD_SET_PHASE_FORMAT, commands, (qint8)channel, CMD_TYPE_COMMAND);
}

void HP8751A::gpib_response(QString resp)
{
    static QString respMerge;
    respMerge.append(resp);
    if (!resp.contains("\n")) {
        return;
    }
    respMerge.remove("\n");
    cmd_queue_t cmd = cmdQueue.takeFirst();
    cmdQueue.squeeze();
    emit instrument_response(cmd.cmd, respMerge, cmd.channel);
    nextCmd = true;

    respMerge.clear();
    respTimer->stop();
}

void HP8751A::resp_timeout()
{
    qDebug() << "TIMEOUT";

    emit response_timeout();
    cmdQueue.pop_front();
    nextCmd = true;
}

void HP8751A::send_command(QString cmdString)
{
    QString temp;
    temp = cmdString + ";*OPC?";
    query_command(temp);
}

void HP8751A::query_command(QString cmdString)
{
    if (!gpib) {
        return;
    }

    gpib->send_command(gpibId, cmdString);
}

void HP8751A::send_timer_timeout()
{
    /* 1) Queue is empty, nextCmd = true: Last CMD has been sent. Nothing to do here
     * 2) Queue is empty, nextCmd = false: Last command in Queue has been sent. Waiting for response. Nothing to do here
     * 3) Queue is not empty, nextCmd = true: Send next command; clear nextCmd and wait for response
     * 4) Queue is not empty, nextCmd = false: Current cmd pending. Waiting for response. Nothing to do here
     */

    if (!cmdQueue.isEmpty() && nextCmd) {
        cmd_queue_t next = cmdQueue.first();
        if (next.type == CMD_TYPE_COMMAND) {
            send_command(next.cmdString);
        } else {
            query_command(next.cmdString);
        }
        nextCmd = false;
        respTimer->start();
    }
}

void HP8751A::enqueue_cmd(command_t cmd, QString cmdString, qint8 channel, cmd_type_t type)
{
    cmdQueue.push_back({cmd, cmdString, channel, type});
    sendCmdTimer->start();
}
