#ifndef DATABASE_H
#define DATABASE_H
#include<QSqlDatabase>
#include<QSqlError>
#include<QSqlQuery>
#include<QString>
#include<QVariantMap>
#include<QSqlQueryModel>
class Database
{

    //打开和关闭已经是自带的了
public:
    Database();
    ~Database();

    //关于用户的信息 （注册和登陆时可能会用到的）
    bool insertUser(const QString &username, const QString &email, const QString &passwordPlain, const QString &role);
    bool findUserByUsername(const QString &username, QVariantMap &outUser); // returns true and fills outUser if found
    bool verifyUserPassword(const QString &username, const QString &passwordPlain);
    //患者表:插入患者的数据 在注册中可以直接插入
    bool insertPatient(const QString& fullName, const QString& dateOfBirth, const QString& idNumber, const QString& phone, const QString& post, const QString& gender);
    bool insertDoctor(int userId, const QString &fullName, const QString &phone, const QString &specialty, const QString &licenseNumber, const QString &clinicAddress);
    bool createTablesIfNeeded();//建立sql表
    // 病历/预约/诊断/医嘱/处方 插入
       bool insertMedicalCase(int patientId, int createdByDoctorId, const QString &title, const QString &description, const QString &attachments);
       bool insertAppointment(int patientId, int doctorId, const QString &scheduledAt, const QString &status, const QString &reason);
       bool insertDiagnosis(int caseId, int appointmentId, int doctorId, int patientId, const QString &diagnosisText, const QString &icdCodes);
       bool insertMedicalOrder(int diagnosisId, int doctorId, int patientId, const QString &orderText, const QString &orderType, const QString &status);
       bool insertPrescription(int diagnosisId, int doctorId, int patientId, const QString &medicationName, const QString &dosage, const QString &frequency, const QString &duration, const QString &notes);

       // 更新 / 删除（示例：患者）
       bool updatePatient(int patientId, const QVariantMap &fields); // fields: column->value
       bool deletePatient(int patientId);

       // 查询模型（方便直接绑定到 QTableView）
       QSqlQueryModel* modelForTable(const QString &tableName); // caller owns the returned model
       QSqlQueryModel* appointmentsForDoctorModel(int doctorId); // caller owns the returned model
       QSqlQueryModel* casesForPatientModel(int patientId); // caller owns the returned model
       QSqlQueryModel* prescriptionsForPatientModel(int patientId); // caller owns the returned model

private:
     QString hashPasswordDemo(const QString &plain) const;
    QSqlDatabase db;

};

#endif // DATABASE_H
