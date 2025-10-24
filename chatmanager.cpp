// chatmanager.cpp（放在sources目录）
#include "chatmanager.h"
#include <QMessageBox>

ChatManager::ChatManager(Database *db, QObject *parent)
    : QObject(parent), m_db(db) {}

// 辅助方法：从数据库获取用户ID和角色（对准Database接口）
bool ChatManager::getUserInfoByUsername(const QString &username, int &outUserId, QString &outRole) {
    QVariantMap user;
    if (!m_db->findUserByUsername(username, user)) {
        QMessageBox::warning(nullptr, "错误", "用户不存在（数据库查询失败）");
        return false;
    }
    outUserId = user["id"].toInt();
    outRole = user["role"].toString();
    return true;
}

// 实例化医生端聊天窗口
ChatDialog* ChatManager::createDoctorChatWindow(const QString &doctorUsername, int selectedPatientId, QWidget *parent) {
    int doctorId;
    QString doctorRole;

    // 1. 从数据库获取当前医生的ID和角色（对准Database::findUserByUsername接口）
    if (!getUserInfoByUsername(doctorUsername, doctorId, doctorRole) || doctorRole != "doctor") {
        QMessageBox::warning(parent, "错误", "医生身份验证失败");
        return nullptr;
    }

    // 2. 校验选中的患者ID（确保非0，从医生界面传入）
    if (selectedPatientId == 0) {
        QMessageBox::warning(parent, "错误", "未选择聊天对象（患者）");
        return nullptr;
    }

    // 3. 实例化ChatDialog（严格按构造函数参数传入，无参实例化会报错！）
    return new ChatDialog(
        doctorId,           // 自身ID（医生，从数据库获取）
        "doctor",           // 自身角色（固定为doctor，与数据库role字段一致）
        selectedPatientId,  // 目标ID（选中的患者，从医生界面传入）
        parent              // 父窗口（医生界面）
    );
}

// 实例化患者端聊天窗口
ChatDialog* ChatManager::createPatientChatWindow(const QString &patientUsername, int selectedDoctorId, QWidget *parent) {
    int patientId;
    QString patientRole;

    // 1. 从数据库获取当前患者的ID和角色（对准Database接口）
    if (!getUserInfoByUsername(patientUsername, patientId, patientRole) || patientRole != "patient") {
        QMessageBox::warning(parent, "错误", "患者身份验证失败");
        return nullptr;
    }

    // 2. 校验选中的医生ID（确保非0，从患者界面传入）
    if (selectedDoctorId == 0) {
        QMessageBox::warning(parent, "错误", "未选择聊天对象（医生）");
        return nullptr;
    }

    // 3. 实例化ChatDialog
    return new ChatDialog(
        patientId,          // 自身ID（患者，从数据库获取）
        "patient",          // 自身角色（固定为patient）
        selectedDoctorId,   // 目标ID（选中的医生，从患者界面传入）
        parent              // 父窗口（患者界面）
    );
}
