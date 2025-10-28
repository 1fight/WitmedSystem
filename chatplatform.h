#ifndef CHATPLATFORM_H
#define CHATPLATFORM_H

#include <QWidget>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidgetItem>

namespace Ui {
class ChatPlatform;
}

class MainForm;
class ChatDialog;

class ChatPlatform : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPlatform(MainForm *mainForm, QWidget *parent = nullptr);
    ~ChatPlatform();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onUserItemClicked(QListWidgetItem *item);
    void onLogoutClicked();

private:
    Ui::ChatPlatform *ui;
    MainForm *m_mainForm;
    QTcpSocket *m_socket;
    QMap<int, QString> m_onlineUsers;  // 存储在线用户ID和名称
    ChatDialog *m_chatDialog;

    void connectToServer();
    void sendOnlineStatus();
    void updateUserList();
};

#endif // CHATPLATFORM_H
