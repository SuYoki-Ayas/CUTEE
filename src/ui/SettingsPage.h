#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace cutee {

class Database;
class SettingsRepository;

/// Application settings: data locations, log level and legacy data control.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    SettingsPage(Database &database, SettingsRepository &settings, QWidget *parent = nullptr);

    void reload();

signals:
    void settingsChanged();

private:
    QWidget *buildForm();
    QWidget *buildLegacySection();
    void save();
    void reloadLegacy();
    void clearLegacy();
    void openPath(const QString &path, bool isDirectory);
    void showFeedback(const QString &message, bool error = false);

    Database &m_database;
    SettingsRepository &m_settings;

    QLineEdit *m_dataDirectory = nullptr;
    QLineEdit *m_databaseFile = nullptr;
    QLineEdit *m_logFile = nullptr;
    QComboBox *m_logLevel = nullptr;
    QLabel *m_feedback = nullptr;
    QLabel *m_legacyStatus = nullptr;
    QPushButton *m_clearLegacyButton = nullptr;
};

} // namespace cutee
