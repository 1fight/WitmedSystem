#ifndef MAINFORM_H
#define MAINFORM_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainForm; }
QT_END_NAMESPACE

class Register; // 前向声明

class MainForm : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainForm(QWidget *parent = nullptr);
    ~MainForm();

private slots:
    void onLoginClicked();
    void onRegClicked();

private:
    Ui::MainForm *ui;
    Register *regWindow;
};

#endif // MAINFORM_H
