// ChatServer.cpp
#include "ChatServer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

ChatServer::ChatServer(QObject *parent) : QTcpServer(parent) {
    connect(this, &QTcpServer::newConnection, this, &ChatServer::onNewConnection);
}

// 启动服务器
void ChatServer::startServer(quint16 port) {
    if (listen(QHostAddress::LocalHost, port)) {
        qDebug() << "聊天服务器启动，端口:" << port;
    } else {
        qDebug() << "服务器启动失败:" << errorString();
    }
}

// 新客户端连接
void ChatServer::onNewConnection() {
    QTcpSocket *socket = nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &ChatServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &ChatServer::onDisconnected);
    qDebug() << "新客户端连接:" << socket->peerAddress().toString();
}

// 接收客户端消息并转发
void ChatServer::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QStringList msgs = QString(data).split('\n');

    foreach (const QString &msgStr, msgs) {
        if (msgStr.isEmpty()) continue;
        QJsonObject msg = QJsonDocument::fromJson(msgStr.toUtf8()).object();
        int from = msg["from"].toInt();
        int to = msg["to"].toInt();

        // 记录用户与Socket的映射（首次收到消息时）
        if (!m_socketUsers.contains(socket)) {
            m_socketUsers[socket] = from;
            m_userSockets[from] = socket;
            qDebug() << "用户" << from << "已连接";
        }

        // 处理匹配请求（告知对方有新聊天请求）
        if (msg["type"].toString() == "match") {
            if (m_userSockets.contains(to)) {
                // 向目标用户发送匹配通知
                QJsonObject resp {
                    {"type", "match_resp"},
                    {"msg", "对方已加入聊天，可开始对话"}
                };
                m_userSockets[to]->write(QJsonDocument(resp).toJson() + "\n");
                // 向发起者发送匹配成功通知
                resp["msg"] = "已成功连接对方";
                socket->write(QJsonDocument(resp).toJson() + "\n");
            } else {
                // 目标用户未连接
                QJsonObject resp {
                    {"type", "match_resp"},
                    {"msg", "对方未在线，请稍后尝试"}
                };
                socket->write(QJsonDocument(resp).toJson() + "\n");
            }
        }
        // 转发聊天消息
        else if (msg["type"].toString() == "chat" && m_userSockets.contains(to)) {
            m_userSockets[to]->write(QJsonDocument(msg).toJson() + "\n"); // 转发给目标
        }
    }
}

// 客户端断开连接
void ChatServer::onDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (m_socketUsers.contains(socket)) {
        int userId = m_socketUsers[socket];
        m_userSockets.remove(userId);
        m_socketUsers.remove(socket);
        qDebug() << "用户" << userId << "已断开";
    }
    socket->deleteLater();
}
