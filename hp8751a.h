#ifndef HP8751A_H
#define HP8751A_H

#include <QObject>
#include <prologixgpib.h>
#include <QTimer>

class HP8751A : public QObject
{
    Q_OBJECT
public:
    explicit HP8751A(PrologixGPIB *gpib, quint16 gpibId, QObject *parent = nullptr);

    enum command_t {
        __CMD_NONE,
        CMD_IDENTIFY,
        CMD_INIT_TRANSFERFUNCTION,
        CMD_INIT_IMPEDANCE,
        CMD_SET_STIMULUS,
        CMD_SET_RECEIVER,
        CMD_POLL_HOLD,
        CMD_START_SWEEP,
        CMD_CANCEL_SWEEP,
        CMD_FIT_TRACE,
        CMD_GET_STIMULUS,
        CMD_GET_CHANNEL_DATA,
        CMD_SET_PHASE_FORMAT
    };

    enum ifbw_t {
        IFBW_2HZ,
        IFBW_20HZ,
        IFBW_200HZ,
        IFBW_1KHZ,
        IFBW_4KHZ,
        IFBW_AUTO
    };

    // Identify the HP 8751A on the bus
    void identify();

    // Init instrument for loopgain measurement
    void init_transferfunction();

    // Init instrument for impedance measurement
    void init_impedance();

    // Set stimulus parameter
    void set_stimulus(quint32 fStart, // Start frequency
                      quint32 fStop, // Stop frequency
                      quint16 points, // Number of points
                      qint16 power, // Source power
                      bool clearPowerTrip = true); // Clear power trip

    // Set receiver parameter
    void set_receiver(bool attenR, // Attenuater input R; true = 20 dB, false = 0 dB
                      bool attenA, // Attenuater input A; true = 20 dB, false = 0 dB
                      HP8751A::ifbw_t ifbw); // Receiver bandwidth

    // Poll sweep status
    void poll_hold();

    // Start sweep
    void start_sweep(bool avgEn, // enable averaging
                     quint16 averFact); // averaging factor

    // Cancel current sweep
    void cancel_sweep();

    // Auto scale trace of selected channel
    void fit_trace(quint8 channel);

    // Get stimulus frequencies from instrument
    void get_stimulus();

    // Get channel data of selected channel from instrument
    void get_channel_data(quint8 channel);

    // Set phase format of selected channel
    void set_phase_format(quint8 channel, bool unwrapPhase);

private:
    PrologixGPIB *gpib = nullptr;
    quint16 gpibId;
    command_t lastCommand;
    void gpib_response(QString resp);
    QTimer *respTimer = nullptr;
    void resp_timeout();
    void send_command(command_t cmd, QString cmdString); // Appends *OPC? to the command list
    void query_command(command_t cmd, QString cmdString);
    qint8 channel;

signals:
    void instrument_response(command_t cmd, QString resp, qint8 channel); // Channel parameter contains 0 or 1 for a channel specific command, -1 otherwise
    void response_timeout();

};

#endif // HP8751A_H
