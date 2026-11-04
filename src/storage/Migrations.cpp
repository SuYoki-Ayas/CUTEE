#include "storage/Migrations.h"

namespace cutee {

const QVector<Migration> &migrations()
{
    static const QVector<Migration> value = {
        {
            1,
            QStringLiteral("initial schema: students, questions, exams, answers, scores, settings"),
            {
                // ---- students ----------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS students (
                    id         TEXT PRIMARY KEY NOT NULL,
                    name       TEXT NOT NULL,
                    student_no TEXT NOT NULL DEFAULT '',
                    class_name TEXT NOT NULL DEFAULT '',
                    note       TEXT NOT NULL DEFAULT '',
                    created_at TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                ))"),
                QStringLiteral(R"(CREATE UNIQUE INDEX IF NOT EXISTS idx_students_student_no
                    ON students(student_no) WHERE student_no <> '')"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_students_name ON students(name)"),

                // ---- questions ---------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS questions (
                    id          TEXT PRIMARY KEY NOT NULL,
                    type        TEXT NOT NULL,
                    title       TEXT NOT NULL,
                    stem        TEXT NOT NULL DEFAULT '',
                    options_json TEXT NOT NULL DEFAULT '[]',
                    answer_json TEXT NOT NULL DEFAULT 'null',
                    score       REAL NOT NULL DEFAULT 0,
                    difficulty  TEXT NOT NULL DEFAULT 'medium',
                    tags        TEXT NOT NULL DEFAULT '',
                    match_mode  TEXT NOT NULL DEFAULT 'trim_case_insensitive',
                    created_at  TEXT NOT NULL,
                    updated_at  TEXT NOT NULL
                ))"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_questions_type ON questions(type)"),

                // ---- exams -------------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS exams (
                    id               TEXT PRIMARY KEY NOT NULL,
                    title            TEXT NOT NULL,
                    note             TEXT NOT NULL DEFAULT '',
                    start_time       TEXT NOT NULL,
                    end_time         TEXT NOT NULL,
                    duration_minutes INTEGER NOT NULL DEFAULT 0,
                    status           TEXT NOT NULL DEFAULT 'draft',
                    created_at       TEXT NOT NULL,
                    updated_at       TEXT NOT NULL
                ))"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_exams_status ON exams(status)"),

                // ---- exam <-> question ------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS exam_questions (
                    exam_id     TEXT NOT NULL,
                    question_id TEXT NOT NULL,
                    score       REAL NOT NULL DEFAULT 0,
                    order_index INTEGER NOT NULL DEFAULT 0,
                    PRIMARY KEY (exam_id, question_id),
                    FOREIGN KEY (exam_id) REFERENCES exams(id) ON DELETE CASCADE,
                    FOREIGN KEY (question_id) REFERENCES questions(id) ON DELETE RESTRICT
                ))"),

                // ---- exam <-> student -------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS exam_students (
                    exam_id    TEXT NOT NULL,
                    student_id TEXT NOT NULL,
                    PRIMARY KEY (exam_id, student_id),
                    FOREIGN KEY (exam_id) REFERENCES exams(id) ON DELETE CASCADE,
                    FOREIGN KEY (student_id) REFERENCES students(id) ON DELETE RESTRICT
                ))"),

                // ---- answers -----------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS answers (
                    exam_id     TEXT NOT NULL,
                    student_id  TEXT NOT NULL,
                    question_id TEXT NOT NULL,
                    answer_json TEXT NOT NULL DEFAULT 'null',
                    updated_at  TEXT NOT NULL,
                    PRIMARY KEY (exam_id, student_id, question_id),
                    FOREIGN KEY (exam_id) REFERENCES exams(id) ON DELETE CASCADE,
                    FOREIGN KEY (student_id) REFERENCES students(id) ON DELETE CASCADE,
                    FOREIGN KEY (question_id) REFERENCES questions(id) ON DELETE CASCADE
                ))"),

                // ---- submissions -------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS submissions (
                    exam_id      TEXT NOT NULL,
                    student_id   TEXT NOT NULL,
                    submitted_at TEXT NOT NULL,
                    status       TEXT NOT NULL DEFAULT 'submitted',
                    PRIMARY KEY (exam_id, student_id),
                    FOREIGN KEY (exam_id) REFERENCES exams(id) ON DELETE CASCADE,
                    FOREIGN KEY (student_id) REFERENCES students(id) ON DELETE CASCADE
                ))"),

                // ---- scores ------------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS scores (
                    exam_id     TEXT NOT NULL,
                    student_id  TEXT NOT NULL,
                    total_score REAL NOT NULL DEFAULT 0,
                    detail_json TEXT NOT NULL DEFAULT '{}',
                    graded_at   TEXT NOT NULL,
                    PRIMARY KEY (exam_id, student_id),
                    FOREIGN KEY (exam_id) REFERENCES exams(id) ON DELETE CASCADE,
                    FOREIGN KEY (student_id) REFERENCES students(id) ON DELETE CASCADE
                ))"),

                // ---- settings ----------------------------------------------------
                QStringLiteral(R"(CREATE TABLE IF NOT EXISTS settings (
                    key   TEXT PRIMARY KEY NOT NULL,
                    value TEXT NOT NULL DEFAULT ''
                ))"),
            },
        },
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

                // Adopt the first legacy student instead of deleting the roster.
                // The legacy students table is left untouched.
                QStringLiteral(R"(INSERT INTO student_profile
                        (id, name, student_no, school_class, note, created_at, updated_at)
                    SELECT 1, name, student_no, class_name, note, created_at, updated_at
                    FROM students
                    WHERE NOT EXISTS (SELECT 1 FROM student_profile)
                    ORDER BY created_at, id
                    LIMIT 1)"),

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
