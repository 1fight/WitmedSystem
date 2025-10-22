#ifndef DATABASE_H
#define DATABASE_H
#include<QSqlDatabase>
#include<QSqlError>
#include<QSqlQuery>
#include<QString>
class Database
{

    //打开和关闭已经是自带的了
public:
    Database();
    //患者表:插入患者的数据
    bool insertPatient(const QString& name,const QString& date,const QString& ID,const QString& phone,const QString& post,const QString& gender);

private:
    QSqlDatabase db;
};

#endif // DATABASE_H
