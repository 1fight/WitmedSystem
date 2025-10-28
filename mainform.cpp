#include "mainform.h"
#include "ui_mainform.h"
#include "register.h"
#include <QMessageBox>
#include "database.h"
#include <QPixmap>
#include <QPainter>

MainForm::MainForm(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainForm), regWindow(new Register)
{
    ui->setupUi(this);
    connect(ui->pushButton_login, &QPushButton::clicked, this, &MainForm::onLoginClicked);
    connect(ui->pushButton_reg, &QPushButton::clicked, this, &MainForm::onRegClicked);
    p=new patient();
}

MainForm::~MainForm()
{
    delete ui;
}

void MainForm::onLoginClicked()
{
    QString user = ui->lineEdit_user->text().trimmed();
    QString pwd  = ui->lineEdit_pwd->text().trimmed();

    if (user.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(this, "提示", "用户名或密码不能为空！");
        return;
    }

    Database db;
    QVariantMap u;
    if (!db.findUserByUsername(user, u)) {
        QMessageBox::warning(this, "登录失败", "用户名不存在！");
        return;
    }
    if (!db.verifyUserPassword(user, pwd)) {
        QMessageBox::warning(this, "登录失败", "密码错误！");
        return;
    }
    // 密码已验证通过
    QString realRole = u["role"].toString();
    if (realRole != ui->comboBox_role->currentText()) {
        QMessageBox::warning(this, "登录失败", "身份选择错误！");
        return;
    }

    QString role = u["role"].toString();
    QMessageBox::information(this, "登录成功",
                             QString("欢迎 %1（%2）").arg(user, role));



    //触发信号   在需要使用目前登录id的界面的构造函数中connect链接 并写槽函数就行
       int id=ui->lineEdit_user->text().toInt();
       emit sendCurrentLoginID(id);

}
void MainForm::onRegClicked()
{
    regWindow->show();
}
