#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QSlider>
#include <QSpinBox>

// Define available themes
QList<Theme> SettingsDialog::getAvailableThemes() {
    QList<Theme> themes;
    
    // Light theme (default)
    themes.append({
        "Light",
        "#f5f5f7",   // background
        "#ffffff",   // surface
        "#fafafa",   // surfaceLight
        "#007aff",   // primary
        "#0056b3",   // primaryHover
        "#f0f0f0",   // secondary
        "#007aff",   // accent
        "#1d1d1f",   // text
        "#86868b",   // textMuted
        "#34c759",   // success
        "#ff9500",   // warning
        "#d2d2d7",   // border
        "#1a1a1a"    // previewBg
    });
    
    // Dark theme
    themes.append({
        "Dark",
        "#1c1c1e",   // background
        "#2c2c2e",   // surface
        "#3a3a3c",   // surfaceLight
        "#0a84ff",   // primary
        "#409cff",   // primaryHover
        "#3a3a3c",   // secondary
        "#0a84ff",   // accent
        "#ffffff",   // text
        "#98989d",   // textMuted
        "#30d158",   // success
        "#ff9f0a",   // warning
        "#48484a",   // border
        "#000000"    // previewBg
    });
    
    // Midnight Blue
    themes.append({
        "Midnight",
        "#0d1b2a",   // background
        "#1b263b",   // surface
        "#273c55",   // surfaceLight
        "#e0e1dd",   // primary
        "#ffffff",   // primaryHover
        "#1b263b",   // secondary
        "#00d4ff",   // accent
        "#e0e1dd",   // text
        "#778da9",   // textMuted
        "#00f5d4",   // success
        "#fee440",   // warning
        "#415a77",   // border
        "#000000"    // previewBg
    });
    
    // Forest Green
    themes.append({
        "Forest",
        "#f1faee",   // background
        "#ffffff",   // surface
        "#f8fff8",   // surfaceLight
        "#2d6a4f",   // primary
        "#1b4332",   // primaryHover
        "#d8f3dc",   // secondary
        "#40916c",   // accent
        "#1b4332",   // text
        "#52796f",   // textMuted
        "#40916c",   // success
        "#e9c46a",   // warning
        "#95d5b2",   // border
        "#1b4332"    // previewBg
    });
    
    // Sunset Orange
    themes.append({
        "Sunset",
        "#fff8f0",   // background
        "#ffffff",   // surface
        "#fff5eb",   // surfaceLight
        "#e85d04",   // primary
        "#d00000",   // primaryHover
        "#ffedd8",   // secondary
        "#f48c06",   // accent
        "#370617",   // text
        "#9d4e15",   // textMuted
        "#38b000",   // success
        "#ffba08",   // warning
        "#ffc8a2",   // border
        "#370617"    // previewBg
    });
    
    // Ocean Blue
    themes.append({
        "Ocean",
        "#f0f9ff",   // background
        "#ffffff",   // surface
        "#f5fbff",   // surfaceLight
        "#0077b6",   // primary
        "#023e8a",   // primaryHover
        "#caf0f8",   // secondary
        "#00b4d8",   // accent
        "#03045e",   // text
        "#0077b6",   // textMuted
        "#06d6a0",   // success
        "#ffd166",   // warning
        "#90e0ef",   // border
        "#03045e"    // previewBg
    });
    
    // Purple Haze
    themes.append({
        "Purple",
        "#faf5ff",   // background
        "#ffffff",   // surface
        "#f5f0ff",   // surfaceLight
        "#7c3aed",   // primary
        "#5b21b6",   // primaryHover
        "#ede9fe",   // secondary
        "#8b5cf6",   // accent
        "#1e1b4b",   // text
        "#6b7280",   // textMuted
        "#10b981",   // success
        "#f59e0b",   // warning
        "#c4b5fd",   // border
        "#1e1b4b"    // previewBg
    });
    
    // Slate Gray
    themes.append({
        "Slate",
        "#f8fafc",   // background
        "#ffffff",   // surface
        "#f1f5f9",   // surfaceLight
        "#475569",   // primary
        "#334155",   // primaryHover
        "#e2e8f0",   // secondary
        "#64748b",   // accent
        "#0f172a",   // text
        "#64748b",   // textMuted
        "#22c55e",   // success
        "#eab308",   // warning
        "#cbd5e1",   // border
        "#1e293b"    // previewBg
    });
    
    // Rose Pink
    themes.append({
        "Rose",
        "#fff1f2",   // background
        "#ffffff",   // surface
        "#fff5f6",   // surfaceLight
        "#e11d48",   // primary
        "#be123c",   // primaryHover
        "#ffe4e6",   // secondary
        "#f43f5e",   // accent
        "#4c0519",   // text
        "#9f1239",   // textMuted
        "#22c55e",   // success
        "#f59e0b",   // warning
        "#fecdd3",   // border
        "#4c0519"    // previewBg
    });
    
    // High Contrast
    themes.append({
        "High Contrast",
        "#ffffff",   // background
        "#ffffff",   // surface
        "#f5f5f5",   // surfaceLight
        "#000000",   // primary
        "#333333",   // primaryHover
        "#eeeeee",   // secondary
        "#0066cc",   // accent
        "#000000",   // text
        "#555555",   // textMuted
        "#008000",   // success
        "#cc6600",   // warning
        "#000000",   // border
        "#000000"    // previewBg
    });
    
    return themes;
}

Theme SettingsDialog::getCurrentTheme() {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    QString themeName = settings.value("theme", "Light").toString();
    
    for (const auto &theme : getAvailableThemes()) {
        if (theme.name == themeName) {
            return theme;
        }
    }
    
    return getAvailableThemes().first();
}

void SettingsDialog::setCurrentTheme(const QString &themeName) {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    settings.setValue("theme", themeName);
}

QString SettingsDialog::getThemeName() {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    return settings.value("theme", "Light").toString();
}

bool SettingsDialog::shouldConvertHeic() {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    return settings.value("convertHeic", true).toBool();
}

void SettingsDialog::setConvertHeic(bool convert) {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    settings.setValue("convertHeic", convert);
}

int SettingsDialog::getJpegQuality() {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    return settings.value("jpegQuality", 90).toInt();
}

void SettingsDialog::setJpegQuality(int quality) {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    settings.setValue("jpegQuality", quality);
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Settings");
    setMinimumSize(500, 450);
    setModal(true);
    
    originalTheme_ = getThemeName();
    
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI() {
    Theme theme = getCurrentTheme();
    
    setStyleSheet(QString(R"(
        QDialog {
            background: %1;
        }
        QGroupBox {
            font-weight: bold;
            font-size: 14px;
            border: 1px solid %2;
            border-radius: 10px;
            margin-top: 15px;
            padding: 15px;
            background: %3;
            color: %4;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 8px;
        }
        QLabel {
            color: %4;
        }
        QComboBox {
            background: %3;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 8px 12px;
            color: %4;
            min-width: 150px;
        }
        QComboBox:hover {
            border-color: %5;
        }
        QComboBox::drop-down {
            border: none;
            width: 25px;
        }
        QComboBox QAbstractItemView {
            background: %3;
            border: 1px solid %2;
            selection-background-color: %5;
            color: %4;
        }
        QCheckBox {
            color: %4;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
            border-radius: 4px;
            border: 2px solid %2;
            background: %3;
        }
        QCheckBox::indicator:checked {
            background: %5;
            border-color: %5;
        }
        QPushButton {
            background: %6;
            color: %4;
            border: 1px solid %2;
            border-radius: 8px;
            padding: 10px 25px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: %7;
            border-color: %5;
        }
        QPushButton#applyBtn {
            background: %5;
            color: white;
            border: none;
        }
        QPushButton#applyBtn:hover {
            background: %8;
        }
    )").arg(theme.background, theme.border, theme.surface, theme.text,
            theme.primary, theme.secondary, theme.surfaceLight, theme.primaryHover));
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(25, 25, 25, 25);
    
    // Theme section
    QGroupBox *themeGroup = new QGroupBox("🎨 Appearance");
    QVBoxLayout *themeLayout = new QVBoxLayout(themeGroup);
    themeLayout->setSpacing(15);
    
    QHBoxLayout *themeRow = new QHBoxLayout();
    QLabel *themeLabel = new QLabel("Color Theme:");
    themeCombo_ = new QComboBox();
    
    for (const auto &t : getAvailableThemes()) {
        themeCombo_->addItem(t.name);
    }
    themeCombo_->setCurrentText(getThemeName());
    
    themeRow->addWidget(themeLabel);
    themeRow->addWidget(themeCombo_, 1);
    
    // Theme preview
    previewWidget_ = new QFrame();
    previewWidget_->setFixedHeight(60);
    previewWidget_->setStyleSheet(QString(R"(
        QFrame {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
    )").arg(theme.background, theme.border));
    
    QHBoxLayout *previewLayout = new QHBoxLayout(previewWidget_);
    previewLayout->setSpacing(10);
    
    // Color swatches
    for (int i = 0; i < 5; i++) {
        QFrame *swatch = new QFrame();
        swatch->setFixedSize(40, 40);
        swatch->setObjectName(QString("swatch%1").arg(i));
        previewLayout->addWidget(swatch);
    }
    previewLayout->addStretch();
    
    applyThemePreview(theme);
    
    themeLayout->addLayout(themeRow);
    themeLayout->addWidget(previewWidget_);
    
    // iOS/HEIC section
    QGroupBox *heicGroup = new QGroupBox("🍎 iOS Settings");
    QVBoxLayout *heicLayout = new QVBoxLayout(heicGroup);
    heicLayout->setSpacing(15);
    
    convertHeicCheck_ = new QCheckBox("Convert HEIC photos to JPEG");
    convertHeicCheck_->setToolTip("Automatically convert Apple's HEIC format to widely compatible JPEG");
    
    QHBoxLayout *qualityRow = new QHBoxLayout();
    QLabel *qualityLabel = new QLabel("JPEG Quality:");
    jpegQualityCombo_ = new QComboBox();
    jpegQualityCombo_->addItem("Maximum (100%)", 100);
    jpegQualityCombo_->addItem("High (90%)", 90);
    jpegQualityCombo_->addItem("Medium (75%)", 75);
    jpegQualityCombo_->addItem("Low (60%)", 60);
    
    qualityRow->addWidget(qualityLabel);
    qualityRow->addWidget(jpegQualityCombo_, 1);
    
    QLabel *heicNote = new QLabel("💡 HEIC files use less space but aren't supported everywhere.\n"
                                   "    Converting to JPEG ensures compatibility with all devices.");
    heicNote->setStyleSheet(QString("color: %1; font-size: 12px;").arg(theme.textMuted));
    heicNote->setWordWrap(true);
    
    heicLayout->addWidget(convertHeicCheck_);
    heicLayout->addLayout(qualityRow);
    heicLayout->addWidget(heicNote);
    
    // General section
    QGroupBox *generalGroup = new QGroupBox("⚙️ General");
    QVBoxLayout *generalLayout = new QVBoxLayout(generalGroup);
    
    rememberSettingsCheck_ = new QCheckBox("Remember window size and position");
    rememberSettingsCheck_->setChecked(true);
    
    generalLayout->addWidget(rememberSettingsCheck_);
    
    // Buttons
    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(15);
    
    cancelBtn_ = new QPushButton("Cancel");
    applyBtn_ = new QPushButton("Apply");
    applyBtn_->setObjectName("applyBtn");
    
    buttonRow->addStretch();
    buttonRow->addWidget(cancelBtn_);
    buttonRow->addWidget(applyBtn_);
    
    mainLayout->addWidget(themeGroup);
    mainLayout->addWidget(heicGroup);
    mainLayout->addWidget(generalGroup);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonRow);
    
    // Connections
    connect(themeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &SettingsDialog::onThemeChanged);
    connect(applyBtn_, &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(cancelBtn_, &QPushButton::clicked, this, &SettingsDialog::onCancel);
}

void SettingsDialog::loadSettings() {
    themeCombo_->setCurrentText(getThemeName());
    convertHeicCheck_->setChecked(shouldConvertHeic());
    
    int quality = getJpegQuality();
    for (int i = 0; i < jpegQualityCombo_->count(); i++) {
        if (jpegQualityCombo_->itemData(i).toInt() == quality) {
            jpegQualityCombo_->setCurrentIndex(i);
            break;
        }
    }
}

void SettingsDialog::saveSettings() {
    setCurrentTheme(themeCombo_->currentText());
    setConvertHeic(convertHeicCheck_->isChecked());
    setJpegQuality(jpegQualityCombo_->currentData().toInt());
}

void SettingsDialog::applyThemePreview(const Theme &theme) {
    QStringList colors = {theme.primary, theme.accent, theme.success, theme.warning, theme.text};
    
    for (int i = 0; i < 5; i++) {
        QFrame *swatch = previewWidget_->findChild<QFrame*>(QString("swatch%1").arg(i));
        if (swatch) {
            swatch->setStyleSheet(QString(R"(
                QFrame {
                    background: %1;
                    border-radius: 6px;
                    border: 2px solid %2;
                }
            )").arg(colors[i], theme.border));
        }
    }
    
    previewWidget_->setStyleSheet(QString(R"(
        QFrame {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
    )").arg(theme.background, theme.border));
}

void SettingsDialog::onThemeChanged(int index) {
    Q_UNUSED(index);
    QString themeName = themeCombo_->currentText();
    
    for (const auto &theme : getAvailableThemes()) {
        if (theme.name == themeName) {
            applyThemePreview(theme);
            break;
        }
    }
}

void SettingsDialog::onApply() {
    saveSettings();
    
    Theme newTheme = getCurrentTheme();
    emit themeChanged(newTheme);
    emit settingsChanged();
    
    accept();
}

void SettingsDialog::onCancel() {
    // Restore original theme if changed
    setCurrentTheme(originalTheme_);
    reject();
}
