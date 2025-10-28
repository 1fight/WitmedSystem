#include "medical_chat.h"

// 前向声明实现
class MainForm {
public:
    int getCurrentUserId() const { return 0; }
    QString getCurrentUserRole() const { return ""; }
    QString getCurrentUsername() const { return ""; }
    void show() {}
};

// ChatServer 实现
ChatServer::ChatServer(QObject *parent) : QTcpServer(parent) {
    connect(this, &ChatServer::newConnection, this, &ChatServer::onNewConnection);
}

bool ChatServer::startServer(quint16 port) {
    if (listen(QHostAddress::Any, port)) {
        qDebug() << "Server started on port" << port;
        return true;
    }
    qDebug() << "Server start failed:" << errorString();
    return false;
}

void ChatServer::onNewConnection() {
    QTcpSocket *socket = nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::disconnected, this, &ChatServer::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &ChatServer::onReadyRead);
}

void ChatServer::onDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    auto it = m_onlineUsers.begin();
    while (it != m_onlineUsers.end()) {
        if (it->socket == socket) {
            it = m_onlineUsers.erase(it);
            broadcastOnlineUsers();
        } else {
            ++it;
        }
    }
    socket->deleteLater();
}

void ChatServer::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QStringList msgs = QString(data).split('\n', Qt::SkipEmptyParts);

    foreach (const QString &msgStr, msgs) {
        QJsonObject msgObj;
        if (!parseMessage(msgStr.toUtf8(), msgObj)) continue;

        QString type = msgObj["type"].toString();
        if (type == "online_status") {
            int userId = msgObj["user_id"].toInt();
            if (msgObj["status"].toString() == "online") {
                OnlineUser user;
                user.id = userId;
                user.username = msgObj["username"].toString();
                user.role = msgObj["role"].toString();
                user.socket = socket;
                m_onlineUsers[userId] = user;
            } else {
                m_onlineUsers.remove(userId);
            }
            broadcastOnlineUsers();
        }
        else if (type == "chat_request" || type == "chat_response") {
            int targetId = msgObj["to"].toInt();
            OnlineUser *user = findUserById(targetId);
            if (user) user->socket->write(msgStr.toUtf8() + "\n");
        }
        else if (type == "chat") {
            int targetId = msgObj["to"].toInt();
            OnlineUser *user = findUserById(targetId);
            if (user) {
                user->socket->write(msgStr.toUtf8() + "\n");
            } else {
                QJsonObject reply{{"type", "error"}, {"message", "User offline"}};
                socket->write(QJsonDocument(reply).toJson() + "\n");
            }
        }
    }
}

bool ChatServer::parseMessage(const QByteArray &data, QJsonObject &obj) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;
    obj = doc.object();
    return true;
}

void ChatServer::broadcastOnlineUsers() {
    QJsonArray usersArray;
    foreach (const OnlineUser &user, m_onlineUsers.values()) {
        usersArray.append(QJsonObject{
            {"id", user.id},
            {"username", user.username},
            {"role", user.role}
        });
    }

    QJsonObject msg{{"type", "online_users"}, {"users", usersArray}};
    QByteArray data = QJsonDocument(msg).toJson() + "\n";

    foreach (const OnlineUser &user, m_onlineUsers.values()) {
        user.socket->write(data);
    }
}

OnlineUser* ChatServer::findUserById(int userId) {
    return m_onlineUsers.contains(userId) ? &m_onlineUsers[userId] : nullptr;
}

// ChatDialog 实现
ChatDialog::ChatDialog(int selfId, const QString &selfRole, int targetId, QWidget *parent)
    : QDialog(parent), m_selfId(selfId), m_selfRole(selfRole), m_targetId(targetId),
      m_isConnected(false), m_socket(new QTcpSocket(this)) {
    setupUI();
    setWindowTitle(QString("%1与%2的聊天").arg(
        selfRole == "doctor" ? "医生" : "患者",
        selfRole == "doctor" ? "患者" : "医生"
    ));

    connect(m_socket, &QTcpSocket::connected, this, &ChatDialog::on_connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatDialog::on_disconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatDialog::on_readyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &ChatDialog::on_errorOccurred);

    connectToServer();
}

ChatDialog::~ChatDialog() {
    m_socket->disconnectFromHost();
}

void ChatDialog::setupUI() {
    chatEdit = new QTextEdit(this);
    chatEdit->setReadOnly(true);
    inputEdit = new QLineEdit(this);
    sendBtn = new QPushButton("发送", this);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(chatEdit);
    
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(inputEdit);
    inputLayout->addWidget(sendBtn);
    mainLayout->addLayout(inputLayout);

    connect(sendBtn, &QPushButton::clicked, this, &ChatDialog::on_sendBtn_clicked);
    connect(inputEdit, &QLineEdit::returnPressed, sendBtn, &QPushButton::click);
}

void ChatDialog::on_sendBtn_clicked() {
    QString content = inputEdit->text().trimmed();
    if (content.isEmpty() || !m_isConnected) return;

    sendMessage(content);
    chatEdit->append(QString("[%1] 我: %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), content));
    inputEdit->clear();
}

void ChatDialog::on_connected() {
    m_isConnected = true;
    chatEdit->append("连接成功，可以开始聊天");
    QJsonObject matchMsg{{"type", "match"}, {"from", m_selfId}, {"to", m_targetId}};
    m_socket->write(QJsonDocument(matchMsg).toJson() + "\n");
}

void ChatDialog::on_disconnected() {
    m_isConnected = false;
    chatEdit->append("连接已断开");
}

void ChatDialog::on_readyRead() {
    QByteArray data = m_socket->readAll();
    QStringList msgs = QString(data).split('\n', Qt::SkipEmptyParts);

    foreach (const QString &msgStr, msgs) {
        QJsonObject msg = QJsonDocument::fromJson(msgStr.toUtf8()).object();
        if (msg["type"].toString() == "chat") {
            chatEdit->append(QString("[%1] 对方: %2")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), msg["content"].toString()));
        }
    }
}

void ChatDialog::on_errorOccurred(QAbstractSocket::SocketError err) {
    chatEdit->append("连接错误: " + m_socket->errorString());
}

void ChatDialog::sendMessage(const QString &content) {
    QJsonObject msg{
        {"type", "chat"},
        {"from", m_selfId},
        {"to", m_targetId},
        {"content", content}
    };
    m_socket->write(QJsonDocument(msg).toJson() + "\n");
}

void ChatDialog::connectToServer() {
    chatEdit->append("正在连接到服务器...");
    m_socket->connectToHost("127.0.0.1", 8888);
}

// ChatPlatform 实现
ChatPlatform::ChatPlatform(MainForm *mainForm, QWidget *parent)
    : QWidget(parent), m_mainForm(mainForm), m_socket(new QTcpSocket(this)), m_chatDialog(nullptr) {
    setupUI();
    setWindowTitle(QString("%1端 - 医患交流平台").arg(
        mainForm->getCurrentUserRole() == "doctor" ? "医生" : "患者"
    ));

    connect(m_socket, &QTcpSocket::connected, this, &ChatPlatform::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatPlatform::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatPlatform::onReadyRead);
    connect(userListWidget, &QListWidget::itemClicked, this, &ChatPlatform::onUserItemClicked);
    connect(logoutButton, &QPushButton::clicked, this, &ChatPlatform::onLogoutClicked);

    connectToServer();
}

ChatPlatform::~ChatPlatform() {
    m_socket->disconnectFromHost();
}

void ChatPlatform::setupUI() {
    userListWidget = new QListWidget(this);
    statusLabel = new QLabel("未连接", this);
    logoutButton = new QPushButton("退出登录", this);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel("在线用户列表", this));
    mainLayout->addWidget(userListWidget);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(logoutButton);
}

void ChatPlatform::connectToServer() {
    statusLabel->setText("正在连接到服务器...");
    m_socket->connectToHost("127.0.0.1", 8888);
}

void ChatPlatform::onConnected() {
    statusLabel->setText("已连接到服务器");
    sendOnlineStatus();
}

void ChatPlatform::onDisconnected() {
    statusLabel->setText("与服务器断开连接");
    m_onlineUsers.clear();
    updateUserList();
}

void ChatPlatform::sendOnlineStatus() {
    QJsonObject statusMsg{
        {"type", "online_status"},
        {"user_id", m_mainForm->getCurrentUserId()},
        {"role", m_mainForm->getCurrentUserRole()},
        {"username", m_mainForm->getCurrentUsername()},
        {"status", "online"}
    };
    m_socket->write(QJsonDocument(statusMsg).toJson() + "\n");
}

void ChatPlatform::updateUserList() {
    userListWidget->clear();
    QString currentRole = m_mainForm->getCurrentUserRole();

    foreach (int userId, m_onlineUsers.keys()) {
        userListWidget->addItem(new QListWidgetItem(m_onlineUsers[userId]));
    }

    if (userListWidget->count() == 0) {
        QListWidgetItem *item = new QListWidgetItem("暂无在线用户");
        item->setEnabled(false);
        userListWidget->addItem(item);
    }
}

void ChatPlatform::onReadyRead() {
    QByteArray data = m_socket->readAll();
    QStringList msgs = QString(data).split('\n', Qt::SkipEmptyParts);

    foreach (const QString &msgStr, msgs) {
        QJsonObject msg = QJsonDocument::fromJson(msgStr.toUtf8()).object();
        QString type = msg["type"].toString();

        if (type == "online_users") {
            m_onlineUsers.clear();
            QJsonArray users = msg["users"].toArray();
            foreach (const QJsonValue &user, users) {
                QJsonObject userObj = user.toObject();
                m_onlineUsers[userObj["id"].toInt()] = userObj["username"].toString();
            }
            updateUserList();
        }
        else if (type == "chat_request") {
            int fromUserId = msg["from"].toInt();
            QString fromUsername = msg["username"].toString();

            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "聊天请求", QString("%1请求与您聊天，是否接受？").arg(fromUsername),
                QMessageBox::Yes|QMessageBox::No);

            QJsonObject response{
                {"type", "chat_response"},
                {"to", fromUserId},
                {"accept", reply == QMessageBox::Yes},
                {"from", m_mainForm->getCurrentUserId()}
            };
            m_socket->write(QJsonDocument(response).toJson() + "\n");

            if (reply == QMessageBox::Yes) {
                if (m_chatDialog) delete m_chatDialog;
                m_chatDialog = new ChatDialog(
                    m_mainForm->getCurrentUserId(),
                    m_mainForm->getCurrentUserRole(),
                    fromUserId,
                    this
                );
                m_chatDialog->show();
            }
        }
        else if (type == "chat_response") {
            bool accepted = msg["accept"].toBool();
            if (accepted) {
                int targetId = msg["from"].toInt();
                if (m_chatDialog) delete m_chatDialog;
                m_chatDialog = new ChatDialog(
                    m_mainForm->getCurrentUserId(),
                    m_mainForm->getCurrentUserRole(),
                    targetId,
                    this
                );
                m_chatDialog->show();
            } else {
                QMessageBox::information(this, "聊天请求", "对方拒绝了您的聊天请求");
            }
        }
    }
}

void ChatPlatform::onUserItemClicked(QListWidgetItem *item) {
    QString username = item->text();
    if (username == "暂无在线用户") return;

    int targetId = -1;
    foreach (int id, m_onlineUsers.keys()) {
        if (m_onlineUsers[id] == username) {
            targetId = id;
            break;
        }
    }

    if (targetId == -1) return;

    QJsonObject request{
        {"type", "chat_request"},
        {"from", m_mainForm->getCurrentUserId()},
        {"to", targetId},
        {"username", m_mainForm->getCurrentUsername()}
    };
    m_socket->write(QJsonDocument(request).toJson() + "\n");
}

void ChatPlatform::onLogoutClicked() {
    QJsonObject statusMsg{
        {"type", "online_status"},
        {"user_id", m_mainForm->getCurrentUserId()},
        {"status", "offline"}
    };
    m_socket->write(QJsonDocument(statusMsg).toJson() + "\n");

    m_socket->disconnectFromHost();
    m_mainForm->show();
    close();
}

// Database 实现
Database::Database() {
    if (QSqlDatabase::contains("MedicalDB")) {
        db = QSqlDatabase::database("MedicalDB");
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", "MedicalDB");
        db.setDatabaseName("medical_system.db");
    }

    if (!db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA foreign_keys = ON;");
    createTablesIfNeeded();
}

Database::~Database() {
    if (db.isOpen()) db.close();
}

bool Database::createTablesIfNeeded() {
    QSqlQuery q(db);
    return q.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL,
            is_active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )") && q.exec(R"(
        CREATE TABLE IF NOT EXISTS doctors (
            id INTEGER PRIMARY KEY,
            full_name TEXT NOT NULL,
            phone TEXT,
            specialty TEXT,
            license_number TEXT UNIQUE,
            clinic_address TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(id) REFERENCES users(id) ON DELETE CASCADE
        )
    )") && q.exec(R"(
        CREATE TABLE IF NOT EXISTS patients (
            id INTEGER PRIMARY KEY,
            full_name TEXT NOT NULL,
            date_of_birth TEXT,
            id_number TEXT UNIQUE,
            phone TEXT,
            post TEXT,
            gender TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(id) REFERENCES users(id) ON DELETE CASCADE
        )
    )");
}

bool Database::findUserByUsername(const QString &username, QVariantMap &outUser) {
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("SELECT id, username, password_hash, role FROM users WHERE username = :u");
    q.bindValue(":u", username);
    if (!q.exec() || !q.next()) return false;

    outUser["id"] = q.value("id");
    outUser["username"] = q.value("username");
    outUser["password_hash"] = q.value("password_hash");
    outUser["role"] = q.value("role");
    return true;
}

QString Database::hashPasswordDemo(const QString &plain) const {
    return QString(QCryptographicHash::hash(plain.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool Database::verifyUserPassword(const QString &username, const QString &passwordPlain) {
    QVariantMap user;
    return findUserByUsername(username, user) && 
           user["password_hash"].toString() == hashPasswordDemo(passwordPlain);
}

bool Database::insertUser(const QString &username, const QString &email, const QString &passwordPlain, const QString &role) {
    QSqlQuery q(db);
    q.prepare("INSERT INTO users(username, email, password_hash, role) VALUES (:u, :e, :p, :r)");
    q.bindValue(":u", username);
    q.bindValue(":e", email);
    q.bindValue(":p", hashPasswordDemo(passwordPlain));
    q.bindValue(":r", role);
    return q.exec();
}

bool Database::insertPatient(const QString& fullName, const QString& dateOfBirth, const QString& idNumber, const QString& phone, const QString& post, const QString& gender) {
    QSqlQuery q(db);
    q.prepare("INSERT INTO patients(full_name, date_of_birth, id_number, phone, post, gender) VALUES (:n, :d, :i, :p, :o, :g)");
    q.bindValue(":n", fullName);
    q.bindValue(":d", dateOfBirth);
    q.bindValue(":i", idNumber);
    q.bindValue(":p", phone);
    q.bindValue(":o", post);
    q.bindValue(":g", gender);
    return q.exec();
}

bool Database::insertDoctor(int userId, const QString &fullName, const QString &phone, const QString &specialty, const QString &licenseNumber, const QString &clinicAddress) {
    QSqlQuery q(db);
    q.prepare("INSERT INTO doctors(id, full_name, phone, specialty, license_number, clinic_address) VALUES (:id, :n, :p, :s, :l, :c)");
    q.bindValue(":id", userId);
    q.bindValue(":n", fullName);
    q.bindValue(":p", phone);
    q.bindValue(":s", specialty);
    q.bindValue(":l", licenseNumber);
    q.bindValue(":c", clinicAddress);
    return q.exec();
}

QSqlQuery Database::getQuery() const {
    return QSqlQuery(db);
}

// ChatManager 实现
ChatManager::ChatManager(Database *db, QObject *parent) : QObject(parent), m_db(db) {}

bool ChatManager::getUserInfoByUsername(const QString &username, int &outUserId, QString &outRole) {
    QVariantMap user;
    if (!m_db->findUserByUsername(username, user)) return false;
    outUserId = user["id"].toInt();
    outRole = user["role"].toString();
    return true;
}

ChatDialog* ChatManager::createDoctorChatWindow(const QString &doctorUsername, int selectedPatientId, QWidget *parent) {
    int doctorId;
    QString role;
    if (!getUserInfoByUsername(doctorUsername, doctorId, role) || role != "doctor") return nullptr;
    if (selectedPatientId == 0) return nullptr;
    return new ChatDialog(doctorId, "doctor", selectedPatientId, parent);
}

ChatDialog* ChatManager::createPatientChatWindow(const QString &patientUsername, int selectedDoctorId, QWidget *parent) {
    int patientId;
    QString role;
    if (!getUserInfoByUsername(patientUsername, patientId, role) || role != "patient") return nullptr;
    if (selectedDoctorId == 0) return nullptr;
    return new ChatDialog(patientId, "patient", selectedDoctorId, parent);
}
