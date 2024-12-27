#include "prologixgpib.h"

PrologixGPIB::PrologixGPIB(QObject *parent) : QObject(parent)
{
    socket = new QTcpSocket;

    QObject::connect(socket, &QTcpSocket::connected, this, &PrologixGPIB::connected); //Forwarding signal
    QObject::connect(socket, &QTcpSocket::disconnected, this, &PrologixGPIB::disconnected); //Forwarding signal
    QObject::connect(socket, &QTcpSocket::stateChanged, this, &PrologixGPIB::stateChanged); //Forwarding signal
    QObject::connect(socket, &QTcpSocket::connected, this, &PrologixGPIB::socket_connected);
    QObject::connect(socket, &QTcpSocket::readyRead, this, &PrologixGPIB::read_socket);
}

void PrologixGPIB::init(QHostAddress &ip, quint16 port)
{
    if (!socket) {
        return;
    }
    socket->connectToHost(ip, port, QIODevice::ReadWrite);
}

void PrologixGPIB::deinit()
{
    if (!socket) {
        return;
    }
    if (!socket->isOpen()) {
        return;
    }
    socket->disconnectFromHost();
}

void PrologixGPIB::send_command(quint16 gpibAddr, const QString command)
{
    if (!socket) {
        return;
    }
    if (!socket->isOpen()) {
        return;
    }
    QString addr = QString("++addr %1\r").arg(gpibAddr);
    socket->write(addr.toLocal8Bit());

    QString cmd = command + QStringLiteral("\r\n");
    socket->write(cmd.toLocal8Bit());
}

void PrologixGPIB::socket_connected()
{
    //Send init commands to Prologix GPIB-Ethernet adapter
    //socket->write("++ver\r");
    socket->write("++mode 1\r");
    socket->write("++auto 1\r");
    socket->write("++eoi 1\r");
    socket->write("++eos 3\r");
    socket->write("++eot_enable 0\r");
    socket->write("++ifc\r");
}

void PrologixGPIB::read_socket()
{
    if (!socket) {
        return;
    }
    if (!socket->isOpen()) {
        return;
    }
    QString resp;
    while (socket->bytesAvailable()) {
        resp = socket->readAll();
    }
    emit response(resp);
}


