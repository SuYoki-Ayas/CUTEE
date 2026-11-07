#pragma once

#include <QHash>
#include <QVector>

#include <optional>

#include "core/Models.h"
#include "core/Result.h"

namespace cutee {

class Database;

/// Subject catalogue shared by OI contests and academic courses.
class SubjectRepository
{
public:
    explicit SubjectRepository(Database &database);

    /// Enabled subjects sorted by sort_order; disabled ones are hidden unless
    /// `includeDisabled` is set (needed by history views and by the editor).
    Result<QVector<Subject>> list(std::optional<ExamType> type = std::nullopt,
                                  bool includeDisabled = true) const;
    Result<Subject> find(qint64 id) const;

    Result<void> create(Subject &subject);
    Result<void> update(const Subject &subject);

    /// Refuses to delete a subject referenced by any exam record.
    Result<void> remove(qint64 id);
    Result<void> setEnabled(qint64 id, bool enabled);

    /// Moves the subject one position up (-1) or down (+1) inside its type.
    Result<void> move(qint64 id, int offset);

    Result<int> referenceCount(qint64 id) const;

    /// Reference counts for every subject in one query; used by list views so
    /// loading a page never issues one query per row.
    Result<QHash<qint64, int>> referenceCounts() const;
    Result<int> nextSortOrder(ExamType type) const;

private:
    Result<QVector<Subject>> listOfType(ExamType type) const;
    Database &m_database;
};

} // namespace cutee
