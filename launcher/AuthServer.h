#pragma once

#include <QString>
#include <QTcpServer>
#include <memory>

class AuthServer: public QObject
{
    Q_OBJECT
public:
    explicit AuthServer(QObject *parent = 0);

    quint16 port();

private slots:
    void newConnection();

private:
    std::shared_ptr<QTcpServer> m_tcpServer;
};
