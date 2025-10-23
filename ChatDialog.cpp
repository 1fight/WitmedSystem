// ChatDialog.cpp
#include "ChatDialog.h"
#include "ui_ChatDialog.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QMessageBox>

ChatDialog::ChatDialog(int selfId, const QString &selfRole, int targetId, QWidget *parent)
    : QDialog(parent), ui(new Ui::ChatDialog),
      m_selfId(selfId), m_selfRole(selfRole), m_targetId(targetId), m_isConnected(false) {
    ui->setupUi(this);
    m_socket = new QTcpSocket(this);

    // 窗口标题显示聊天对象
    setWindowTitle(QString("%1与%2的聊天").arg(
        m_selfRole == "doctor" ? "医生" : "患者",
        m_selfRole == "doctor" ? "患者" : "医生"
    ));

    // 连接Socket信号槽
    connect(m_socket, &QTcpSocket::connected, this, &ChatDialog::on_connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatDialog::on_disconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatDialog::on_readyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &ChatDialog::on_errorOccurred);

    // 自动连接服务器（本地地址，端口8888）
    connectToServer();
}

ChatDialog::~ChatDialog() {
    m_socket->disconnectFromHost();
    delete ui;
}

// 连接服务器（本地服务器）
void ChatDialog::connectToServer() {
    ui->chatEdit->append("正在连接到服务器...");
    m_socket->connectToHost("127.0.0.1", 8888); // 同一电脑，固定本地地址和端口
}

// 发送按钮点击
void ChatDialog::on_sendBtn_clicked() {
    QString content = ui->inputEdit->text().trimmed();
    if (content.isEmpty() || !m_isConnected) return;

    sendMessage(content);
    // 本地显示自己的消息
    ui->chatEdit->append(QString("[%1] 我: %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), content));
    ui->inputEdit->clear();
}

// 发送消息到服务器
void ChatDialog::sendMessage(const QString &content) {
    QJsonObject msg {
        {"type", "chat"},
        {"from", m_selfId},
        {"to", m_targetId},
        {"content", content}
    };
    m_socket->write(QJsonDocument(msg).toJson() + "\n"); // 换行符分隔消息
}

// 连接服务器成功
void ChatDialog::on_connected() {
    m_isConnected = true;
    ui->chatEdit->append("连接成功，可以开始聊天");
    // 发送匹配请求（告知服务器要与目标用户聊天）
    QJsonObject matchMsg {
        {"type", "match"},
        {"from", m_selfId},
        {"to", m_targetId}
    };
    m_socket->write(QJsonDocument(matchMsg).toJson() + "\n");
}

// 断开连接
void ChatDialog::on_disconnected() {
    m_isConnected = false;
    ui->chatEdit->append("连接已断开");
}

// 接收服务器转发的消息
void ChatDialog::on_readyRead() {
    QByteArray data = m_socket->readAll();
    QStringList msgs = QString(data).split('\n'); // 按换行符分割多条消息

    foreach (const QString &msgStr, msgs) {
        if (msgStr.isEmpty()) continue;
        QJsonObject msg = QJsonDocument::fromJson(msgStr.toUtf8()).object();
        if (msg["type"].toString() == "chat") {
            // 显示对方消息
            ui->chatEdit->append(QString("[%1] 对方: %2")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), msg["content"].toString()));
        } else if (msg["type"].toString() == "match_resp") {
            ui->chatEdit->append(msg["msg"].toString()); // 显示匹配结果（如"对方已加入聊天"）
        }
    }
}

// 连接错误处理
void ChatDialog::on_errorOccurred(QAbstractSocket::SocketError err) {
    ui->chatEdit->append("连接错误: " + m_socket->errorString());
}
