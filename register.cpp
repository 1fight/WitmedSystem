#include "register.h"
#include "ui_register.h"
#include <QMessageBox>
#include "database.h"

Register::Register(QWidget *parent)
    : QWidget(parent), ui(new Ui::Register)
{
    ui->setupUi(this);
    connect(ui->pushButton_regOK, &QPushButton::clicked, this, &Register::onRegOKClicked);
}

Register::~Register()
{
    delete ui;
}



void Register::onRegOKClicked()
{
    QString user = ui->lineEdit_regUse->text().trimmed();
    QString pwd  = ui->lineEdit_regPwd->text().trimmed();
    QString role = ui->comboBox_role->currentText();

    if (user.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(this, "提示", "用户名或密码不能为空！");
        return;
    }

    Database db;
    QVariantMap u;
    if (db.findUserByUsername(user, u)) {
        QMessageBox::warning(this, "注册失败", "用户名已存在！");
        return;
    }

    // ① 插入 users 表
    if (!db.insertUser(user, "", pwd, role)) {
        QMessageBox::warning(this, "注册失败", "写入用户表失败！");
        return;
    }

    // ② 查出刚插入的 userId
    if (!db.findUserByUsername(user, u)) {
        QMessageBox::warning(this, "注册失败", "无法获取用户信息！");
        return;
    }
    int userId = u["id"].toInt();

    // ③ 获取 UI 上有的值
    QString age        = ui->lineEdit_age->text().trimmed();
    QString idNumber   = ui->lineEdit_IDNumber->text().trimmed();
    QString phone      = ui->lineEdit_PhoneNumber->text().trimmed();
    QString address    = ui->lineEdit_address->text().trimmed();

    // ④ 根据角色插入 patients 或 doctors 表
    if (role == "患者") {
        QString fullName = user;        // 先用用户名顶替姓名
        QString birth    = age;         // 先用年龄顶替出生日期
        QString gender   = ui->comboBox_gender->currentText();       // 你没性别输入，先空着

        if (!db.insertPatient(fullName, birth, idNumber, phone, address, gender)) {
            QMessageBox::warning(this, "注册失败", "写入患者表失败！");
            return;
        }
    } else if (role == "医生") {
        QString fullName      = user;   // 先用用户名顶替姓名
        QString specialty     = "";     // 你没科室输入，先空着
        QString licenseNumber = "";     // 你没执业证号输入，先空着
        QString clinicAddress = address;// 你用输入的地址
        QString gender   = ui->comboBox_gender->currentText();

        if (!db.insertDoctor(userId, fullName, phone, specialty, licenseNumber, clinicAddress)) {
            QMessageBox::warning(this, "注册失败", "写入医生表失败！");
            return;
        }
    }

    QMessageBox::information(this, "注册成功", "用户已注册并同步到对应表！");
    this->close();
}
