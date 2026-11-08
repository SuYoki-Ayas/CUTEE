#pragma once

#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace cutee {

class Database;
class SettingsRepository;

/// Application settings: data locations, log level and database information.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    SettingsPage(Database &database, SettingsRepository &settings, QWidget *parent = nullptr);

    void reload();

signals:
    void settingsChanged();
    /// Emitted after the database was replaced or reset, so the shell can
    /// refresh every page (profile, subjects, scores, summary).
    void dataReplaced();

private:
    QWidget *buildForm();
    QWidget *buildMaintenanceSection();
    QWidget *buildAboutSection();
    void save();
    void resetForm();
    void undoLastChange();
    void updateActionState();
    QString currentLogLevel() const;
    void openPath(const QString &path, bool isDirectory);
    void showFeedback(const QString &message, bool error = false);
    void exportDatabase();
    void importDatabase();
    void resetData();
    void reloadAfterDataChange();
    QString defaultExportName() const;

    Database &m_database;
    SettingsRepository &m_settings;

    QLineEdit *m_dataDirectory = nullptr;
    QLineEdit *m_databaseFile = nullptr;
    QLineEdit *m_logFile = nullptr;
    QComboBox *m_logLevel = nullptr;
    QLabel *m_feedback = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_importButton = nullptr;
    QPushButton *m_wipeButton = nullptr;

    /// Guards the "dirty" detection while the combo is filled programmatically.
    bool m_updatingForm = false;
    /// Log level stored in the database; the form is "clean" when it matches.
    QString m_loadedLevel;
    /// Level captured right before the last successful save, used by 撤销修改.
    QString m_previousLevel;
    bool m_canUndoLastSave = false;
};

} // namespace cutee
