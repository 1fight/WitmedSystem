
#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>

class ChatServer : public QTcpServer {
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    void startServer(quint16 port = 8888);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QMap<int, QTcpSocket*> m_userSockets; // 用户ID -> 对应的Socket
    QMap<QTcpSocket*, int> m_socketUsers; // Socket -> 用户ID（反向映射）
};

#endif // CHATSERVER_H
