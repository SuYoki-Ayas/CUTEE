#pragma once

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QVector>

#include <optional>

#include "core/Models.h"

namespace cutee {

/// Read-only table model over a snapshot of exam records. Every value the view
/// needs is computed here so QTableView never has to build item objects, which
/// keeps filtering and sorting cheap on thousand-row data sets.
class ScoreTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        DateColumn,
        TypeColumn,
        SubjectColumn,
        TitleColumn,
        ScoreColumn,
        FullScoreColumn,
        RateColumn,
        RankColumn,
        ParticipantsColumn,
        NoteColumn,
        ColumnCount,
    };

    enum Role {
        RecordIdRole = Qt::UserRole,
        SortValueRole,
        ExamTypeRole,
        SubjectIdRole,
        SearchRole,
        RateRole,
    };

    /// Score rates below this value are highlighted in the rate column.
    static constexpr double kLowRateThreshold = 0.60;

    explicit ScoreTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void setRecords(QVector<ExamRecord> records);
    const QVector<ExamRecord> &records() const { return m_records; }
    ExamRecord recordAt(int row) const;
    int indexOfRecord(qint64 id) const;

private:
    QVector<ExamRecord> m_records;
};

/// Session-level filtering that mirrors ExamRecordFilter semantics (type,
/// subject, inclusive date range, keyword over title/subject/note) without
/// touching the database.
class ScoreFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ScoreFilterProxyModel(QObject *parent = nullptr);

    void setExamType(std::optional<ExamType> type);
    void setSubjectId(qint64 subjectId);
    void setDateRange(const QDate &from, const QDate &to);
    void setKeyword(const QString &keyword);

    bool hasActiveFilters() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    void refreshFilter();

    std::optional<ExamType> m_type;
    qint64 m_subjectId = 0;
    QDate m_from;
    QDate m_to;
    QString m_keyword;
};

} // namespace cutee
