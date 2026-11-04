#pragma once

#include <optional>

#include "core/Models.h"
#include "core/Result.h"

namespace cutee {

class Database;

/// Reads and writes the single student_profile row (id = 1).
class StudentProfileRepository
{
public:
    explicit StudentProfileRepository(Database &database);

    Result<std::optional<StudentProfile>> load() const;
    Result<bool> exists() const;

    /// Inserts the profile on first save and updates it afterwards. There is no
    /// API to create a second student.
    Result<void> save(const StudentProfile &profile);

private:
    Database &m_database;
};

} // namespace cutee
