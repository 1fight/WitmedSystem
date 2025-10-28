#include "chatplatform.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

// 构造函数：接收当前用户ID、角色、数据库对象（从医生/患者界面传递）
ChatPlatform::ChatPlatform(int currentUserId, const QString &currentRole, Database *db, QWidget *parent)
    : QWidget(parent), 
      m_currentUserId(currentUserId), 
      m_currentRole(currentRole), 
      m_db(db),
      m_socket(new QTcpSocket(this)), 
      m_chatDialog(nullptr)
{
    // 初始化UI
    setupUI();
    // 设置窗口标题（区分医生/患者端）
    setWindowTitle(QString("%1端 - 医患交流平台").arg(
        m_currentRole == "doctor" ? "医生" : "患者"
    ));

    // 绑定网络信号槽
    connect(m_socket, &QTcpSocket::connected, this, &ChatPlatform::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatPlatform::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatPlatform::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &ChatPlatform::onSocketError);

    // 连接服务器
    connectToServer();
}

ChatPlatform::~ChatPlatform()
{
    // 断开连接并释放资源
    if (m_socket->isOpen()) {
        sendOnlineStatus("offline"); // 发送下线状态
        m_socket->disconnectFromHost();
    }
    delete m_chatDialog;
}

// 纯代码构建UI界面（无.ui文件）
void ChatPlatform::setupUI()
{
    // 1. 在线用户列表
    m_userListWidget = new QListWidget(this);
    m_userListWidget->setHeaderLabel("在线用户");
    // 绑定列表点击事件（点击用户发起聊天）
    connect(m_userListWidget, &QListWidget::itemClicked, this, &ChatPlatform::onUserItemClicked);

    // 2. 状态标签
    m_statusLabel = new QLabel("未连接到服务器", this);
    m_statusLabel->setStyleSheet("color: #666; font-size: 12px;");

    // 3. 退出按钮
    m_logoutButton = new QPushButton("关闭交流平台", this);
    connect(m_logoutButton, &QPushButton::clicked, this, &ChatPlatform::onLogoutClicked);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel("<h3>在线" + (m_currentRole == "doctor" ? "患者" : "医生") + "列表</h3>", this));
    mainLayout->addWidget(m_userListWidget);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_logoutButton);

    setLayout(mainLayout);
    setMinimumSize(400, 500); // 最小窗口尺寸
}

// 连接到聊天服务器
void ChatPlatform::connectToServer()
{
    m_statusLabel->setText("正在连接服务器...");
    // 连接本地服务器（实际部署时可改为实际IP）
    m_socket->connectToHost("127.0.0.1", 8888);
}

// 服务器连接成功
void ChatPlatform::onConnected()
{
    m_statusLabel->setText("已连接到服务器");
    // 发送上线状态（包含用户ID、角色、用户名）
    sendOnlineStatus("online");
}

// 服务器断开连接
void ChatPlatform::onDisconnected()
{
    m_statusLabel->setText("与服务器断开连接");
    m_onlineUsers.clear(); // 清空在线用户列表
    updateUserList();
}

// 网络错误处理
void ChatPlatform::onSocketError(QAbstractSocket::SocketError error)
{
    m_statusLabel->setText("连接错误：" + m_socket->errorString());
}

// 发送在线状态（上线/下线）
void ChatPlatform::sendOnlineStatus(const QString &status)
{
    if (!m_socket->isOpen()) return;

    // 从数据库获取当前用户名（通过用户ID查询）
    QVariantMap userInfo;
    QString username = "未知用户";
    if (m_db->findUserById(m_currentUserId, userInfo)) { // 假设Database类有findUserById方法
        username = userInfo["username"].toString();
    }

    // 构造状态消息（JSON格式）
    QJsonObject statusMsg {
        {"type", "online_status"},
        {"user_id", m_currentUserId},
        {"role", m_currentRole},
        {"username", username},
        {"status", status}
    };
    // 发送消息（末尾加换行符作为分隔符）
    m_socket->write(QJsonDocument(statusMsg).toJson() + "\n");
}

// 更新在线用户列表（根据角色筛选）
void ChatPlatform::updateUserList()
{
    m_userListWidget->clear();

    // 筛选逻辑：医生只显示在线患者，患者只显示在线医生
    foreach (int userId, m_onlineUsers.keys()) {
        const OnlineUserInfo &user = m_onlineUsers[userId];
        if ((m_currentRole == "doctor" && user.role == "patient") || 
            (m_currentRole == "patient" && user.role == "doctor")) {
            // 添加用户到列表（显示用户名）
            QListWidgetItem *item = new QListWidgetItem(user.username);
            item->setData(Qt::UserRole, userId); // 存储用户ID（用于后续发起聊天）
            m_userListWidget->addItem(item);
        }
    }

    // 无符合条件的在线用户
    if (m_userListWidget->count() == 0) {
        QListWidgetItem *item = new QListWidgetItem("暂无在线" + (m_currentRole == "doctor" ? "患者" : "医生"));
        item->setEnabled(false); // 不可点击
        m_userListWidget->addItem(item);
    }
}

// 接收服务器消息
void ChatPlatform::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    QStringList msgs = QString(data).split('\n', Qt::SkipEmptyParts); // 按换行符拆分消息

    foreach (const QString &msgStr, msgs) {
        QJsonObject msg = QJsonDocument::fromJson(msgStr.toUtf8()).object();
        QString type = msg["type"].toString();

        // 处理在线用户列表更新
        if (type == "online_users") {
            m_onlineUsers.clear();
            QJsonArray users = msg["users"].toArray();
            foreach (const QJsonValue &userVal, users) {
                QJsonObject userObj = userVal.toObject();
                OnlineUserInfo user;
                user.id = userObj["user_id"].toInt();
                user.username = userObj["username"].toString();
                user.role = userObj["role"].toString();
                m_onlineUsers[user.id] = user;
            }
            updateUserList(); // 刷新列表
        }
        // 处理聊天请求
        else if (type == "chat_request") {
            int fromUserId = msg["from"].toInt();
            QString fromUsername = msg["username"].toString();
            // 弹窗询问是否接受
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "聊天请求", 
                QString("%1请求与您聊天，是否接受？").arg(fromUsername),
                QMessageBox::Yes | QMessageBox::No
            );
            // 发送响应给请求方
            sendChatResponse(fromUserId, reply == QMessageBox::Yes);
            // 接受则打开聊天窗口
            if (reply == QMessageBox::Yes) {
                openChatDialog(fromUserId, fromUsername);
            }
        }
        // 处理聊天请求的响应
        else if (type == "chat_response") {
            int fromUserId = msg["from"].toInt();
            bool accepted = msg["accept"].toBool();
            QString fromUsername = getUsernameById(fromUserId); // 从在线列表获取用户名

            if (accepted) {
                // 对方接受，打开聊天窗口
                openChatDialog(fromUserId, fromUsername);
            } else {
                QMessageBox::information(this, "聊天请求", QString("%1拒绝了您的聊天请求").arg(fromUsername));
            }
        }
    }
}

// 点击在线用户发起聊天请求
void ChatPlatform::onUserItemClicked(QListWidgetItem *item)
{
    // 忽略"暂无在线用户"项
    if (!item->isEnabled()) return;

    int targetUserId = item->data(Qt::UserRole).toInt(); // 获取目标用户ID
    QString targetUsername = item->text();

    // 发送聊天请求
    QJsonObject request {
        {"type", "chat_request"},
        {"from", m_currentUserId},
        {"to", targetUserId},
        {"username", getCurrentUsername()} // 发送自己的用户名
    };
    m_socket->write(QJsonDocument(request).toJson() + "\n");
}

// 关闭交流平台（返回医生/患者专属界面）
void ChatPlatform::onLogoutClicked()
{
    sendOnlineStatus("offline"); // 发送下线状态
    m_socket->disconnectFromHost();
    this->close(); // 关闭当前窗口
}

// 发送聊天请求的响应（接受/拒绝）
void ChatPlatform::sendChatResponse(int targetUserId, bool accept)
{
    QJsonObject response {
        {"type", "chat_response"},
        {"from", m_currentUserId},
        {"to", targetUserId},
        {"accept", accept},
        {"username", getCurrentUsername()}
    };
    m_socket->write(QJsonDocument(response).toJson() + "\n");
}

// 打开聊天窗口
void ChatPlatform::openChatDialog(int targetUserId, const QString &targetUsername)
{
    // 关闭已有聊天窗口（避免重复）
    if (m_chatDialog) {
        m_chatDialog->close();
        delete m_chatDialog;
    }
    // 创建新聊天窗口（传递当前用户与目标用户信息）
    m_chatDialog = new ChatDialog(
        m_currentUserId,       // 自己的ID
        getCurrentUsername(),  // 自己的用户名
        m_currentRole,         // 自己的角色
        targetUserId,          // 目标用户ID
        targetUsername,        // 目标用户用户名
        m_socket,              // 复用当前TCP连接
        this
    );
    m_chatDialog->show();
}

// 获取当前用户的用户名（从数据库查询）
QString ChatPlatform::getCurrentUsername()
{
    QVariantMap userInfo;
    if (m_db->findUserById(m_currentUserId, userInfo)) {
        return userInfo["username"].toString();
    }
    return "未知用户";
}

// 根据用户ID获取用户名（从在线列表）
QString ChatPlatform::getUsernameById(int userId)
{
    if (m_onlineUsers.contains(userId)) {
        return m_onlineUsers[userId].username;
    }
    return "未知用户";
}
