#include "storage/Migrations.h"

namespace cutee {

const QVector<Migration> &migrations()
{
    static const QVector<Migration> value = {
        {
            2,
            QStringLiteral("single student profile, subjects and exam_records"),
            {
                // ---- single student profile --------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS student_profile (
                    id           INTEGER PRIMARY KEY CHECK (id = 1),
                    name         TEXT NOT NULL,
                    student_no   TEXT NOT NULL DEFAULT '',
                    school_class TEXT NOT NULL DEFAULT '',
                    note         TEXT NOT NULL DEFAULT '',
                    created_at   TEXT NOT NULL DEFAULT '',
                    updated_at   TEXT NOT NULL DEFAULT ''
                ))"),

                // ---- subjects ----------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS subjects (
                    id         INTEGER PRIMARY KEY AUTOINCREMENT,
                    name       TEXT NOT NULL,
                    type       TEXT NOT NULL CHECK (type IN ('OI', 'ACADEMIC')),
                    sort_order INTEGER NOT NULL DEFAULT 0,
                    enabled    INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1)),
                    note       TEXT NOT NULL DEFAULT '',
                    created_at TEXT NOT NULL DEFAULT '',
                    updated_at TEXT NOT NULL DEFAULT ''
                ))"),
                QStringLiteral(R"(CREATE UNIQUE INDEX IF NOT EXISTS idx_subjects_type_name
                    ON subjects(type, name COLLATE NOCASE))"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_subjects_sort ON subjects(type, sort_order)"),

                // ---- exam records -----------------------------------------------
                // score_hundredths / full_score_hundredths are integer hundredths
                // of a point: 152.5 is stored as 15250, so no REAL drift.
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS exam_records (
                    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
                    exam_type             TEXT NOT NULL CHECK (exam_type IN ('OI', 'ACADEMIC')),
                    subject_id            INTEGER NOT NULL,
                    title                 TEXT NOT NULL,
                    exam_date             TEXT NOT NULL,
                    score_hundredths      INTEGER NOT NULL DEFAULT 0,
                    full_score_hundredths INTEGER NOT NULL DEFAULT 100,
                    rank                  INTEGER,
                    participants          INTEGER,
                    note                  TEXT NOT NULL DEFAULT '',
                    detail_json           TEXT NOT NULL DEFAULT '',
                    created_at            TEXT NOT NULL,
                    updated_at            TEXT NOT NULL,
                    FOREIGN KEY (subject_id) REFERENCES subjects(id) ON DELETE RESTRICT
                ))"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_exam_records_date ON exam_records(exam_date)"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_exam_records_subject ON exam_records(subject_id)"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_exam_records_type ON exam_records(exam_type)"),

                // ---- settings ----------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS settings (
                    key   TEXT PRIMARY KEY NOT NULL,
                    value TEXT NOT NULL DEFAULT ''
                ))"),
            },
        },
        {
            3,
            QStringLiteral("student profile: entry stage and enrolment year"),
            {
                // The student's stage and the year they entered it replace the
                // old 学号 / 班级 columns; the grade is derived at display time
                // from these two values and the current date.
                QStringLiteral(
                    "ALTER TABLE student_profile ADD COLUMN stage TEXT NOT NULL DEFAULT ''"),
                QStringLiteral(
                    "ALTER TABLE student_profile ADD COLUMN enrollment_year INTEGER NOT NULL "
                    "DEFAULT 0"),
            },
        },
    };
    return value;
}

int targetSchemaVersion()
{
    int target = 0;
    for (const Migration &migration : migrations())
        target = qMax(target, migration.version);
    return target;
}

} // namespace cutee
