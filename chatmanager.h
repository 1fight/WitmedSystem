// chatmanager.h（放在sources目录）
#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>
#include <QWidget>
#include "chatdialog.h"
#include "database.h"

class ChatManager : public QObject {
    Q_OBJECT
public:
    explicit ChatManager(Database *db, QObject *parent = nullptr);

    // 核心接口：实例化医生端聊天窗口（返回ChatDialog对象）
    ChatDialog* createDoctorChatWindow(const QString &doctorUsername, int selectedPatientId, QWidget *parent);

    // 核心接口：实例化患者端聊天窗口（返回ChatDialog对象）
    ChatDialog* createPatientChatWindow(const QString &patientUsername, int selectedDoctorId, QWidget *parent);

private:
    Database *m_db; // 持有数据库实例（复用项目已有的Database，不重复创建）

    // 辅助方法：通过用户名从数据库获取用户ID和角色
    bool getUserInfoByUsername(const QString &username, int &outUserId, QString &outRole);
};

#endif // CHATMANAGER_H
