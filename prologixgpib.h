#ifndef PROLOGIXGPIB_H
#define PROLOGIXGPIB_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>

class PrologixGPIB : public QObject
{
    Q_OBJECT
public:
    explicit PrologixGPIB(QObject *parent = nullptr);
    void init(QHostAddress &ip, quint16 port);
    void deinit();
    void send_command(quint16 gpibAddr, const QString command);

private:
    QTcpSocket *socket = nullptr;
    void read_socket();
    void socket_connected();
signals:
    void connected();
    void disconnected();
    void stateChanged(QAbstractSocket::SocketState);
    void response(QByteArray);

};

#endif // PROLOGIXGPIB_H
