#include <qsqldatabase.h>
#ifndef CUTEE_H
#define CUTEE_H

#include <QMainWindow>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui { class CUTEEClass; }
QT_END_NAMESPACE

class QSqlTableModel;

class CUTEE : public QMainWindow
{
    Q_OBJECT

public:
    explicit CUTEE(QWidget* parent = nullptr);
    ~CUTEE();

private slots:
    void on_btnTestDb_clicked();   // 按钮点击槽函数

private:
    Ui::CUTEEClass*ui;
    QSqlDatabase m_db;             // 数据库连接
    QSqlTableModel* m_model;       // 数据模型（用于显示表）
};

#endif // CUTEE_Hv