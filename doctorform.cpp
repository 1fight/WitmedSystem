#include "DoctorForm.h"
#include "chatdialog.h"
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>
#include <QWidget>

DoctorForm::DoctorForm(const QString &doctorUsername, Database *db, QWidget *parent)
    : QMainWindow(parent), 
      m_doctorUsername(doctorUsername), 
      m_db(db), 
      m_selectedPatientId(0),
      m_chatManager(new ChatManager(db, this))
{
    // 从数据库获取医生ID（使用仓库提供的findUserByUsername接口）
    QVariantMap doctorInfo;
    if (!m_db->findUserByUsername(m_doctorUsername, doctorInfo)) {
        QMessageBox::critical(this, "错误", "获取医生信息失败");
        close();
        return;
    }
    m_doctorId = doctorInfo["id"].toInt();

    // 初始化数据模型
    m_patientModel = new QSqlQueryModel(this);
    m_appointmentModel = new QSqlQueryModel(this);

    // 初始化界面（纯代码）
    initUI();

    // 加载数据
    initData();

    // 窗口设置
    setWindowTitle(QString("医生工作台 - %1").arg(doctorUsername));
    setMinimumSize(1000, 600);
}

DoctorForm::~DoctorForm()
{
    delete m_patientModel;
    delete m_appointmentModel;
}

// 纯代码创建界面组件和布局
void DoctorForm::initUI()
{
    // 1. 顶部刷新按钮
    m_refreshBtn = new QPushButton("刷新数据", this);
    m_refreshBtn->setMinimumHeight(30);
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("<h3>医生工作平台</h3>", this));
    topLayout->addStretch();
    topLayout->addWidget(m_refreshBtn);

    // 2. 中间表格区域（左右分栏）
    m_patientTable = new QTableView(this);
    m_patientTable->setModel(m_patientModel);
    m_patientTable->setSelectionBehavior(QAbstractItemView::SelectRows); // 整行选择
    m_patientTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // 列自适应

    m_appointmentTable = new QTableView(this);
    m_appointmentTable->setModel(m_appointmentModel);
    m_appointmentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_appointmentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->addWidget(new QLabel("接诊患者列表", this));
    centerLayout->addWidget(m_patientTable, 1); // 占1份宽度
    centerLayout->addSpacing(20);
    centerLayout->addWidget(new QLabel("我的预约列表", this));
    centerLayout->addWidget(m_appointmentTable, 1); // 占1份宽度

    // 3. 底部操作按钮
    m_chatBtn = new QPushButton("医患聊天", this);
    m_medicalRecordBtn = new QPushButton("查看病历", this);
    m_appointmentBtn = new QPushButton("处理预约", this);
    m_chatBtn->setMinimumHeight(40);
    m_medicalRecordBtn->setMinimumHeight(40);
    m_appointmentBtn->setMinimumHeight(40);

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(m_chatBtn);
    bottomLayout->addWidget(m_medicalRecordBtn);
    bottomLayout->addWidget(m_appointmentBtn);

    // 4. 主布局整合
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(centerLayout, 1); // 表格区域占主要高度
    mainLayout->addLayout(bottomLayout);

    // 5. 状态栏
    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);
    m_statusBar->showMessage("就绪");

    // 6. 设置主窗口中央部件
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    // 绑定信号槽
    connect(m_patientTable, &QTableView::clicked, this, &DoctorForm::onPatientTableClicked);
    connect(m_chatBtn, &QPushButton::clicked, this, &DoctorForm::onChatBtnClicked);
    connect(m_medicalRecordBtn, &QPushButton::clicked, this, &DoctorForm::onMedicalRecordBtnClicked);
    connect(m_appointmentBtn, &QPushButton::clicked, this, &DoctorForm::onAppointmentBtnClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &DoctorForm::onRefreshBtnClicked);
}

// 初始化数据
void DoctorForm::initData()
{
    loadPatients();
    loadAppointments();
}

// 加载医生接诊的患者（关联appointment表，使用仓库getQuery()接口）
void DoctorForm::loadPatients()
{
    // 使用Database的公共方法getQuery()获取查询对象（不直接访问db）
    QSqlQuery query = m_db->getQuery();
    // 仓库中预约表名为appointment（单数），修正表名
    query.prepare(R"(
        SELECT DISTINCT p.id, p.full_name, p.gender, p.date_of_birth, p.phone 
        FROM patients p
        JOIN appointment a ON p.id = a.patient_id
        WHERE a.doctor_id = :doctorId
        ORDER BY a.scheduled_at DESC
    )");
    query.bindValue(":doctorId", m_doctorId);
    if (!query.exec()) {
        m_statusBar->showMessage("患者列表加载失败: " + query.lastError().text());
        return;
    }
    m_patientModel->setQuery(std::move(query));
    // 设置表头
    m_patientModel->setHeaderData(0, Qt::Horizontal, "患者ID");
    m_patientModel->setHeaderData(1, Qt::Horizontal, "姓名");
    m_patientModel->setHeaderData(2, Qt::Horizontal, "性别");
    m_patientModel->setHeaderData(3, Qt::Horizontal, "出生日期");
    m_patientModel->setHeaderData(4, Qt::Horizontal, "电话");
}

// 加载医生的预约（使用仓库getQuery()接口）
void DoctorForm::loadAppointments()
{
    QSqlQuery query = m_db->getQuery();
    query.prepare(R"(
        SELECT id, patient_id, scheduled_at, status, reason 
        FROM appointment 
        WHERE doctor_id = :doctorId 
        ORDER BY scheduled_at DESC
    )");
    query.bindValue(":doctorId", m_doctorId);
    if (!query.exec()) {
        m_statusBar->showMessage("预约列表加载失败: " + query.lastError().text());
        return;
    }
    m_appointmentModel->setQuery(std::move(query));
    // 设置表头
    m_appointmentModel->setHeaderData(0, Qt::Horizontal, "预约ID");
    m_appointmentModel->setHeaderData(1, Qt::Horizontal, "患者ID");
    m_appointmentModel->setHeaderData(2, Qt::Horizontal, "预约时间");
    m_appointmentModel->setHeaderData(3, Qt::Horizontal, "状态");
    m_appointmentModel->setHeaderData(4, Qt::Horizontal, "就诊原因");
}

// 选中患者行（获取患者ID）
void DoctorForm::onPatientTableClicked(const QModelIndex &index)
{
    m_selectedPatientId = m_patientModel->data(
        m_patientModel->index(index.row(), 0) // 第0列是患者ID
    ).toInt();
    QString patientName = m_patientModel->data(
        m_patientModel->index(index.row(), 1) // 第1列是姓名
    ).toString();
    m_statusBar->showMessage(QString("已选中患者: %1 (ID: %2)").arg(patientName).arg(m_selectedPatientId));
}

// 打开医患聊天窗口（对接仓库聊天系统）
void DoctorForm::onChatBtnClicked()
{
    if (m_selectedPatientId == 0) {
        QMessageBox::warning(this, "提示", "请先在患者列表中选择患者");
        return;
    }
    // 调用仓库ChatManager创建医生聊天窗口
    ChatDialog *chatDlg = m_chatManager->createDoctorChatWindow(
        m_doctorUsername, m_selectedPatientId, this
    );
    if (chatDlg) {
        chatDlg->show();
    } else {
        QMessageBox::warning(this, "错误", "创建聊天窗口失败");
    }
}

// 查看病历（使用仓库getQuery()接口）
void DoctorForm::onMedicalRecordBtnClicked()
{
    if (m_selectedPatientId == 0) {
        QMessageBox::warning(this, "提示", "请先选择患者");
        return;
    }
    QSqlQuery query = m_db->getQuery();
    query.prepare("SELECT id, content, created_at FROM medical_case WHERE patient_id = :pid");
    query.bindValue(":pid", m_selectedPatientId);
    if (query.exec() && query.next()) {
        QMessageBox::information(this, "患者病历", 
            QString("病历ID: %1\n内容: %2\n创建时间: %3")
            .arg(query.value(0).toString())
            .arg(query.value(1).toString())
            .arg(query.value(2).toString()));
    } else {
        QMessageBox::information(this, "患者病历", "该患者暂无病历记录");
    }
}

// 处理预约（使用仓库getQuery()接口更新状态）
void DoctorForm::onAppointmentBtnClicked()
{
    QModelIndex index = m_appointmentTable->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "提示", "请先选择预约记录");
        return;
    }
    int appointmentId = m_appointmentModel->data(
        m_appointmentModel->index(index.row(), 0)
    ).toInt();
    QSqlQuery query = m_db->getQuery();
    query.prepare("UPDATE appointment SET status = '已处理' WHERE id = :aid");
    query.bindValue(":aid", appointmentId);
    if (query.exec()) {
        m_statusBar->showMessage("预约处理成功");
        loadAppointments(); // 刷新列表
    } else {
        m_statusBar->showMessage("预约处理失败: " + query.lastError().text());
    }
}

// 刷新数据
void DoctorForm::onRefreshBtnClicked()
{
    initData();
    m_statusBar->showMessage("数据已刷新 " + QDateTime::currentDateTime().toString("HH:mm:ss"));
}
