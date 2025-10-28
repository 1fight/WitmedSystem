#include "database.h"
#include <QDebug>
#include <QCryptographicHash>

Database::Database()
{
    // 使用唯一连接名
    if (QSqlDatabase::contains("MedicalDB")) {
        db = QSqlDatabase::database("MedicalDB");
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", "MedicalDB"); // 连接名
        db.setDatabaseName("medical_system.db");
    }

    if (!db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    // 启用 SQLite 外键约束（重要）
    {
        QSqlQuery pragma(db);
        pragma.exec("PRAGMA foreign_keys = ON;");
    }
    // 创建表（如果需要）
    if (!createTablesIfNeeded()) {
        qWarning() << "Failed to create tables";
    }
}

bool Database::createTablesIfNeeded()
{
    QSqlQuery q(db);
    // users
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL,
            is_active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )")) {
        qWarning() << "create users table error:" << q.lastError().text();
        return false;
    }

    // doctors
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS doctors (
            id INTEGER PRIMARY KEY, -- user id
            full_name TEXT NOT NULL,
            phone TEXT,
            specialty TEXT,
            license_number TEXT UNIQUE,
            clinic_address TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(id) REFERENCES users(id) ON DELETE CASCADE
        );
    )")) {
        qWarning() << "create doctors table error:" << q.lastError().text();
        return false;
    }

    // patients
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS patients (
            id INTEGER PRIMARY KEY, -- user id
            full_name TEXT NOT NULL,
            date_of_birth TEXT,
            id_number TEXT UNIQUE,
            phone TEXT,
            post TEXT,
            gender TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(id) REFERENCES users(id) ON DELETE CASCADE
        );
    )")) {
        qWarning() << "create patients table error:" << q.lastError().text();
        return false;
    }

    // medical_cases
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS medical_cases (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_id INTEGER NOT NULL,
            created_by_doctor_id INTEGER,
            title TEXT,
            description TEXT,
            attachments TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(patient_id) REFERENCES patients(id) ON DELETE CASCADE,
            FOREIGN KEY(created_by_doctor_id) REFERENCES doctors(id)
        );
    )")) {
        qWarning() << "create medical_cases error:" << q.lastError().text();
        return false;
    }

    // appointments
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS appointments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_id INTEGER NOT NULL,
            doctor_id INTEGER NOT NULL,
            scheduled_at TEXT NOT NULL,
            status TEXT DEFAULT 'scheduled',
            reason TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(patient_id) REFERENCES patients(id) ON DELETE CASCADE,
            FOREIGN KEY(doctor_id) REFERENCES doctors(id) ON DELETE CASCADE
        );
    )")) {
        qWarning() << "create appointments error:" << q.lastError().text();
        return false;
    }

    // diagnoses
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS diagnoses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            case_id INTEGER,
            appointment_id INTEGER,
            doctor_id INTEGER NOT NULL,
            patient_id INTEGER NOT NULL,
            diagnosis_text TEXT NOT NULL,
            icd_codes TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(case_id) REFERENCES medical_cases(id),
            FOREIGN KEY(appointment_id) REFERENCES appointments(id),
            FOREIGN KEY(doctor_id) REFERENCES doctors(id),
            FOREIGN KEY(patient_id) REFERENCES patients(id)
        );
    )")) {
        qWarning() << "create diagnoses error:" << q.lastError().text();
        return false;
    }

    // medical_orders
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS medical_orders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            diagnosis_id INTEGER,
            doctor_id INTEGER NOT NULL,
            patient_id INTEGER NOT NULL,
            order_text TEXT NOT NULL,
            order_type TEXT,
            status TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(diagnosis_id) REFERENCES diagnoses(id),
            FOREIGN KEY(doctor_id) REFERENCES doctors(id),
            FOREIGN KEY(patient_id) REFERENCES patients(id)
        );
    )")) {
        qWarning() << "create medical_orders error:" << q.lastError().text();
        return false;
    }

    // prescriptions
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS prescriptions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            diagnosis_id INTEGER,
            doctor_id INTEGER NOT NULL,
            patient_id INTEGER NOT NULL,
            medication_name TEXT NOT NULL,
            dosage TEXT,
            frequency TEXT,
            duration TEXT,
            notes TEXT,
            issued_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(diagnosis_id) REFERENCES diagnoses(id),
            FOREIGN KEY(doctor_id) REFERENCES doctors(id),
            FOREIGN KEY(patient_id) REFERENCES patients(id)
        );
    )")) {
        qWarning() << "create prescriptions error:" << q.lastError().text();
        return false;
    }

    // audit_logs
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS audit_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER,
            action TEXT NOT NULL,
            object_type TEXT,
            object_id INTEGER,
            details TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )")) {
        qWarning() << "create audit_logs error:" << q.lastError().text();
        return false;
    }

    return true;
}

// 查找用户（示例）
bool Database::findUserByUsername(const QString &username, QVariantMap &outUser)
{
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("SELECT id, username, email, password_hash, role, is_active, created_at FROM users WHERE username = :u");
    q.bindValue(":u", username);
    if (!q.exec()) {
        qWarning() << "findUser exec error:" << q.lastError().text();
        return false;
    }
    if (q.next()) {
        outUser["id"] = q.value("id");
        outUser["username"] = q.value("username");
        outUser["email"] = q.value("email");
        outUser["password_hash"] = q.value("password_hash");
        outUser["role"] = q.value("role");
        outUser["is_active"] = q.value("is_active");
        outUser["created_at"] = q.value("created_at");
        return true;
    }
    return false;
}

// 验证密码（演示用：SHA256，生产请用 bcrypt/Argon2/libsodium）
static QString simpleHash(const QString &plain)
{
    QByteArray ba = plain.toUtf8();
    QByteArray hash = QCryptographicHash::hash(ba, QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

bool Database::verifyUserPassword(const QString &username, const QString &passwordPlain)
{
    QVariantMap user;
    if (!findUserByUsername(username, user)) return false;
    QString storedHash = user.value("password_hash").toString();
    if (storedHash.isEmpty()) return false;
    return simpleHash(passwordPlain) == storedHash; // demo only
}

// 插入用户（演示：使用简单哈希）
bool Database::insertUser(const QString &username, const QString &email, const QString &passwordPlain, const QString &role)
{
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    QString passwordHash = simpleHash(passwordPlain); // demo only
    q.prepare(R"(
        INSERT INTO users(username, email, password_hash, role)
        VALUES (:username, :email, :password_hash, :role)
    )");
    q.bindValue(":username", username);
    q.bindValue(":email", email);
    q.bindValue(":password_hash", passwordHash);
    q.bindValue(":role", role);
    if (!q.exec()) {
        qWarning() << "insertUser error:" << q.lastError().text();
        return false;
    }
    return true;
}

// 插入患者（注意列名要与表一致）
bool Database::insertPatient(const QString& fullName, const QString& dateOfBirth, const QString& idNumber, const QString& phone, const QString& post, const QString& gender)
{
    if (!db.isOpen()) {
        qWarning() << "Database not open";
        return false;
    }
    QSqlQuery query(db);
    const QString sql = R"(
        INSERT INTO patients (full_name, date_of_birth, id_number, phone, post, gender)
        VALUES (:full_name, :date_of_birth, :id_number, :phone, :post, :gender)
    )";
    if (!query.prepare(sql)) {
        qWarning() << "prepare insertPatient failed:" << query.lastError().text();
        return false;
    }
    query.bindValue(":full_name", fullName);
    query.bindValue(":date_of_birth", dateOfBirth);
    query.bindValue(":id_number", idNumber);
    query.bindValue(":phone", phone);
    query.bindValue(":post", post);
    query.bindValue(":gender", gender);
    if (!query.exec()) {
        qWarning() << "Insert patient failed:" << query.lastError().text();
        return false;
    }
    return true;
}

// 插入医生（与表列保持一致：当前 doctors 表没有 gender 列）
// Database.cpp
// Replace or add this function in your Database implementation.

bool Database::insertDoctor(int userId,
                            const QString &fullName,
                            const QString &phone,
                            const QString &specialty,
                            const QString &licenseNumber,
                            const QString &clinicAddress)
{
    if (!db.isOpen()) {
        qWarning() << "insertDoctor: db not open";
        return false;
    }

    // 确保外键工作（SQLite 每连接需要打开）
    QSqlQuery fk(db);
    fk.exec("PRAGMA foreign_keys = ON;");

    // 1) 验证 users 表存在该 userId
    {
        QSqlQuery chk(db);
        chk.prepare("SELECT 1 FROM users WHERE id = :uid LIMIT 1");
        chk.bindValue(":uid", userId);
        if (!chk.exec()) {
            qWarning() << "insertDoctor: check user exec failed:" << chk.lastError().text();
            return false;
        }
        if (!chk.next()) {
            qWarning() << "insertDoctor: userId does not exist in users:" << userId;
            return false;
        }
    }

    // 处理 licenseNumber：把空字符串当作 SQL NULL
    QVariant licenseValue;
    QString licenseTrim = licenseNumber.trimmed();
    if (licenseTrim.isEmpty()) {
        licenseValue = QVariant(); // NULL
    } else {
        licenseValue = licenseTrim;
        // 如果是非空执业证号，先检查是否被其他 doctor 使用（避免 UNIQUE 失败）
        QSqlQuery checkLicense(db);
        checkLicense.prepare("SELECT id FROM doctors WHERE license_number = :lic LIMIT 1");
        checkLicense.bindValue(":lic", licenseTrim);
        if (!checkLicense.exec()) {
            qWarning() << "insertDoctor: check license exec failed:" << checkLicense.lastError().text();
            return false;
        }
        if (checkLicense.next()) {
            int existingId = checkLicense.value(0).toInt();
            if (existingId != userId) {
                qWarning() << "insertDoctor: license number already used by doctor id=" << existingId;
                // 这里可以根据业务决定：返回 false 并让调用者告知用户，或将冲突处理为 update 等
                return false;
            }
            // 如果 existingId == userId，则允许继续（是更新或重复提交）
        }
    }

    // 2) 检查 doctors 中是否已有该 id（你的表结构以 id==userId）
    QSqlQuery exist(db);
    exist.prepare("SELECT 1 FROM doctors WHERE id = :id LIMIT 1");
    exist.bindValue(":id", userId);
    if (!exist.exec()) {
        qWarning() << "insertDoctor: check doctor exist failed:" << exist.lastError().text();
        return false;
    }

    bool useTx = db.driver()->hasFeature(QSqlDriver::Transactions);
    if (useTx) db.transaction();

    if (exist.next()) {
        // UPDATE existing row（注意绑定 licenseValue 可能为 NULL）
        QSqlQuery q(db);
        const QString sql = QStringLiteral(
            "UPDATE doctors SET full_name = ?, phone = ?, specialty = ?, license_number = ?, clinic_address = ? WHERE id = ?"
        );
        if (!q.prepare(sql)) {
            qWarning() << "insertDoctor: prepare UPDATE failed:" << q.lastError().text();
            if (useTx) db.rollback();
            return false;
        }
        q.addBindValue(fullName);
        q.addBindValue(phone);
        q.addBindValue(specialty);
        q.addBindValue(licenseValue); // NULL 或 实际字符串
        q.addBindValue(clinicAddress);
        q.addBindValue(userId);
        if (!q.exec()) {
            qWarning() << "insertDoctor: UPDATE exec failed:" << q.lastError().text();
            if (useTx) db.rollback();
            return false;
        }
        if (useTx) db.commit();
        qDebug() << "insertDoctor: updated existing doctor id=" << userId;
        return true;
    } else {
        // INSERT 新行（id 存 userId）
        QSqlQuery q(db);
        const QString sql = QStringLiteral(
            "INSERT INTO doctors (id, full_name, phone, specialty, license_number, clinic_address) VALUES (?, ?, ?, ?, ?, ?)"
        );
        if (!q.prepare(sql)) {
            qWarning() << "insertDoctor: prepare INSERT failed:" << q.lastError().text();
            if (useTx) db.rollback();
            return false;
        }
        q.addBindValue(userId);
        q.addBindValue(fullName);
        q.addBindValue(phone);
        q.addBindValue(specialty);
        q.addBindValue(licenseValue); // NULL 或 实际字符串
        q.addBindValue(clinicAddress);

        if (!q.exec()) {
            qWarning() << "insertDoctor: INSERT exec failed:" << q.lastError().text();
            // 如果是 UNIQUE constraint failed: doctors.license_number，可以在这里给出更友好的信息
            if (q.lastError().text().contains("UNIQUE") && !licenseTrim.isEmpty()) {
                qWarning() << "insertDoctor: license number conflict for value =" << licenseTrim;
            }
            if (useTx) db.rollback();
            return false;
        }
        if (useTx) db.commit();
        qDebug() << "insertDoctor: inserted new doctor id=" << userId;
        return true;
    }
}
// 其余插入方法保持不变（你之前实现的逻辑是可以的）
// 例如 insertMedicalCase, insertAppointment, insertDiagnosis, insertMedicalOrder, insertPrescription
// 请确保函数签名与 header 文件一致（如果 header 签名不同，请同步）

bool Database::insertMedicalCase(int patientId, int createdByDoctorId, const QString &title, const QString &description, const QString &attachments)
{
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO medical_cases (patient_id, created_by_doctor_id, title, description, attachments)
        VALUES (:patient_id, :doctor_id, :title, :description, :attachments)
    )");
    q.bindValue(":patient_id", patientId);
    q.bindValue(":doctor_id", createdByDoctorId);
    q.bindValue(":title", title);
    q.bindValue(":description", description);
    q.bindValue(":attachments", attachments);
    if (!q.exec()) {
        qWarning() << "insertMedicalCase error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::insertAppointment(int patientId, int doctorId, const QString &scheduledAt, const QString &status, const QString &reason)
{
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO appointments (patient_id, doctor_id, scheduled_at, status, reason)
        VALUES (:patient_id, :doctor_id, :scheduled_at, :status, :reason)
    )");
    q.bindValue(":patient_id", patientId);
    q.bindValue(":doctor_id", doctorId);
    q.bindValue(":scheduled_at", scheduledAt);
    q.bindValue(":status", status);
    q.bindValue(":reason", reason);
    if (!q.exec()) {
        qWarning() << "insertAppointment error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::insertDiagnosis(int caseId, int appointmentId, int doctorId, int patientId, const QString &diagnosisText, const QString &icdCodes)
{
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO diagnoses (case_id, appointment_id, doctor_id, patient_id, diagnosis_text, icd_codes)
        VALUES (:case_id, :appointment_id, :doctor_id, :patient_id, :diagnosis_text, :icd_codes)
    )");
    q.bindValue(":case_id", caseId > 0 ? QVariant(caseId) : QVariant());
    q.bindValue(":appointment_id", appointmentId > 0 ? QVariant(appointmentId) : QVariant());
    q.bindValue(":doctor_id", doctorId);
    q.bindValue(":patient_id", patientId);
    q.bindValue(":diagnosis_text", diagnosisText);
    q.bindValue(":icd_codes", icdCodes);
    if (!q.exec()) {
        qWarning() << "insertDiagnosis error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::insertMedicalOrder(int diagnosisId, int doctorId, int patientId, const QString &orderText, const QString &orderType, const QString &status)
{
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO medical_orders (diagnosis_id, doctor_id, patient_id, order_text, order_type, status)
        VALUES (:diagnosis_id, :doctor_id, :patient_id, :order_text, :order_type, :status)
    )");
    q.bindValue(":diagnosis_id", diagnosisId > 0 ? QVariant(diagnosisId) : QVariant());
    q.bindValue(":doctor_id", doctorId);
    q.bindValue(":patient_id", patientId);
    q.bindValue(":order_text", orderText);
    q.bindValue(":order_type", orderType);
    q.bindValue(":status", status);
    if (!q.exec()) {
        qWarning() << "insertMedicalOrder error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::insertPrescription(int diagnosisId, int doctorId, int patientId, const QString &medicationName, const QString &dosage, const QString &frequency, const QString &duration, const QString &notes)
{
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO prescriptions (diagnosis_id, doctor_id, patient_id, medication_name, dosage, frequency, duration, notes)
        VALUES (:diagnosis_id, :doctor_id, :patient_id, :medication_name, :dosage, :frequency, :duration, :notes)
    )");
    q.bindValue(":diagnosis_id", diagnosisId > 0 ? QVariant(diagnosisId) : QVariant());
    q.bindValue(":doctor_id", doctorId);
    q.bindValue(":patient_id", patientId);
    q.bindValue(":medication_name", medicationName);
    q.bindValue(":dosage", dosage);
    q.bindValue(":frequency", frequency);
    q.bindValue(":duration", duration);
    q.bindValue(":notes", notes);
    if (!q.exec()) {
        qWarning() << "insertPrescription error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::updatePatient(int patientId, const QVariantMap &fields)
{
    if (!db.isOpen()) return false;
    if (fields.isEmpty()) return true;

    QStringList parts;
    QMap<QString, QVariant> bound;
    int idx = 0;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        QString key = it.key();
        QString param = QString(":p%1").arg(idx++);
        parts << QString("%1 = %2").arg(key, param);
        bound[param] = it.value();
    }
    QString sql = QString("UPDATE patients SET %1 WHERE id = :id").arg(parts.join(", "));
    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        qWarning() << "prepare updatePatient failed:" << q.lastError().text();
        return false;
    }
    for (auto it = bound.constBegin(); it != bound.constEnd(); ++it) {
        q.bindValue(it.key(), it.value());
    }
    q.bindValue(":id", patientId);
    if (!q.exec()) {
        qWarning() << "updatePatient error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::deletePatient(int patientId)
{
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("DELETE FROM patients WHERE id = :id");
    q.bindValue(":id", patientId);
    if (!q.exec()) {
        qWarning() << "deletePatient error:" << q.lastError().text();
        return false;
    }
    return true;
}

QSqlQueryModel* Database::modelForTable(const QString &tableName)
{
    QSqlQueryModel *model = new QSqlQueryModel;
    model->setQuery(QString("SELECT * FROM %1").arg(tableName), db);
    return model;
}

QSqlQueryModel* Database::appointmentsForDoctorModel(int doctorId)
{
    QSqlQueryModel *model = new QSqlQueryModel;
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT a.id, a.scheduled_at, a.status, a.reason, p.full_name AS patient_name, p.phone AS patient_phone
        FROM appointments a
        JOIN patients p ON p.id = a.patient_id
        WHERE a.doctor_id = :did
        ORDER BY a.scheduled_at ASC
    )");
    q.bindValue(":did", doctorId);
    if (!q.exec()) {
        qWarning() << "appointmentsForDoctorModel query error:" << q.lastError().text();
    }
    model->setQuery(q);
    return model;
}

QSqlQueryModel* Database::casesForPatientModel(int patientId)
{
    QSqlQueryModel *model = new QSqlQueryModel;
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT id, title, description, attachments, created_at
        FROM medical_cases
        WHERE patient_id = :pid
        ORDER BY created_at DESC
    )");
    q.bindValue(":pid", patientId);
    if (!q.exec()) {
        qWarning() << "casesForPatientModel query error:" << q.lastError().text();
    }
    model->setQuery(q);
    return model;
}

QSqlQueryModel* Database::prescriptionsForPatientModel(int patientId)
{
    QSqlQueryModel *model = new QSqlQueryModel;
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT pr.id, pr.medication_name, pr.dosage, pr.frequency, pr.duration, pr.issued_at, u.username AS prescriber
        FROM prescriptions pr
        JOIN users u ON u.id = pr.doctor_id
        WHERE pr.patient_id = :pid
        ORDER BY pr.issued_at DESC
    )");
    q.bindValue(":pid", patientId);
    if (!q.exec()) {
        qWarning() << "prescriptionsForPatientModel query error:" << q.lastError().text();
    }
    model->setQuery(q);
    return model;
}

Database::~Database()
{
    if (db.isOpen()) db.close();
}
