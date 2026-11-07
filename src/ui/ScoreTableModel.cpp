#include "ui/ScoreTableModel.h"

#include "core/ScoreValue.h"

#include <QColor>

namespace cutee {
namespace {

constexpr char kTextColor[] = "#1f2933";
constexpr char kLowRateColor[] = "#b54708";
constexpr char kInvalidScoreColor[] = "#b42318";

const QStringList &headerLabels()
{
    static const QStringList labels = {
        QStringLiteral("日期"),     QStringLiteral("类型"),   QStringLiteral("科目"),
        QStringLiteral("考试名称"), QStringLiteral("得分"),   QStringLiteral("满分"),
        QStringLiteral("得分率"),   QStringLiteral("排名"),   QStringLiteral("参考人数"),
        QStringLiteral("备注"),
    };
    return labels;
}

} // namespace

ScoreTableModel::ScoreTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ScoreTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

int ScoreTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ScoreTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return {};

    const ExamRecord &record = m_records.at(index.row());
    const int column = index.column();

    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case DateColumn:
            return record.examDate.toString(QStringLiteral("yyyy-MM-dd"));
        case TypeColumn:
            return examTypeDisplayName(record.examType);
        case SubjectColumn:
            return record.subjectName;
        case TitleColumn:
            return record.title;
        case ScoreColumn:
            return scoreutil::formatPoints(record.scoreHundredths);
        case FullScoreColumn:
            return scoreutil::formatPoints(record.fullScoreHundredths);
        case RateColumn:
            return scoreutil::formatRate(record.scoreHundredths, record.fullScoreHundredths);
        case RankColumn:
            return record.rank ? QString::number(*record.rank) : QString();
        case ParticipantsColumn:
            return record.participants ? QString::number(*record.participants) : QString();
        case NoteColumn:
            return record.note;
        default:
            return {};
        }
    case Qt::TextAlignmentRole:
        switch (column) {
        case DateColumn:
            return QVariant(Qt::AlignCenter);
        case ScoreColumn:
        case FullScoreColumn:
        case RateColumn:
        case RankColumn:
        case ParticipantsColumn:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    case Qt::ForegroundRole:
        if (record.fullScoreHundredths > 0 && record.scoreHundredths > record.fullScoreHundredths)
            return QColor(QString::fromLatin1(kInvalidScoreColor));
        if (column == RateColumn && record.fullScoreHundredths > 0
            && scoreutil::rateOf(record.scoreHundredths, record.fullScoreHundredths)
                < kLowRateThreshold)
            return QColor(QString::fromLatin1(kLowRateColor));
        return QColor(QString::fromLatin1(kTextColor));
    case Qt::ToolTipRole:
        if (column == RateColumn && record.scoreHundredths > record.fullScoreHundredths)
            return QStringLiteral("得分高于满分，请核对这条记录");
        if (column == NoteColumn && record.note.isEmpty())
            return {};
        return record.title;
    case RecordIdRole:
        return record.id;
    case ExamTypeRole:
        return static_cast<int>(record.examType);
    case SubjectIdRole:
        return record.subjectId;
    case RateRole:
        return scoreutil::rateOf(record.scoreHundredths, record.fullScoreHundredths);
    case SearchRole:
        return QStringLiteral("%1\n%2\n%3").arg(record.title, record.subjectName, record.note);
    case SortValueRole:
        switch (column) {
        case DateColumn:
            return record.examDate;
        case TypeColumn:
            return static_cast<int>(record.examType);
        case SubjectColumn:
            return record.subjectName.toLower();
        case TitleColumn:
            return record.title.toLower();
        case ScoreColumn:
            return record.scoreHundredths;
        case FullScoreColumn:
            return record.fullScoreHundredths;
        case RateColumn:
            return scoreutil::rateOf(record.scoreHundredths, record.fullScoreHundredths);
        case RankColumn:
            return record.rank ? QVariant(*record.rank) : QVariant();
        case ParticipantsColumn:
            return record.participants ? QVariant(*record.participants) : QVariant();
        case NoteColumn:
            return record.note.toLower();
        default:
            return {};
        }
    default:
        return {};
    }
}

QVariant ScoreTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QAbstractTableModel::headerData(section, orientation, role);

    return headerLabels().value(section);
}

void ScoreTableModel::setRecords(QVector<ExamRecord> records)
{
    beginResetModel();
    m_records = std::move(records);
    endResetModel();
}

ExamRecord ScoreTableModel::recordAt(int row) const
{
    return row >= 0 && row < m_records.size() ? m_records.at(row) : ExamRecord{};
}

int ScoreTableModel::indexOfRecord(qint64 id) const
{
    for (int row = 0; row < m_records.size(); ++row) {
        if (m_records.at(row).id == id)
            return row;
    }
    return -1;
}

ScoreFilterProxyModel::ScoreFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortRole(ScoreTableModel::SortValueRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void ScoreFilterProxyModel::refreshFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
    beginFilterChange();
    endFilterChange();
#else
    invalidateFilter();
#endif
}

void ScoreFilterProxyModel::setExamType(std::optional<ExamType> type)
{
    if (m_type == type)
        return;
    m_type = type;
    refreshFilter();
}

void ScoreFilterProxyModel::setSubjectId(qint64 subjectId)
{
    if (m_subjectId == subjectId)
        return;
    m_subjectId = subjectId;
    refreshFilter();
}

void ScoreFilterProxyModel::setDateRange(const QDate &from, const QDate &to)
{
    if (m_from == from && m_to == to)
        return;
    m_from = from;
    m_to = to;
    refreshFilter();
}

void ScoreFilterProxyModel::setKeyword(const QString &keyword)
{
    const QString trimmed = keyword.trimmed();
    if (m_keyword == trimmed)
        return;
    m_keyword = trimmed;
    refreshFilter();
}

bool ScoreFilterProxyModel::hasActiveFilters() const
{
    return m_type.has_value() || m_subjectId > 0 || m_from.isValid() || m_to.isValid()
        || !m_keyword.isEmpty();
}

bool ScoreFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *model = sourceModel();
    if (!model)
        return false;

    const QModelIndex anchor = model->index(sourceRow, 0, sourceParent);

    if (m_type.has_value()
        && model->data(anchor, ScoreTableModel::ExamTypeRole).toInt()
            != static_cast<int>(*m_type))
        return false;

    if (m_subjectId > 0
        && model->data(anchor, ScoreTableModel::SubjectIdRole).toLongLong() != m_subjectId)
        return false;

    if (m_from.isValid() || m_to.isValid()) {
        const QDate date = model->data(anchor, ScoreTableModel::SortValueRole).toDate();
        if (m_from.isValid() && date < m_from)
            return false;
        if (m_to.isValid() && date > m_to)
            return false;
    }

    if (!m_keyword.isEmpty()) {
        const QString haystack = model->data(anchor, ScoreTableModel::SearchRole).toString();
        if (!haystack.contains(m_keyword, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

} // namespace cutee
