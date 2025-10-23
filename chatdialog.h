#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <QTcpSocket>
#include <QJsonObject>

namespace Ui {
class ChatDialog;
}

// 聊天窗口（医生/患者通用）
class ChatDialog : public QDialog {
    Q_OBJECT

public:
    // 构造函数：需传入自身ID、角色、目标用户ID
    explicit ChatDialog(int selfId, const QString &selfRole, int targetId, QWidget *parent = nullptr);
    ~ChatDialog();

private slots:
    void on_sendBtn_clicked(); // 发送按钮点击
    void on_connected();       // 连接服务器成功
    void on_disconnected();    // 断开连接
    void on_readyRead();       // 接收消息
    void on_errorOccurred(QAbstractSocket::SocketError err); // 连接错误

private:
    Ui::ChatDialog *ui;
    QTcpSocket *m_socket;
    int m_selfId;       // 自身用户ID（医生/患者）
    QString m_selfRole; // 自身角色（"doctor"/"patient"）
    int m_targetId;     // 目标用户ID（对方）
    bool m_isConnected; // 是否已建立连接

    void sendMessage(const QString &content); // 发送消息
    void connectToServer();                   // 连接服务器
};

#endif // CHATDIALOG_H
