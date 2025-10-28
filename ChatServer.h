#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>

// 存储在线用户信息
struct OnlineUser {
    int id;
    QString username;
    QString role;
    QTcpSocket* socket;
};

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
    // 存储用户ID与对应的Socket和用户信息
    QMap<int, OnlineUser> m_onlineUsers;

    // 解析客户端消息
    bool parseMessage(const QByteArray &data, QJsonObject &obj);
    // 广播在线用户列表
    void broadcastOnlineUsers();
    // 根据用户ID查找在线用户
    OnlineUser* findUserById(int userId);
};

#endif // CHATSERVER_H
