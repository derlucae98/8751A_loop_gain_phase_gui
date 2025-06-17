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

    init_statemachine_sweep();
}

void HP8751A::identify()
{
    enqueue_cmd(CMD_IDENTIFY, "*IDN?", -1, CMD_TYPE_QUERY);
}

void HP8751A::init_function(input_port_t portCh1, conversion_t convCh1, format_t fmtCh1, input_port_t portCh2, conversion_t convCh2, format_t fmtCh2)
{
    QString commands;
    commands.append("LOGFREQ;"); // log sweep
    commands.append("DUACON;"); // activate dual channel
    commands.append("SPLDON;"); // activate split display
    commands.append("HOLD;"); // Stop sweep
    commands.append("CHAN1;"); // select channel 1
    commands.append(port_to_string(portCh1) + ";"); // Select meas function
    commands.append(conversion_to_string(convCh1) + ";"); // Select conversion
    commands.append(format_to_string(fmtCh1) + ";"); // Select format
    commands.append("CHAN2;"); // select channel 2
    commands.append(port_to_string(portCh2) + ";"); // Select meas function
    commands.append(conversion_to_string(convCh2) + ";"); // Select conversion
    commands.append(format_to_string(fmtCh2)); // Select format
    enqueue_cmd(CMD_INIT_FUNCTION, commands, -1, CMD_TYPE_COMMAND);
}

void HP8751A::set_instrument_parameters(instrument_parameters_t param)
{
    this->params = param;
    QString commands;
    commands.append(QString("STAR %1;").arg(param.fStart));
    commands.append(QString("STOP %1;").arg(param.fStop));
    commands.append(QString("POIN %1;").arg(param.points));
    commands.append(QString("POWE %1;").arg(param.power));
    if (param.clearPowerTrip) {
        commands.append("CLEPTRIP;");
    }
    if (param.attenR) {
        commands.append("ATTIR20DB;");
    } else {
        commands.append("ATTIR0DB;");
    }
    if (param.attenA) {
        commands.append("ATTIA20DB;");
    } else {
        commands.append("ATTIA0DB;");
    }
    commands.append(ifbw_to_string(params.ifbw) + ";");

    commands.append("CHAN2;");

    if (param.unwrapPhase) {
        commands.append(format_to_string(FMT_EXPP) + ";");
    } else {
        commands.append(format_to_string(FMT_PHAS) + ";");
    }

    if (param.avgEn) {
        commands.append("CHAN1;AVERON;");
        commands.append(QString("AVERFACT %1;").arg(param.averFact));
        commands.append("CHAN2;AVERON;");
        commands.append(QString("AVERFACT %1").arg(param.averFact));
    } else {
        commands.append("CHAN1;AVEROFF;CHAN2;AVEROFF");
    }

    enqueue_cmd(CMD_SET_PARAMETERS, commands, -1, CMD_TYPE_COMMAND);
}

void HP8751A::request_sweep()
{
    QTimer::singleShot(0, this, [=] {
        emit sig_start_sweep(QPrivateSignal());
    });
}

void HP8751A::request_cancel()
{
    QTimer::singleShot(0, this, [=] {
        emit sig_cancel_sweep(QPrivateSignal());
    });
}

bool HP8751A::sweep_done()
{
    return sweepDone;
}

void HP8751A::get_data(instrument_data_t &data)
{
    data = this->data;
}

void HP8751A::start_sweep()
{
    QString commands;
    if (this->params.avgEn) {
        commands.append(QString("NUMG %1").arg(this->params.averFact));
    } else {
        commands.append("SING");
    }

    enqueue_cmd(CMD_START_SWEEP, commands, -1, CMD_TYPE_COMMAND);
}

void HP8751A::cancel_sweep()
{
    enqueue_cmd(CMD_CANCEL_SWEEP, "HOLD", -1, CMD_TYPE_COMMAND);
}

void HP8751A::poll_hold()
{
    enqueue_cmd(CMD_POLL_HOLD, "HOLD?", -1, CMD_TYPE_QUERY);
}

void HP8751A::fit_trace()
{
    QString commands;
    commands.append("CHAN1;");
    commands.append("AUTO;");
    commands.append("SCAL?;");
    commands.append("REFV?;");
    commands.append("REFP 5;");
    commands.append("CHAN2;");
    commands.append("AUTO;");
    commands.append("SCAL?;");
    commands.append("REFV?;");
    commands.append("REFP 5;");
    commands.append("CHAN1");

    enqueue_cmd(CMD_FIT_TRACE, commands, -1, CMD_TYPE_QUERY);
}

void HP8751A::get_stimulus()
{
    QString commands;
    commands.append("FORM5;OUTPSTIM?");
    enqueue_cmd(CMD_GET_STIMULUS, commands, -1, CMD_TYPE_QUERY);
}

void HP8751A::get_channel_data(quint8 channel)
{
    QString commands;
    if (channel > 1) {
        channel = 1;
    }
    commands.append(QString("CHAN%1;").arg(channel + 1));
    commands.append("FORM5;OUTPFORM?");
    enqueue_cmd(CMD_GET_DATA, commands, (qint8)channel, CMD_TYPE_QUERY);
}

void HP8751A::unpack_stimulus(const QByteArray &resp)
{
    data.stimulus.clear();

    for (unsigned int i = 0; i < resp.size() / sizeof(float); i++) {
        float frequency = *(reinterpret_cast<float*>(resp.mid(4 * i, 4).data()));
        data.stimulus.push_back(frequency);
    }
}

void HP8751A::unpack_channel(const QByteArray &resp, quint8 channel)
{
    float channelData;

    if (channel == 0) {
        data.channel1.clear();
    } else {
        data.channel2.clear();
    }

    for (unsigned int i = 0; i < resp.size() / (2 * sizeof(float)); i++) {
        channelData = *(reinterpret_cast<float*>(resp.mid(8 * i, 4).data()));
        if (channel == 0) {
            data.channel1.push_back(channelData);
        } else {
            data.channel2.push_back(channelData);
        }
    }
}

QString HP8751A::port_to_string(input_port_t port)
{
    switch (port) {
    case PORT_AR:
        return "AR";
    case PORT_BR:
        return "BR";
    case PORT_AB:
        return "AB";
    case PORT_A:
        return "MEASA";
    case PORT_B:
        return "MEASB";
    case PORT_R:
        return "MEASR";
    case PORT_S11_AR:
        return "S11";
    case PORT_S21_BR:
        return "S21";
    case PORT_S12_AR:
        return "S12";
    case PORT_S22_BR:
        return "S22";
    }
}

QString HP8751A::conversion_to_string(conversion_t conv)
{
    switch (conv) {
    case CONV_OFF:
        return "CONVOFF";
    case CONV_Z_REFL:
        return "CONVZREF";
    case CONV_Z_TRANS:
        return "CONVZTRA";
    case CONV_Y_REFL:
        return "CONVYREF";
    case CONV_Y_TRANS:
        return "CONVYTRA";
    }
}

QString HP8751A::ifbw_to_string(ifbw_t ifbw)
{
    switch (ifbw) {
    case IFBW_2HZ:
        return "IFBW 2";
    case IFBW_20HZ:
        return "IFBW 20";
    case IFBW_200HZ:
        return "IFBW 200";
    case IFBW_1KHZ:
        return "IFBW 1000";
    case IFBW_4KHZ:
        return "IFBW 4000";
    case IFBW_AUTO:
        return "IFBWAUTO";
    }
}

QString HP8751A::format_to_string(format_t fmt)
{
    switch (fmt) {
    case FMT_LOGM:
        return "FMT LOGM";
    case FMT_PHAS:
        return "FMT PHAS";
    case FMT_DELA:
        return "FMT DELA";
    case FMT_SMIC:
        return "FMT SMIC";
    case FMT_POLA:
        return "FMT POLA";
    case FMT_LINM:
        return "FMT LINM";
    case FMT_SWR:
        return "FMT SWR";
    case FMT_REAL:
        return "FMT REAL";
    case FMT_IMAG:
        return "FMT IMAG";
    case FMT_EXPP:
        return "FMT EXPP";
    case FMT_INVSCHAR:
        return "FMT INVSCHAR";
    case FMT_LOGMP:
        return "FMT LOGMP";
    case FMT_LOGMD:
        return "FMT LOGMD";
    }
}

void HP8751A::gpib_response(QByteArray resp)
{
    static quint32 numberOfBytes;
    static quint32 bytesReceived;
    static QByteArray respMerge;

    if (cmdQueue.isEmpty()) {
        return;
    }

    if (cmdQueue.first().cmd == CMD_GET_DATA ||
        cmdQueue.first().cmd == CMD_GET_STIMULUS) {

        bytesReceived += resp.size();

        respMerge.append(resp);

        // When receiving data in float mode, the number of following bytes is transmitted.
        // Sequence starts with "#6";
        if (resp.mid(0, 2) == "#6") {
            numberOfBytes = resp.mid(2, 6).toUInt();
            bytesReceived -= 9; // Don't count "#6", 6 byte length and \n
            respMerge.remove(0, 8); // Remove the first block which tells the number of bytes
        }

        if (bytesReceived != numberOfBytes) {
            return;
        }

    } else if (cmdQueue.first().cmd == CMD_FIT_TRACE) {
        respMerge.append(resp);
        // After the first trace is auto scaled and the scale and ref position are transmitted,
        // we have to wait for the second channel to be scaled and the values to be transmitted
        if (resp.back() != '\n') {
            return;
        }
    } else {
        respMerge = resp;
    }

    bytesReceived = 0;
    numberOfBytes = 0;
    if (respMerge.back() == '\n') {
        respMerge.chop(1);
    }

    cmd_queue_t cmd = cmdQueue.takeFirst();
    cmdQueue.squeeze();
    instrument_response(cmd.cmd, respMerge, cmd.channel);
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
    /* 1) Queue is empty, nextCmd = true: Last command has been sent. Nothing to do here
     * 2) Queue is empty, nextCmd = false: Last command in Queue has been sent. Waiting for response. Nothing to do here
     * 3) Queue is not empty, nextCmd = true: Send next command; clear nextCmd and wait for response
     * 4) Queue is not empty, nextCmd = false: Current command pending. Waiting for response. Nothing to do here
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

void HP8751A::init_statemachine_sweep()
{
    QStateMachine *smSweep = new QStateMachine(this);
    QState *sIdle = new QState();
    QState *sStartSweep = new QState();
    QState *sPollHold = new QState();
    QState *sFitTrace = new QState();
    QState *sGetStimulus = new QState();
    QState *sGetTrace1 = new QState();
    QState *sGetTrace2 = new QState();
    QState *sHold = new QState();
    QState *sStop = new QState();

    QObject::connect(sIdle, &QState::entered, this, [=] {sweepDone = true;});
    QObject::connect(sIdle, &QState::exited, this, [=] {sweepDone = false;});
    sIdle->addTransition(this, &HP8751A::sig_start_sweep, sStartSweep);

    QObject::connect(sStartSweep, &QState::entered, this, &HP8751A::start_sweep);
    sStartSweep->addTransition(this, &HP8751A::responseOK, sPollHold);
    sStartSweep->addTransition(this, &HP8751A::sig_cancel_sweep, sHold);

    QObject::connect(sPollHold, &QState::entered, this, &HP8751A::poll_hold);
    sPollHold->addTransition(this, &HP8751A::responseOK, sFitTrace);
    sPollHold->addTransition(this, &HP8751A::responseNOK, sPollHold);
    sPollHold->addTransition(this, &HP8751A::sig_cancel_sweep, sHold);

    QObject::connect(sFitTrace, &QState::entered, this, &HP8751A::fit_trace);
    QObject::connect(sFitTrace, &QState::entered, this, &HP8751A::retrieving_data);
    sFitTrace->addTransition(this, &HP8751A::responseOK, sGetStimulus);
    sFitTrace->addTransition(this, &HP8751A::sig_cancel_sweep, sHold);

    QObject::connect(sGetStimulus, &QState::entered, this, &HP8751A::get_stimulus);
    sGetStimulus->addTransition(this, &HP8751A::responseOK, sGetTrace1);
    sGetStimulus->addTransition(this, &HP8751A::sig_cancel_sweep, sHold);

    QObject::connect(sGetTrace1, &QState::entered, this, [=] {get_channel_data(0);});
    sGetTrace1->addTransition(this, &HP8751A::responseOK, sGetTrace2);
    sGetTrace1->addTransition(this, &HP8751A::sig_cancel_sweep, sHold);

    QObject::connect(sGetTrace2, &QState::entered, this, [=] {get_channel_data(1);});
    sGetTrace2->addTransition(this, &HP8751A::responseOK, sStop);
    sGetTrace2->addTransition(this, &HP8751A::sig_cancel_sweep, sHold);

    QObject::connect(sHold, &QState::entered, this, &HP8751A::cancel_sweep);
    QObject::connect(sHold, &QState::exited, this, &HP8751A::sweep_cancelled);
    sHold->addTransition(this, &HP8751A::responseOK, sIdle);

    QObject::connect(sStop, &QState::entered, this, [=] {emit new_data(this->data);});
    sStop->addTransition(sStop, &QState::entered, sIdle);

    smSweep->addState(sIdle);
    smSweep->addState(sStartSweep);
    smSweep->addState(sPollHold);
    smSweep->addState(sFitTrace);
    smSweep->addState(sGetStimulus);
    smSweep->addState(sGetTrace1);
    smSweep->addState(sGetTrace2);
    smSweep->addState(sHold);
    smSweep->addState(sStop);
    smSweep->setInitialState(sIdle);
    smSweep->start();
}

void HP8751A::enqueue_cmd(command_t cmd, QString cmdString, qint8 channel, cmd_type_t type)
{
    cmdQueue.push_back({cmd, cmdString, channel, type});
    sendCmdTimer->start();
}

void HP8751A::instrument_response(command_t cmd, QByteArray resp, qint8 channel)
{
    switch (cmd) {
    case CMD_IDENTIFY:
        emit instrument_identification(QString(resp));
        break;

    case CMD_INIT_FUNCTION:
        emit instrument_initialized();
        break;

    case CMD_SET_PARAMETERS:
        emit set_parameters_finished();
        break;

    case CMD_START_SWEEP:
    case CMD_CANCEL_SWEEP:
        emit responseOK(QPrivateSignal());
        break;

    case CMD_POLL_HOLD:
        if (resp == "0") {
            emit responseNOK(QPrivateSignal());
        } else {
            emit responseOK(QPrivateSignal());
        }
        break;

    case HP8751A::CMD_FIT_TRACE:
        {
            QString respStr = resp;
            QStringList respSeparate = respStr.split(";");
            data.channel1Scale = respSeparate.at(0).toFloat();
            data.channel1RefVal = respSeparate.at(1).toFloat();
            data.channel2Scale = respSeparate.at(2).toFloat();
            data.channel2RefVal = respSeparate.at(3).toFloat();
            emit responseOK(QPrivateSignal());
            break;
        }

    case HP8751A::CMD_GET_STIMULUS:
        unpack_stimulus(resp);
        emit responseOK(QPrivateSignal());
        break;

    case HP8751A::CMD_GET_DATA:
        unpack_channel(resp, channel);
        emit responseOK(QPrivateSignal());
        break;

    default:
        break;
    }
}
