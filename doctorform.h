#ifndef DOCTORFORM_H
#define DOCTORFORM_H

#include <QMainWindow>
#include <QTableView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSqlQueryModel>
#include <QStatusBar>
#include <QLabel>
#include "database.h"
#include "chatmanager.h"

class DoctorForm : public QMainWindow
{
    Q_OBJECT

public:
    explicit DoctorForm(const QString &doctorUsername, Database *db, QWidget *parent = nullptr);
    ~DoctorForm();

private slots:
    void onPatientTableClicked(const QModelIndex &index); // 选中患者
    void onChatBtnClicked();       // 医患聊天
    void onMedicalRecordBtnClicked(); // 查看病历
    void onAppointmentBtnClicked();   // 处理预约
    void onRefreshBtnClicked();       // 刷新数据

private:
    // 核心属性
    QString m_doctorUsername;     // 医生用户名
    int m_doctorId;               // 医生ID（从数据库获取）
    int m_selectedPatientId;      // 选中的患者ID
    Database *m_db;               // 数据库实例
    ChatManager *m_chatManager;   // 聊天管理器

    // 数据模型
    QSqlQueryModel *m_patientModel;    // 患者列表模型
    QSqlQueryModel *m_appointmentModel; // 预约列表模型

    // UI组件
    QTableView *m_patientTable;   // 患者列表
    QTableView *m_appointmentTable; // 预约列表
    QPushButton *m_chatBtn;       // 聊天按钮
    QPushButton *m_medicalRecordBtn; // 病历按钮
    QPushButton *m_appointmentBtn;   // 预约按钮
    QPushButton *m_refreshBtn;     // 刷新按钮
    QStatusBar *m_statusBar;       // 状态栏

    // 初始化方法
    void initUI();                // 纯代码创建界面
    void initData();              // 加载初始数据
    void loadPatients();          // 加载医生接诊的患者
    void loadAppointments();      // 加载医生的预约
};

#endif // DOCTORFORM_H
