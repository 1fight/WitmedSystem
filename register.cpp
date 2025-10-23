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
    QString user = ui->lineEdit_regUse->text().trimmed();  // 注意你的控件名
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

    if (db.insertUser(user, "", pwd, role)) {   // email 先留空
        QMessageBox::information(this, "成功", "注册成功！");
        this->close();
    } else {
        QMessageBox::warning(this, "注册失败", "写入数据库失败！");
    }
}
