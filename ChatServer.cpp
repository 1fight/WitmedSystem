#include "chatserver.h"
#include <QDebug>

ChatServer::ChatServer(QObject *parent) : QTcpServer(parent)
{
    // 连接新客户端信号
    connect(this, &ChatServer::newConnection, this, &ChatServer::onNewConnection);
}

// 启动服务器（返回是否启动成功）
bool ChatServer::startServer(quint16 port)
{
    if (this->listen(QHostAddress::Any, port)) { // 监听所有IP的指定端口
        qDebug() << "聊天服务器启动成功，端口：" << port;
        return true;
    } else {
        qDebug() << "聊天服务器启动失败：" << this->errorString();
        return false;
    }
}

// 处理新客户端连接
void ChatServer::onNewConnection()
{
    QTcpSocket *clientSocket = this->nextPendingConnection();
    if (!clientSocket) return;

    qDebug() << "新客户端连接，IP：" << clientSocket->peerAddress().toString();

    // 连接客户端断开信号
    connect(clientSocket, &QTcpSocket::disconnected, this, &ChatServer::onDisconnected);
    // 连接接收消息信号
    connect(clientSocket, &QTcpSocket::readyRead, this, &ChatServer::onReadyRead);
}

// 处理客户端断开连接
void ChatServer::onDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    // 从在线用户列表中移除断开的客户端
    auto it = m_onlineUsers.begin();
    while (it != m_onlineUsers.end()) {
        if (it.value().socket == clientSocket) {
            qDebug() << "用户ID：" << it.key() << "断开连接";
            it = m_onlineUsers.erase(it);
            // 广播在线用户列表更新
            broadcastOnlineUsers();
        } else {
            ++it;
        }
    }

    clientSocket->deleteLater();
}

// 接收消息并转发
void ChatServer::onReadyRead()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    // 读取客户端发送的所有数据
    QByteArray data = clientSocket->readAll();
    QStringList msgs = QString(data).split('\n');
    
    foreach (const QString &msgStr, msgs) {
        if (msgStr.isEmpty()) continue;
        
        QJsonObject msgObj;
        if (!parseMessage(msgStr.toUtf8(), msgObj)) {
            qDebug() << "消息格式错误，忽略：" << msgStr;
            continue;
        }
        
        QString type = msgObj["type"].toString();
        
        if (type == "online_status") {
            // 处理在线状态更新
            int userId = msgObj["user_id"].toInt();
            QString status = msgObj["status"].toString();
            
            if (status == "online") {
                // 用户上线
                OnlineUser user;
                user.id = userId;
                user.username = msgObj["username"].toString();
                user.role = msgObj["role"].toString();
                user.socket = clientSocket;
                m_onlineUsers[userId] = user;
                qDebug() << "用户上线：" << userId << "(" << user.username << ")";
            } else if (status == "offline") {
                // 用户下线
                m_onlineUsers.remove(userId);
                qDebug() << "用户下线：" << userId;
            }
            
            // 广播在线用户列表
            broadcastOnlineUsers();
        }
        else if (type == "chat_request") {
            // 转发聊天请求
            int fromUserId = msgObj["from"].toInt();
            int toUserId = msgObj["to"].toInt();
            
            OnlineUser* targetUser = findUserById(toUserId);
            if (targetUser) {
                targetUser->socket->write(msgStr.toUtf8() + "\n");
                qDebug() << "转发聊天请求：" << fromUserId << "→" << toUserId;
            }
        }
        else if (type == "chat_response") {
            // 转发聊天响应
            int toUserId = msgObj["to"].toInt();
            OnlineUser* targetUser = findUserById(toUserId);
            if (targetUser) {
                targetUser->socket->write(msgStr.toUtf8() + "\n");
                qDebug() << "转发聊天响应：" << msgObj["from"].toInt() << "→" << toUserId;
            }
        }
        else if (type == "chat") {
            // 处理普通聊天消息（现有功能）
            int fromUserId = msgObj["from"].toInt();
            int toUserId = msgObj["to"].toInt();
            
            OnlineUser* targetUser = findUserById(toUserId);
            if (targetUser) {
                targetUser->socket->write(msgStr.toUtf8() + "\n");
                qDebug() << "转发聊天消息：" << fromUserId << "→" << toUserId;
            } else {
                // 目标用户不在线
                QJsonObject reply {
                    {"type", "error"},
                    {"message", "对方不在线，无法发送消息"}
                };
                clientSocket->write(QJsonDocument(reply).toJson() + "\n");
            }
        }
    }
}

// 解析客户端消息
bool ChatServer::parseMessage(const QByteArray &data, QJsonObject &obj)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;
    
    obj = doc.object();
    return true;
}

// 广播在线用户列表
void ChatServer::broadcastOnlineUsers()
{
    QJsonArray usersArray;
    foreach (const OnlineUser &user, m_onlineUsers.values()) {
        QJsonObject userObj;
        userObj["id"] = user.id;
        userObj["username"] = user.username;
        userObj["role"] = user.role;
        usersArray.append(userObj);
    }
    
    QJsonObject onlineUsersMsg;
    onlineUsersMsg["type"] = "online_users";
    onlineUsersMsg["users"] = usersArray;
    
    QByteArray data = QJsonDocument(onlineUsersMsg).toJson() + "\n";
    
    // 发送给所有在线用户
    foreach (const OnlineUser &user, m_onlineUsers.values()) {
        user.socket->write(data);
    }
    
    qDebug() << "广播在线用户列表，当前在线人数：" << m_onlineUsers.size();
}

// 根据用户ID查找在线用户
OnlineUser* ChatServer::findUserById(int userId)
{
    if (m_onlineUsers.contains(userId)) {
        return &m_onlineUsers[userId];
    }
    return nullptr;
}
