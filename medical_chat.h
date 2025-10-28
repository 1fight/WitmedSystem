#ifndef MEDICAL_CHAT_H
#define MEDICAL_CHAT_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDateTime>
#include <QMap>
#include <QVariantMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>

// 前向声明
class MainForm;

// 在线用户结构体
struct OnlineUser {
    int id;
    QString username;
    QString role;
    QTcpSocket* socket;
};

// 聊天服务器类
class ChatServer : public QTcpServer {
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    bool startServer(quint16 port = 8888);

private slots:
    void onNewConnection();
    void onDisconnected();
    void onReadyRead();

private:
    QMap<int, OnlineUser> m_onlineUsers;
    bool parseMessage(const QByteArray &data, QJsonObject &obj);
    void broadcastOnlineUsers();
    OnlineUser* findUserById(int userId);
};

// 聊天对话框类
class ChatDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChatDialog(int selfId, const QString &selfRole, int targetId, QWidget *parent = nullptr);
    ~ChatDialog();

private slots:
    void on_sendBtn_clicked();
    void on_connected();
    void on_disconnected();
    void on_readyRead();
    void on_errorOccurred(QAbstractSocket::SocketError err);

private:
    QTcpSocket *m_socket;
    int m_selfId;
    QString m_selfRole;
    int m_targetId;
    bool m_isConnected;

    // UI组件
    QTextEdit *chatEdit;
    QLineEdit *inputEdit;
    QPushButton *sendBtn;

    void sendMessage(const QString &content);
    void connectToServer();
    void setupUI();
};

// 聊天平台类
class ChatPlatform : public QWidget {
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
    MainForm *m_mainForm;
    QTcpSocket *m_socket;
    QMap<int, QString> m_onlineUsers;
    ChatDialog *m_chatDialog;

    // UI组件
    QListWidget *userListWidget;
    QLabel *statusLabel;
    QPushButton *logoutButton;

    void connectToServer();
    void sendOnlineStatus();
    void updateUserList();
    void setupUI();
};

// 数据库类
class Database {
public:
    Database();
    ~Database();

    bool insertUser(const QString &username, const QString &email, const QString &passwordPlain, const QString &role);
    bool findUserByUsername(const QString &username, QVariantMap &outUser);
    bool verifyUserPassword(const QString &username, const QString &passwordPlain);
    bool insertPatient(const QString& fullName, const QString& dateOfBirth, const QString& idNumber, const QString& phone, const QString& post, const QString& gender);
    bool insertDoctor(int userId, const QString &fullName, const QString &phone, const QString &specialty, const QString &licenseNumber, const QString &clinicAddress);
    bool createTablesIfNeeded();
    QSqlQuery getQuery() const;

private:
    QSqlDatabase db;
    QString hashPasswordDemo(const QString &plain) const;
};

// 聊天管理类
class ChatManager : public QObject {
    Q_OBJECT
public:
    explicit ChatManager(Database *db, QObject *parent = nullptr);
    ChatDialog* createDoctorChatWindow(const QString &doctorUsername, int selectedPatientId, QWidget *parent);
    ChatDialog* createPatientChatWindow(const QString &patientUsername, int selectedDoctorId, QWidget *parent);

private:
    Database *m_db;
    bool getUserInfoByUsername(const QString &username, int &outUserId, QString &outRole);
};

#endif // MEDICAL_CHAT_H
