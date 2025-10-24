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

    // 从映射表中移除断开的客户端（通过Socket反向查找用户ID）
    auto it = m_clientMap.begin();
    while (it != m_clientMap.end()) {
        if (it.value() == clientSocket) {
            qDebug() << "用户ID：" << it.key() << "断开连接";
            it = m_clientMap.erase(it); // 移除并迭代下一个
        } else {
            ++it;
        }
    }

    clientSocket->deleteLater(); // 释放资源
}

// 接收消息并转发
void ChatServer::onReadyRead()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    // 读取客户端发送的所有数据
    QByteArray data = clientSocket->readAll();
    int senderId = -1;    // 发送者ID（医生/患者的user_id）
    int targetId = -1;    // 目标ID（对方的user_id）
    QString content;      // 消息内容

    // 解析消息格式（简易版用JSON，确保格式正确）
    if (!parseMessage(data, senderId, targetId, content)) {
        qDebug() << "消息格式错误，忽略：" << data;
        clientSocket->write("消息格式错误，请使用标准格式");
        return;
    }

    // 首次发送消息时，注册用户ID与Socket的映射（senderId未在映射表中时）
    if (!m_clientMap.contains(senderId)) {
        m_clientMap.insert(senderId, clientSocket);
        qDebug() << "用户ID：" << senderId << "注册到聊天服务器";
    }

    // 检查目标用户是否在线（在映射表中）
    if (m_clientMap.contains(targetId)) {
        // 构造转发消息（保留发送者ID，方便接收方显示）
        QJsonObject forwardMsg;
        forwardMsg["senderId"] = senderId;
        forwardMsg["content"] = content;
        QByteArray forwardData = QJsonDocument(forwardMsg).toJson();

        // 转发给目标用户
        m_clientMap[targetId]->write(forwardData);
        qDebug() << "转发消息：" << senderId << "→" << targetId << "，内容：" << content;
    } else {
        // 目标不在线，提示发送者
        QJsonObject replyMsg;
        replyMsg["error"] = "对方不在线，无法发送消息";
        clientSocket->write(QJsonDocument(replyMsg).toJson());
        qDebug() << "目标用户" << targetId << "不在线，发送失败";
    }
}

// 解析客户端消息（JSON格式校验）
bool ChatServer::parseMessage(const QByteArray &data, int &senderId, int &targetId, QString &content)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;

    QJsonObject obj = doc.object();
    // 必须包含senderId（整数）、targetId（整数）、content（字符串）
    if (!obj.contains("senderId") || !obj["senderId"].isDouble()
        || !obj.contains("targetId") || !obj["targetId"].isDouble()
        || !obj.contains("content") || !obj["content"].isString()) {
        return false;
    }

    senderId = obj["senderId"].toInt();
    targetId = obj["targetId"].toInt();
    content = obj["content"].toString();
    return true;
}
