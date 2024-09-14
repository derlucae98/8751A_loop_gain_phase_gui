#ifndef PROLOGIXGPIB_H
#define PROLOGIXGPIB_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>

const quint16 PROLOGIX_PORT = 1234;

class PrologixGPIB : public QObject
{
    Q_OBJECT
public:
    explicit PrologixGPIB(QObject *parent = nullptr);
    void init(QHostAddress &ip);
    void deinit();
    void send_command(quint16 gpibAddr, const QString command);

private:
    QTcpSocket *socket = nullptr;
    void read_socket();
    void socket_connected();
signals:
    void connected();
    void disconnected();
    void stateChanged();
    void response(QString);

};

#endif // PROLOGIXGPIB_H
