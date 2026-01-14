#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSettings>

// Theme definitions
struct Theme {
    QString name;
    QString background;
    QString surface;
    QString surfaceLight;
    QString primary;
    QString primaryHover;
    QString secondary;
    QString accent;
    QString text;
    QString textMuted;
    QString success;
    QString warning;
    QString border;
    QString previewBg;
};

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    
    // Theme management
    static QList<Theme> getAvailableThemes();
    static Theme getCurrentTheme();
    static void setCurrentTheme(const QString &themeName);
    static QString getThemeName();
    
    // HEIC settings
    static bool shouldConvertHeic();
    static void setConvertHeic(bool convert);
    static int getJpegQuality();
    static void setJpegQuality(int quality);

signals:
    void themeChanged(const Theme &theme);
    void settingsChanged();

private slots:
    void onThemeChanged(int index);
    void onApply();
    void onCancel();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void applyThemePreview(const Theme &theme);
    
    QComboBox *themeCombo_;
    QCheckBox *convertHeicCheck_;
    QComboBox *jpegQualityCombo_;
    QCheckBox *rememberSettingsCheck_;
    
    QWidget *previewWidget_;
    QLabel *previewLabel_;
    
    QPushButton *applyBtn_;
    QPushButton *cancelBtn_;
    
    QString originalTheme_;
};

#endif // SETTINGSDIALOG_H
