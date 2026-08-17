#include "CUTEE.h"
#include "ui_CUTEE.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlTableModel>
#include <QMessageBox>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

CUTEE::CUTEE(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::CUTEEClass)
    , m_model(nullptr)
{
    ui->setupUi(this);

    // 使用用户数据目录存储数据库文件
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString dbPath = QCoreApplication::applicationDirPath() + "/data.db";

    // 初始化数据库连接（使用 SQLite 文件数据库）
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        ui->labelStatus->setText("数据库连接失败: " + m_db.lastError().text());
    }
    else {
        ui->labelStatus->setText("数据库连接成功: " + dbPath);
        // 创建表（如果不存在）
        QSqlQuery query(m_db);
        if (!query.exec("CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT, "
            "email TEXT)")) {
            ui->labelStatus->setText("创建表失败: " + query.lastError().text());
            return;
        }
        // 可选：插入示例数据（仅测试用，可根据需要移除）
        query.exec("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')");
        query.exec("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')");
    }
}

CUTEE::~CUTEE()
{
    delete ui;
    if (m_db.isOpen())
        m_db.close();
}

void CUTEE::on_btnTestDb_clicked()
{
    if (!m_db.isOpen()) {
        QMessageBox::warning(this, "数据库测试", "数据库未连接！");
        return;
    }

    // 查询数据并显示在表格中
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, name, email FROM users")) {
        QMessageBox::critical(this, "数据库测试", "查询失败: " + query.lastError().text());
        return;
    }

    // 如果之前没有创建模型，则创建并设置给表格
    if (!m_model) {
        m_model = new QSqlTableModel(this, m_db);
        m_model->setTable("users");
        m_model->select();
        ui->tableView->setModel(m_model);
        ui->tableView->resizeColumnsToContents();
    }
    else {
        m_model->select();  // 刷新数据
    }

    ui->labelStatus->setText("查询成功，共 " + QString::number(m_model->rowCount()) + " 行数据");
}