#ifndef HP8751A_H
#define HP8751A_H

#include <QObject>
#include <prologixgpib.h>
#include <QTimer>
#include <QVector>
#include <QStateMachine>
#include <QState>

class HP8751A : public QObject
{
    Q_OBJECT
public:
    explicit HP8751A(PrologixGPIB *gpib, quint16 gpibId, QObject *parent = nullptr);

    enum ifbw_t {
        IFBW_2HZ,
        IFBW_20HZ,
        IFBW_200HZ,
        IFBW_1KHZ,
        IFBW_4KHZ,
        IFBW_AUTO
    };

    enum input_port_t {
        PORT_AR,
        PORT_BR,
        PORT_AB,
        PORT_A,
        PORT_B,
        PORT_R,
        PORT_S11_AR,
        PORT_S21_BR,
        PORT_S12_AR,
        PORT_S22_BR
    };

    enum conversion_t {
        CONV_OFF,
        CONV_Z_REFL,
        CONV_Z_TRANS,
        CONV_Y_REFL,
        CONV_Y_TRANS
    };

    struct instrument_parameters_t {
        quint32 fStart; // Start frequency
        quint32 fStop; // Stop frequency
        quint16 points; // Number of points
        qint16 power; // Source power
        bool clearPowerTrip; // Clear power trip
        bool attenR; // Attenuater input R; true = 20 dB, false = 0 dB
        bool attenA; // Attenuater input A; true = 20 dB, false = 0 dB
        HP8751A::ifbw_t ifbw; // Receiver bandwidth
        bool unwrapPhase; // Phase is always channel 2
        bool avgEn; // Enable averaging
        quint16 averFact; // Averaging factor
    };

    struct instrument_data_t {
        QVector<float> stimulus;
        QVector<float> channel1;
        QVector<float> channel2;
        float channel1Scale;
        float channel1Refpos;
        float channel2Scale;
        float channel2Refpos;
    };

    // Identify the HP 8751A on the bus
    void identify();

    // Init basic measurement functions
    void init_function(input_port_t portCh1, conversion_t convCh1, input_port_t portCh2, conversion_t convCh2);

    void set_instrument_parameters(instrument_parameters_t param);

    // Start single sweep
    void request_sweep();

    // Cancel current sweep
    void request_cancel();

    // Request if instrument is currently sweeping
    bool sweep_done();

    // Get stimulus and channel data from local buffer
    void get_data(HP8751A::instrument_data_t &data);

private:
    PrologixGPIB *gpib = nullptr;
    quint16 gpibId;
    void gpib_response(QByteArray resp);
    QTimer *respTimer = nullptr;
    void resp_timeout();
    void send_command(QString cmdString); // Appends *OPC? to the command list
    void query_command(QString cmdString);

    QTimer *sendCmdTimer = nullptr;
    void send_timer_timeout();

    void init_statemachine_sweep();

    void start_sweep();
    void cancel_sweep();

    void poll_hold();

    void fit_trace();
    void get_stimulus();
    void get_channel_data(quint8 channel);

    instrument_parameters_t params;
    instrument_data_t data;

    void unpack_stimulus(const QByteArray &resp);
    void unpack_channel(const QByteArray &resp, quint8 channel);

    bool sweepDone;

    enum cmd_type_t {
        CMD_TYPE_COMMAND,
        CMD_TYPE_QUERY
    };

    enum command_t {
        CMD_IDENTIFY,
        CMD_INIT_FUNCTION,
        CMD_SET_PARAMETERS,
        CMD_START_SWEEP,
        CMD_CANCEL_SWEEP,
        CMD_POLL_HOLD,
        CMD_FIT_TRACE,
        CMD_GET_STIMULUS,
        CMD_GET_DATA
    };

    struct cmd_queue_t {
        command_t cmd;
        QString cmdString;
        qint8 channel;
        cmd_type_t type;
    };

    QString port_to_string(input_port_t port);
    QString conversion_to_string(conversion_t conv);
    QString ifbw_to_string(ifbw_t ifbw);

    void enqueue_cmd(command_t cmd, QString cmdString, qint8 channel, cmd_type_t type);
    bool nextCmd;

    void instrument_response(command_t cmd, QByteArray resp, qint8 channel); // Channel parameter contains 0 or 1 for a channel specific command, -1 otherwise

    QVector<cmd_queue_t> cmdQueue;

signals:
    // Public signals
    void instrument_identification(QString);
    void instrument_initialized();
    void set_parameters_finished();
    void retrieving_data();
    void new_data(HP8751A::instrument_data_t);
    void sweep_cancelled();
    void response_timeout();

    // Private signals
    void responseOK(QPrivateSignal);
    void responseNOK(QPrivateSignal);
    void sig_start_sweep(QPrivateSignal);
    void sig_cancel_sweep(QPrivateSignal);

};

#endif // HP8751A_H
