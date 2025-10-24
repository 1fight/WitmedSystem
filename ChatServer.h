#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>

// 聊天服务器：管理客户端连接，转发医患消息（仅支持文字，不存储记录）
class ChatServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);

    // 启动服务器（指定端口，简易版固定用8888）
    bool startServer(quint16 port = 8888);

private slots:
    // 新客户端连接
    void onNewConnection();
    // 客户端断开连接
    void onDisconnected();
    // 接收客户端消息并转发
    void onReadyRead();

private:
    // 存储用户ID与对应的Socket（key: 用户ID，value: 连接的Socket）
    QMap<int, QTcpSocket*> m_clientMap;

    // 解析客户端消息（JSON格式：{senderId: x, targetId: y, content: "消息内容"}）
    bool parseMessage(const QByteArray &data, int &senderId, int &targetId, QString &content);
};

#endif // CHATSERVER_H
