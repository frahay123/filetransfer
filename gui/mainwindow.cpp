#include "mainwindow.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPixmap>
#include <QBuffer>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QHeaderView>
#include <QDesktopServices>
#include <QUrl>
#include <QGraphicsDropShadowEffect>
#include <QScrollArea>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QMovie>

#ifdef ENABLE_ANDROID
    #ifdef USE_WPD
        #include "../src/wpd_handler.h"
    #else
        #include "../src/mtp_handler.h"
    #endif
#endif
#ifdef ENABLE_IOS
#include "../src/ios_handler.h"
#endif

#include "settingsdialog.h"

// Current theme colors (loaded from settings)
namespace Colors {
    QString Background;
    QString Surface;
    QString SurfaceLight;
    QString Primary;
    QString PrimaryHover;
    QString Secondary;
    QString Accent;
    QString Text;
    QString TextMuted;
    QString Success;
    QString Warning;
    QString Border;
    QString PreviewBg;
    
    void loadFromTheme(const Theme &theme) {
        Background = theme.background;
        Surface = theme.surface;
        SurfaceLight = theme.surfaceLight;
        Primary = theme.primary;
        PrimaryHover = theme.primaryHover;
        Secondary = theme.secondary;
        Accent = theme.accent;
        Text = theme.text;
        TextMuted = theme.textMuted;
        Success = theme.success;
        Warning = theme.warning;
        Border = theme.border;
        PreviewBg = theme.previewBg;
    }
}

// Initialize colors from saved theme
static void initColors() {
    Theme theme = SettingsDialog::getCurrentTheme();
    Colors::loadFromTheme(theme);
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Photo Transfer");
    setMinimumSize(1400, 900);
    setAcceptDrops(true);
    
    // Load theme colors
    initColors();
    
    // Initialize backend
    transferQueue_ = std::make_unique<TransferQueue>();
    database_ = std::make_unique<PhotoDB>();
    
    // State file for resume
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    stateFilePath_ = appData + "/transfer_state.dat";
    
    setupUI();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
    
    loadSettings();
    
    // Check for incomplete transfers
    if (QFile::exists(stateFilePath_)) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Resume Transfer");
        msgBox.setText("An incomplete transfer was found.");
        msgBox.setInformativeText("Would you like to resume it?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setStyleSheet(getMessageBoxStyle());
        
        if (msgBox.exec() == QMessageBox::Yes) {
            onResumeIncomplete();
        }
    }
    
    // Setup worker thread
    workerThread_ = new QThread(this);
    deviceWorker_ = new DeviceWorker();
    deviceWorker_->moveToThread(workerThread_);
    workerThread_->start();
    
    // Setup thumbnail thread
    thumbnailThread_ = new QThread(this);
    thumbnailLoader_ = new ThumbnailLoader();
    thumbnailLoader_->moveToThread(thumbnailThread_);
    thumbnailThread_->start();
    
    // Connect worker signals
    connect(deviceWorker_, &DeviceWorker::devicesFound, this, [this](QStringList devices) {
        deviceCombo_->clear();
        deviceCombo_->addItems(devices);
        statusLabel_->setText(QString("Found %1 device(s)").arg(devices.size()));
    });
    
    connect(deviceWorker_, &DeviceWorker::connected, this, [this](QString name, QString mfr, QString model) {
        isConnected_ = true;
        connectBtn_->setText("⏏ Disconnect");
        connectBtn_->setStyleSheet(getDisconnectButtonStyle());
        deviceInfoLabel_->setText(QString("<span style='font-size:18px;font-weight:bold;'>%1</span><br>"
                                          "<span style='color:%2;'>%3 %4</span>")
                                  .arg(name, Colors::TextMuted, mfr, model));
        statusLabel_->setText("Connected to " + name);
        
        // Enumerate media
        emit requestEnumerate();
    });
    
    connect(deviceWorker_, &DeviceWorker::disconnected, this, [this]() {
        isConnected_ = false;
        connectBtn_->setText("🔗 Connect");
        connectBtn_->setStyleSheet(getConnectButtonStyle());
        deviceInfoLabel_->setText("<span style='color:" + Colors::TextMuted + ";'>No device connected</span>");
        storageLabel_->setText("");
        storageProgress_->setValue(0);
        photoList_->clear();
        mediaList_.clear();
        statusLabel_->setText("Disconnected");
    });
    
    connect(deviceWorker_, &DeviceWorker::storageInfo, this, [this](qint64 total, qint64 free) {
        double usedPct = 100.0 * (total - free) / total;
        storageProgress_->setValue(static_cast<int>(usedPct));
        storageLabel_->setText(QString("%1 / %2 used")
            .arg(formatSize(total - free))
            .arg(formatSize(total)));
    });
    
    connect(deviceWorker_, &DeviceWorker::mediaEnumerated, this, [this](int count) {
        photoCountLabel_->setText(QString("<span style='font-size:24px;font-weight:bold;color:%1;'>%2</span>"
                                          "<span style='color:%3;'> items</span>")
                                  .arg(Colors::Accent).arg(count).arg(Colors::TextMuted));
        updatePhotoList();
        loadThumbnailsAsync();
    });
    
    connect(deviceWorker_, &DeviceWorker::transferProgress, this, &MainWindow::onTransferProgress);
    connect(deviceWorker_, &DeviceWorker::transferComplete, this, &MainWindow::onTransferComplete);
    connect(deviceWorker_, &DeviceWorker::error, this, &MainWindow::onTransferError);
    
    connect(thumbnailLoader_, &ThumbnailLoader::thumbnailLoaded, this, &MainWindow::onThumbnailLoaded);
    
    // Initial device refresh
    QTimer::singleShot(500, this, &MainWindow::onRefreshDevices);
}

MainWindow::~MainWindow() {
    saveSettings();
    
    workerThread_->quit();
    workerThread_->wait();
    delete deviceWorker_;
    
    thumbnailThread_->quit();
    thumbnailThread_->wait();
    delete thumbnailLoader_;
}

QString MainWindow::getGlobalStyle() {
    return QString(R"(
        QMainWindow {
            background: %1;
        }
        QWidget {
            background: transparent;
            color: %2;
            font-family: 'Segoe UI', 'SF Pro Display', -apple-system, sans-serif;
        }
        QGroupBox {
            font-weight: 600;
            font-size: 14px;
            border: 1px solid %3;
            border-radius: 12px;
            margin-top: 20px;
            padding: 20px 15px 15px 15px;
            background: %4;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 20px;
            padding: 0 10px;
            color: %2;
        }
        QScrollBar:vertical {
            background: %4;
            width: 10px;
            border-radius: 5px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %3;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: %5;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: %4;
            height: 10px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal {
            background: %3;
            border-radius: 5px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: %5;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
        QComboBox {
            background: %4;
            border: 1px solid %3;
            border-radius: 8px;
            padding: 10px 15px;
            font-size: 14px;
            min-width: 200px;
        }
        QComboBox:hover {
            border-color: %5;
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid %6;
            margin-right: 10px;
        }
        QComboBox QAbstractItemView {
            background: %4;
            border: 1px solid %3;
            border-radius: 8px;
            selection-background-color: %7;
            outline: none;
        }
        QLineEdit {
            background: %4;
            border: 1px solid %3;
            border-radius: 8px;
            padding: 12px 15px;
            font-size: 14px;
            selection-background-color: %5;
        }
        QLineEdit:focus {
            border-color: %5;
        }
        QProgressBar {
            background: %4;
            border: none;
            border-radius: 6px;
            height: 12px;
            text-align: center;
            font-size: 11px;
            color: %6;
        }
        QProgressBar::chunk {
            border-radius: 6px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 %5, stop:1 %8);
        }
        QMenuBar {
            background: %1;
            border-bottom: 1px solid %3;
            padding: 5px;
        }
        QMenuBar::item {
            padding: 8px 15px;
            border-radius: 6px;
        }
        QMenuBar::item:selected {
            background: %4;
        }
        QMenu {
            background: %4;
            border: 1px solid %3;
            border-radius: 8px;
            padding: 5px;
        }
        QMenu::item {
            padding: 10px 30px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            background: %7;
        }
        QStatusBar {
            background: %1;
            border-top: 1px solid %3;
            padding: 5px;
        }
        QToolTip {
            background: %4;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 8px;
            color: %2;
        }
    )").arg(Colors::Background, Colors::Text, Colors::Border, Colors::Surface,
            Colors::Primary, Colors::TextMuted, Colors::Secondary, Colors::Accent);
}

QString MainWindow::getConnectButtonStyle() {
    return QString(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 %1, stop:1 %2);
            color: white;
            border: none;
            border-radius: 10px;
            padding: 12px 25px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 %2, stop:1 %1);
        }
        QPushButton:pressed {
            background: %1;
        }
    )").arg(Colors::Primary, Colors::PrimaryHover);
}

QString MainWindow::getDisconnectButtonStyle() {
    return QString(R"(
        QPushButton {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 10px;
            padding: 12px 25px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: %4;
            border-color: %5;
        }
    )").arg(Colors::Surface, Colors::Text, Colors::Border, Colors::SurfaceLight, Colors::Primary);
}

QString MainWindow::getStartButtonStyle() {
    return QString(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %1, stop:1 #00e676);
            color: white;
            border: none;
            border-radius: 12px;
            padding: 15px 40px;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #00e676, stop:1 %1);
        }
        QPushButton:disabled {
            background: %2;
            color: %3;
        }
    )").arg(Colors::Success, Colors::Surface, Colors::TextMuted);
}

QString MainWindow::getSecondaryButtonStyle() {
    return QString(R"(
        QPushButton {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 10px;
            padding: 12px 25px;
            font-size: 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: %4;
            border-color: %5;
        }
        QPushButton:disabled {
            background: %1;
            color: %6;
            border-color: %1;
        }
    )").arg(Colors::Surface, Colors::Text, Colors::Border, 
            Colors::SurfaceLight, Colors::Accent, Colors::TextMuted);
}

QString MainWindow::getMessageBoxStyle() {
    return QString(R"(
        QMessageBox {
            background: %1;
        }
        QMessageBox QLabel {
            color: %2;
        }
        QPushButton {
            background: %3;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 20px;
            min-width: 80px;
        }
        QPushButton:hover {
            background: %4;
        }
    )").arg(Colors::Surface, Colors::Text, Colors::Primary, Colors::PrimaryHover);
}

void MainWindow::setupUI() {
    // Apply global style
    setStyleSheet(getGlobalStyle());
    
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    
    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Left panel: Device and photo list
    QWidget *leftPanel = new QWidget();
    leftPanel->setMaximumWidth(600);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(20);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    createDevicePanel();
    createPhotoPanel();
    
    leftLayout->addWidget(findChild<QGroupBox*>("deviceGroup"));
    leftLayout->addWidget(findChild<QGroupBox*>("photoGroup"), 1);
    
    // Right panel: Preview and transfer
    QWidget *rightPanel = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(20);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    createPreviewPanel();
    createTransferPanel();
    
    rightLayout->addWidget(findChild<QGroupBox*>("previewGroup"), 1);
    rightLayout->addWidget(findChild<QGroupBox*>("transferGroup"));
    
    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(rightPanel, 1);
}

void MainWindow::createDevicePanel() {
    QGroupBox *group = new QGroupBox("📱 Device", this);
    group->setObjectName("deviceGroup");
    
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(15);
    
    // Device selection row
    QHBoxLayout *selectRow = new QHBoxLayout();
    deviceCombo_ = new QComboBox();
    deviceCombo_->addItem("🔍 Searching for devices...");
    
    refreshBtn_ = new QPushButton("↻ Refresh");
    refreshBtn_->setStyleSheet(getSecondaryButtonStyle());
    refreshBtn_->setFixedWidth(100);
    
    connectBtn_ = new QPushButton("🔗 Connect");
    connectBtn_->setStyleSheet(getConnectButtonStyle());
    connectBtn_->setFixedWidth(140);
    
    selectRow->addWidget(deviceCombo_, 1);
    selectRow->addWidget(refreshBtn_);
    selectRow->addWidget(connectBtn_);
    
    // Device info card
    QFrame *infoCard = new QFrame();
    infoCard->setStyleSheet(QString(R"(
        QFrame {
            background: %1;
            border-radius: 10px;
            padding: 15px;
        }
    )").arg(Colors::SurfaceLight));
    
    QVBoxLayout *infoLayout = new QVBoxLayout(infoCard);
    
    deviceInfoLabel_ = new QLabel("<span style='color:" + Colors::TextMuted + ";'>No device connected</span>");
    deviceInfoLabel_->setTextFormat(Qt::RichText);
    
    // Storage bar
    QWidget *storageWidget = new QWidget();
    QVBoxLayout *storageLayout = new QVBoxLayout(storageWidget);
    storageLayout->setContentsMargins(0, 10, 0, 0);
    storageLayout->setSpacing(5);
    
    storageLabel_ = new QLabel("");
    storageLabel_->setStyleSheet("color: " + Colors::TextMuted + "; font-size: 12px;");
    
    storageProgress_ = new QProgressBar();
    storageProgress_->setRange(0, 100);
    storageProgress_->setValue(0);
    storageProgress_->setTextVisible(false);
    storageProgress_->setFixedHeight(8);
    storageProgress_->setStyleSheet(QString(R"(
        QProgressBar {
            background: %1;
            border-radius: 4px;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %2, stop:0.7 %3, stop:1 %4);
            border-radius: 4px;
        }
    )").arg(Colors::Surface, Colors::Success, Colors::Warning, Colors::Primary));
    
    storageLayout->addWidget(storageLabel_);
    storageLayout->addWidget(storageProgress_);
    
    infoLayout->addWidget(deviceInfoLabel_);
    infoLayout->addWidget(storageWidget);
    
    layout->addLayout(selectRow);
    layout->addWidget(infoCard);
}

void MainWindow::createPhotoPanel() {
    QGroupBox *group = new QGroupBox("🖼 Photos & Videos", this);
    group->setObjectName("photoGroup");
    
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(15);
    
    // Header with count and buttons
    QHBoxLayout *headerRow = new QHBoxLayout();
    
    photoCountLabel_ = new QLabel("<span style='color:" + Colors::TextMuted + ";'>No items</span>");
    photoCountLabel_->setTextFormat(Qt::RichText);
    
    selectAllBtn_ = new QPushButton("Select All");
    selectAllBtn_->setStyleSheet(getSecondaryButtonStyle());
    selectAllBtn_->setFixedWidth(100);
    
    selectNoneBtn_ = new QPushButton("Clear");
    selectNoneBtn_->setStyleSheet(getSecondaryButtonStyle());
    selectNoneBtn_->setFixedWidth(80);
    
    selectNewBtn_ = new QPushButton("✨ New Only");
    selectNewBtn_->setStyleSheet(getSecondaryButtonStyle());
    selectNewBtn_->setFixedWidth(100);
    
    headerRow->addWidget(photoCountLabel_);
    headerRow->addStretch();
    headerRow->addWidget(selectAllBtn_);
    headerRow->addWidget(selectNoneBtn_);
    headerRow->addWidget(selectNewBtn_);
    
    // Photo grid
    photoList_ = new QListWidget();
    photoList_->setViewMode(QListView::IconMode);
    photoList_->setIconSize(QSize(140, 140));
    photoList_->setSpacing(12);
    photoList_->setResizeMode(QListView::Adjust);
    photoList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    photoList_->setDragEnabled(false);
    photoList_->setUniformItemSizes(true);
    photoList_->setStyleSheet(QString(R"(
        QListWidget {
            background: %1;
            border: 1px solid %2;
            border-radius: 12px;
            padding: 10px;
        }
        QListWidget::item {
            background: %3;
            border-radius: 10px;
            padding: 8px;
            margin: 4px;
        }
        QListWidget::item:selected {
            background: %4;
            border: 2px solid %5;
        }
        QListWidget::item:hover:!selected {
            background: %6;
        }
    )").arg(Colors::Surface, Colors::Border, Colors::SurfaceLight,
            Colors::Secondary, Colors::Accent, Colors::SurfaceLight));
    
    layout->addLayout(headerRow);
    layout->addWidget(photoList_);
}

void MainWindow::createPreviewPanel() {
    QGroupBox *group = new QGroupBox("👁 Preview", this);
    group->setObjectName("previewGroup");
    
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(15);
    
    // Preview container with aspect ratio
    QFrame *previewFrame = new QFrame();
    previewFrame->setStyleSheet(QString(R"(
        QFrame {
            background: #1a1a1a;
            border-radius: 12px;
        }
    )"));
    
    QVBoxLayout *previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    
    previewImage_ = new QLabel();
    previewImage_->setMinimumSize(400, 350);
    previewImage_->setAlignment(Qt::AlignCenter);
    previewImage_->setStyleSheet("color: " + Colors::TextMuted + "; font-size: 16px;");
    previewImage_->setText("Select a photo to preview");
    previewImage_->setScaledContents(false);
    
    previewLayout->addWidget(previewImage_);
    
    // Info card
    QFrame *infoCard = new QFrame();
    infoCard->setStyleSheet(QString(R"(
        QFrame {
            background: %1;
            border-radius: 10px;
            padding: 12px;
        }
    )").arg(Colors::SurfaceLight));
    
    QVBoxLayout *infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setSpacing(5);
    
    previewInfo_ = new QLabel();
    previewInfo_->setTextFormat(Qt::RichText);
    previewInfo_->setStyleSheet("font-size: 13px;");
    
    infoLayout->addWidget(previewInfo_);
    
    layout->addWidget(previewFrame, 1);
    layout->addWidget(infoCard);
}

void MainWindow::createTransferPanel() {
    QGroupBox *group = new QGroupBox("📤 Transfer", this);
    group->setObjectName("transferGroup");
    
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(15);
    
    // Destination row
    QHBoxLayout *destRow = new QHBoxLayout();
    
    QLabel *destIcon = new QLabel("📁");
    destIcon->setStyleSheet("font-size: 20px;");
    
    destinationEdit_ = new QLineEdit();
    destinationEdit_->setText(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/PhonePhotos");
    destinationEdit_->setPlaceholderText("Choose destination folder...");
    
    browseBtn_ = new QPushButton("Browse");
    browseBtn_->setStyleSheet(getSecondaryButtonStyle());
    browseBtn_->setFixedWidth(100);
    
    destRow->addWidget(destIcon);
    destRow->addWidget(destinationEdit_, 1);
    destRow->addWidget(browseBtn_);
    
    // Control buttons
    QHBoxLayout *ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(15);
    
    startBtn_ = new QPushButton("▶ Start Transfer");
    startBtn_->setStyleSheet(getStartButtonStyle());
    startBtn_->setMinimumHeight(50);
    
    pauseBtn_ = new QPushButton("⏸ Pause");
    pauseBtn_->setStyleSheet(getSecondaryButtonStyle());
    pauseBtn_->setEnabled(false);
    pauseBtn_->setMinimumHeight(50);
    
    cancelBtn_ = new QPushButton("⏹ Cancel");
    cancelBtn_->setStyleSheet(QString(R"(
        QPushButton {
            background: %1;
            color: %2;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 12px 25px;
            font-size: 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: %2;
            color: white;
        }
        QPushButton:disabled {
            background: %1;
            color: %3;
            border-color: %3;
        }
    )").arg(Colors::Surface, Colors::Primary, Colors::TextMuted));
    cancelBtn_->setEnabled(false);
    cancelBtn_->setMinimumHeight(50);
    
    ctrlRow->addWidget(startBtn_, 2);
    ctrlRow->addWidget(pauseBtn_, 1);
    ctrlRow->addWidget(cancelBtn_, 1);
    
    // Progress section
    QFrame *progressCard = new QFrame();
    progressCard->setStyleSheet(QString(R"(
        QFrame {
            background: %1;
            border-radius: 12px;
            padding: 15px;
        }
    )").arg(Colors::SurfaceLight));
    
    QVBoxLayout *progressLayout = new QVBoxLayout(progressCard);
    progressLayout->setSpacing(12);
    
    // Overall progress
    QHBoxLayout *overallRow = new QHBoxLayout();
    QLabel *overallLabel = new QLabel("Overall Progress");
    overallLabel->setStyleSheet("font-weight: 600;");
    overallProgressLabel_ = new QLabel("0%");
    overallProgressLabel_->setStyleSheet("color: " + Colors::Accent + "; font-weight: bold;");
    overallRow->addWidget(overallLabel);
    overallRow->addStretch();
    overallRow->addWidget(overallProgressLabel_);
    
    overallProgress_ = new QProgressBar();
    overallProgress_->setTextVisible(false);
    overallProgress_->setFixedHeight(16);
    overallProgress_->setStyleSheet(QString(R"(
        QProgressBar {
            background: %1;
            border-radius: 8px;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %2, stop:1 %3);
            border-radius: 8px;
        }
    )").arg(Colors::Surface, Colors::Primary, Colors::Accent));
    
    // Current file
    transferStatusLabel_ = new QLabel("Ready to transfer");
    transferStatusLabel_->setStyleSheet("color: " + Colors::TextMuted + ";");
    transferStatusLabel_->setWordWrap(true);
    
    // Stats row
    QHBoxLayout *statsRow = new QHBoxLayout();
    
    speedLabel_ = new QLabel("");
    speedLabel_->setStyleSheet(QString("color: %1; font-weight: 600; font-size: 15px;").arg(Colors::Success));
    
    etaLabel_ = new QLabel("");
    etaLabel_->setStyleSheet(QString("color: %1; font-size: 14px;").arg(Colors::TextMuted));
    
    statsRow->addWidget(speedLabel_);
    statsRow->addStretch();
    statsRow->addWidget(etaLabel_);
    
    progressLayout->addLayout(overallRow);
    progressLayout->addWidget(overallProgress_);
    progressLayout->addWidget(transferStatusLabel_);
    progressLayout->addLayout(statsRow);
    
    // iOS HEIC conversion options (hidden by default)
    heicOptionsWidget_ = new QWidget();
    heicOptionsWidget_->setVisible(false);
    
    QFrame *heicCard = new QFrame(heicOptionsWidget_);
    heicCard->setStyleSheet(QString(R"(
        QFrame {
            background: %1;
            border: 2px solid %2;
            border-radius: 10px;
            padding: 12px;
        }
    )").arg(Colors::SurfaceLight, Colors::Warning));
    
    QVBoxLayout *heicCardLayout = new QVBoxLayout(heicCard);
    heicCardLayout->setSpacing(10);
    
    QLabel *heicTitle = new QLabel("🍎 Apple Photos Detected");
    heicTitle->setStyleSheet(QString("font-weight: bold; font-size: 14px; color: %1;").arg(Colors::Text));
    
    convertHeicCheck_ = new QCheckBox("Convert HEIC/HEIF photos to JPEG");
    convertHeicCheck_->setChecked(SettingsDialog::shouldConvertHeic());
    convertHeicCheck_->setStyleSheet(QString(R"(
        QCheckBox {
            color: %1;
            font-size: 13px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 2px solid %2;
        }
        QCheckBox::indicator:checked {
            background: %3;
            border-color: %3;
        }
    )").arg(Colors::Text, Colors::Border, Colors::Success));
    
    QHBoxLayout *qualityRow = new QHBoxLayout();
    QLabel *qualityLabel = new QLabel("Quality:");
    qualityLabel->setStyleSheet(QString("color: %1;").arg(Colors::TextMuted));
    
    jpegQualityCombo_ = new QComboBox();
    jpegQualityCombo_->addItem("Maximum (100%)", 100);
    jpegQualityCombo_->addItem("High (90%)", 90);
    jpegQualityCombo_->addItem("Medium (75%)", 75);
    jpegQualityCombo_->setCurrentIndex(1);
    jpegQualityCombo_->setFixedWidth(150);
    
    qualityRow->addWidget(qualityLabel);
    qualityRow->addWidget(jpegQualityCombo_);
    qualityRow->addStretch();
    
    heicCardLayout->addWidget(heicTitle);
    heicCardLayout->addWidget(convertHeicCheck_);
    heicCardLayout->addLayout(qualityRow);
    
    QVBoxLayout *heicLayout = new QVBoxLayout(heicOptionsWidget_);
    heicLayout->setContentsMargins(0, 0, 0, 0);
    heicLayout->addWidget(heicCard);
    
    layout->addLayout(destRow);
    layout->addWidget(heicOptionsWidget_);
    layout->addLayout(ctrlRow);
    layout->addWidget(progressCard);
}

void MainWindow::setupMenuBar() {
    QMenuBar *menuBar = this->menuBar();
    
    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("📂 Open Destination", this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(destinationEdit_->text()));
    });
    fileMenu->addSeparator();
    fileMenu->addAction("⚙ Settings", this, &MainWindow::onOpenSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("🚪 Exit", this, &QWidget::close);
    
    // Device menu
    QMenu *deviceMenu = menuBar->addMenu("&Device");
    deviceMenu->addAction("↻ Refresh", this, &MainWindow::onRefreshDevices);
    deviceMenu->addAction("🔗 Connect", this, &MainWindow::onConnectDevice);
    deviceMenu->addAction("⏏ Disconnect", this, &MainWindow::onDisconnectDevice);
    
    // Transfer menu
    QMenu *transferMenu = menuBar->addMenu("&Transfer");
    transferMenu->addAction("▶ Start", this, &MainWindow::onStartTransfer);
    transferMenu->addAction("⏸ Pause", this, &MainWindow::onPauseTransfer);
    transferMenu->addAction("⏹ Cancel", this, &MainWindow::onCancelTransfer);
    transferMenu->addSeparator();
    transferMenu->addAction("🔄 Resume Incomplete", this, &MainWindow::onResumeIncomplete);
    
    // Help menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("ℹ About", this, &MainWindow::onAbout);
    helpMenu->addAction("About Qt", qApp, &QApplication::aboutQt);
}

void MainWindow::setupStatusBar() {
    statusLabel_ = new QLabel("Ready");
    statusLabel_->setStyleSheet("padding: 5px;");
    statusBar()->addWidget(statusLabel_);
}

void MainWindow::setupConnections() {
    connect(refreshBtn_, &QPushButton::clicked, this, &MainWindow::onRefreshDevices);
    connect(connectBtn_, &QPushButton::clicked, this, [this]() {
        if (isConnected_) {
            onDisconnectDevice();
        } else {
            onConnectDevice();
        }
    });
    
    connect(selectAllBtn_, &QPushButton::clicked, this, &MainWindow::onSelectAll);
    connect(selectNoneBtn_, &QPushButton::clicked, this, &MainWindow::onSelectNone);
    connect(selectNewBtn_, &QPushButton::clicked, this, &MainWindow::onSelectNew);
    connect(photoList_, &QListWidget::itemClicked, this, &MainWindow::onPreviewPhoto);
    connect(photoList_, &QListWidget::itemDoubleClicked, this, &MainWindow::onPreviewPhoto);
    
    connect(browseBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseDestination);
    connect(startBtn_, &QPushButton::clicked, this, &MainWindow::onStartTransfer);
    connect(pauseBtn_, &QPushButton::clicked, this, &MainWindow::onPauseTransfer);
    connect(cancelBtn_, &QPushButton::clicked, this, &MainWindow::onCancelTransfer);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (isTransferring_) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Transfer in Progress");
        msgBox.setText("A transfer is in progress.");
        msgBox.setInformativeText("Do you want to save and exit? You can resume later.");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msgBox.setStyleSheet(getMessageBoxStyle());
        
        int reply = msgBox.exec();
        
        if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        
        if (reply == QMessageBox::Yes) {
            transferQueue_->saveState(stateFilePath_.toStdString());
        }
        
        transferQueue_->cancel();
    }
    
    saveSettings();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            QString path = urls.first().toLocalFile();
            if (QDir(path).exists()) {
                destinationEdit_->setText(path);
            }
        }
    }
}

void MainWindow::onRefreshDevices() {
    statusLabel_->setText("🔍 Searching for devices...");
    deviceCombo_->clear();
    deviceCombo_->addItem("🔍 Searching...");
    
#ifdef ENABLE_ANDROID
    #ifdef USE_WPD
    auto mtp = std::make_unique<WPDHandler>();
    #else
    auto mtp = std::make_unique<MTPHandler>();
    #endif
    if (mtp->detectDevices()) {
        deviceCombo_->clear();
        deviceCombo_->addItem("📱 Android: " + QString::fromStdString(mtp->getDeviceName()), "android");
    }
#endif
    
#ifdef ENABLE_IOS
    auto ios = std::make_unique<iOSHandler>();
    if (ios->detectDevices()) {
        if (deviceCombo_->count() == 1 && deviceCombo_->itemText(0).startsWith("🔍")) {
            deviceCombo_->clear();
        }
        deviceCombo_->addItem("🍎 iOS: " + QString::fromStdString(ios->getDeviceName()), "ios");
    }
#endif
    
    if (deviceCombo_->count() == 0 || deviceCombo_->itemText(0).startsWith("🔍")) {
        deviceCombo_->clear();
        deviceCombo_->addItem("❌ No devices found");
        statusLabel_->setText("No devices found. Connect a device and try again.");
    } else {
        statusLabel_->setText(QString("✓ Found %1 device(s)").arg(deviceCombo_->count()));
    }
}

void MainWindow::onDeviceSelected(int index) {
    Q_UNUSED(index);
}

void MainWindow::onConnectDevice() {
    if (deviceCombo_->currentData().isNull()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("No Device");
        msgBox.setText("Please select a device to connect.");
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }
    
    QString deviceType = deviceCombo_->currentData().toString();
    statusLabel_->setText("🔗 Connecting...");
    
#ifdef ENABLE_ANDROID
    if (deviceType == "android") {
    #ifdef USE_WPD
        deviceHandler_ = std::make_unique<WPDHandler>();
    #else
        deviceHandler_ = std::make_unique<MTPHandler>();
    #endif
    }
#endif
    
#ifdef ENABLE_IOS
    if (deviceType == "ios") {
        deviceHandler_ = std::make_unique<iOSHandler>();
    }
#endif
    
    if (!deviceHandler_) {
        QMessageBox::critical(this, "Error", "Failed to create device handler.");
        return;
    }
    
    if (!deviceHandler_->detectDevices()) {
        QMessageBox::critical(this, "Error", "Device not found. Please reconnect and try again.");
        return;
    }
    
    if (!deviceHandler_->connectToDevice()) {
        QMessageBox::critical(this, "Error", 
            QString("Failed to connect: %1").arg(QString::fromStdString(deviceHandler_->getLastError())));
        return;
    }
    
    isConnected_ = true;
    connectBtn_->setText("⏏ Disconnect");
    connectBtn_->setStyleSheet(getDisconnectButtonStyle());
    
    // Update device info
    deviceInfoLabel_->setText(QString("<span style='font-size:18px;font-weight:bold;'>%1</span><br>"
                                      "<span style='color:%2;'>%3 %4</span>")
        .arg(QString::fromStdString(deviceHandler_->getDeviceName()))
        .arg(Colors::TextMuted)
        .arg(QString::fromStdString(deviceHandler_->getDeviceManufacturer()))
        .arg(QString::fromStdString(deviceHandler_->getDeviceModel())));
    
    // Get storage info
    auto storages = deviceHandler_->getStorageInfo();
    if (!storages.empty()) {
        auto &s = storages[0];
        double usedPct = 100.0 * (s.max_capacity - s.free_space) / s.max_capacity;
        storageProgress_->setValue(static_cast<int>(usedPct));
        storageLabel_->setText(QString("%1 / %2 used")
            .arg(formatSize(s.max_capacity - s.free_space))
            .arg(formatSize(s.max_capacity)));
    }
    
    statusLabel_->setText("✓ Connected. Scanning media...");
    
    // Enumerate media
    mediaList_ = deviceHandler_->enumerateMedia();
    photoCountLabel_->setText(QString("<span style='font-size:24px;font-weight:bold;color:%1;'>%2</span>"
                                      "<span style='color:%3;'> items</span>")
                              .arg(Colors::Accent).arg(mediaList_.size()).arg(Colors::TextMuted));
    updatePhotoList();
    loadThumbnailsAsync();
    
    statusLabel_->setText(QString("✓ Found %1 photos/videos").arg(mediaList_.size()));
    
    // Show HEIC conversion options for iOS devices
    if (deviceType == "ios") {
        int heicCount = 0;
        for (const auto &media : mediaList_) {
            QString filename = QString::fromStdString(media.filename).toLower();
            if (filename.endsWith(".heic") || filename.endsWith(".heif")) {
                heicCount++;
            }
        }
        
        if (heicCount > 0) {
            heicOptionsWidget_->setVisible(true);
            // Update the title to show count
            QLabel *title = heicOptionsWidget_->findChild<QLabel*>();
            if (title) {
                title->setText(QString("🍎 %1 Apple HEIC Photos Detected").arg(heicCount));
            }
        }
    } else {
        heicOptionsWidget_->setVisible(false);
    }
    
    // Start loading thumbnails
    deviceWorker_->setDeviceHandler(deviceHandler_.get());
    thumbnailLoader_->setDeviceHandler(deviceHandler_.get());
}

void MainWindow::onDisconnectDevice() {
    if (deviceHandler_) {
        deviceHandler_->disconnect();
        deviceHandler_.reset();
    }
    
    isConnected_ = false;
    connectBtn_->setText("🔗 Connect");
    connectBtn_->setStyleSheet(getConnectButtonStyle());
    deviceInfoLabel_->setText("<span style='color:" + Colors::TextMuted + ";'>No device connected</span>");
    storageLabel_->setText("");
    storageProgress_->setValue(0);
    photoList_->clear();
    mediaList_.clear();
    previewImage_->clear();
    previewImage_->setText("Select a photo to preview");
    previewInfo_->clear();
    heicOptionsWidget_->setVisible(false);
    
    statusLabel_->setText("Disconnected");
}

void MainWindow::onSelectAll() {
    photoList_->selectAll();
}

void MainWindow::onSelectNone() {
    photoList_->clearSelection();
}

void MainWindow::onSelectNew() {
    photoList_->clearSelection();
    photoList_->selectAll();
}

void MainWindow::onPreviewPhoto(QListWidgetItem *item) {
    int index = photoList_->row(item);
    if (index < 0 || index >= static_cast<int>(mediaList_.size())) {
        return;
    }
    
    const auto &media = mediaList_[index];
    
    // Load full image for preview
    std::vector<uint8_t> data;
    if (deviceHandler_ && deviceHandler_->readFile(media.object_id, data)) {
        QPixmap pixmap;
        pixmap.loadFromData(data.data(), data.size());
        
        if (!pixmap.isNull()) {
            previewImage_->setPixmap(pixmap.scaled(previewImage_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    
    // Show info
    QDateTime dt = QDateTime::fromSecsSinceEpoch(media.modification_date);
    previewInfo_->setText(QString(
        "<span style='font-size:16px;font-weight:bold;'>%1</span><br>"
        "<span style='color:%5;'>📦 Size:</span> %2<br>"
        "<span style='color:%5;'>📅 Date:</span> %3<br>"
        "<span style='color:%5;'>📄 Type:</span> %4"
    ).arg(QString::fromStdString(media.filename))
     .arg(formatSize(media.file_size))
     .arg(dt.toString("yyyy-MM-dd hh:mm:ss"))
     .arg(QString::fromStdString(media.mime_type))
     .arg(Colors::TextMuted));
}

void MainWindow::onBrowseDestination() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Destination Folder",
        destinationEdit_->text(),
        QFileDialog::ShowDirsOnly
    );
    
    if (!dir.isEmpty()) {
        destinationEdit_->setText(dir);
    }
}

void MainWindow::onStartTransfer() {
    if (!isConnected_) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Not Connected");
        msgBox.setText("Please connect a device first.");
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }
    
    QList<QListWidgetItem*> selected = photoList_->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("No Selection");
        msgBox.setText("Please select photos to transfer.");
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }
    
    QString dest = destinationEdit_->text();
    if (dest.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("No Destination");
        msgBox.setText("Please select a destination folder.");
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }
    
    QDir().mkpath(dest);
    
    transferQueue_->clear();
    transferQueue_->setDestinationFolder(dest.toStdString());
    transferQueue_->setDeviceHandler(deviceHandler_.get());
    
    for (auto *item : selected) {
        int idx = photoList_->row(item);
        if (idx >= 0 && idx < static_cast<int>(mediaList_.size())) {
            transferQueue_->addItem(mediaList_[idx]);
        }
    }
    
    isTransferring_ = true;
    startBtn_->setEnabled(false);
    pauseBtn_->setEnabled(true);
    cancelBtn_->setEnabled(true);
    
    overallProgress_->setValue(0);
    overallProgress_->setMaximum(selected.size());
    overallProgressLabel_->setText("0%");
    
    transferStatusLabel_->setText("Starting transfer...");
    statusLabel_->setText("📤 Transferring...");
    
    transferQueue_->setProgressCallback([this](const TransferStats &stats) {
        QMetaObject::invokeMethod(this, [this, stats]() {
            onTransferProgress(stats.completed, stats.total_items, 
                              stats.transferred_bytes, stats.total_bytes,
                              QString::fromStdString(stats.current_file));
        }, Qt::QueuedConnection);
    });
    
    QThread *transferThread = QThread::create([this]() {
        transferQueue_->start();
        QMetaObject::invokeMethod(this, &MainWindow::onTransferComplete, Qt::QueuedConnection);
    });
    
    connect(transferThread, &QThread::finished, transferThread, &QThread::deleteLater);
    transferThread->start();
}

void MainWindow::onPauseTransfer() {
    if (transferQueue_->isPaused()) {
        transferQueue_->resume();
        pauseBtn_->setText("⏸ Pause");
        statusLabel_->setText("📤 Transferring...");
    } else {
        transferQueue_->pause();
        pauseBtn_->setText("▶ Resume");
        statusLabel_->setText("⏸ Paused");
        transferQueue_->saveState(stateFilePath_.toStdString());
    }
}

void MainWindow::onCancelTransfer() {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Cancel Transfer");
    msgBox.setText("Are you sure you want to cancel?");
    msgBox.setInformativeText("You can save progress to resume later.");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setStyleSheet(getMessageBoxStyle());
    
    if (msgBox.exec() == QMessageBox::Yes) {
        transferQueue_->saveState(stateFilePath_.toStdString());
        transferQueue_->cancel();
        
        isTransferring_ = false;
        startBtn_->setEnabled(true);
        pauseBtn_->setEnabled(false);
        cancelBtn_->setEnabled(false);
        pauseBtn_->setText("⏸ Pause");
        
        statusLabel_->setText("⏹ Cancelled. Progress saved.");
    }
}

void MainWindow::onResumeIncomplete() {
    if (!QFile::exists(stateFilePath_)) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("No Incomplete Transfer");
        msgBox.setText("No incomplete transfer found.");
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }
    
    if (!transferQueue_->loadState(stateFilePath_.toStdString())) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Error");
        msgBox.setText("Failed to load transfer state.");
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }
    
    statusLabel_->setText("🔄 Resuming transfer...");
}

void MainWindow::onTransferProgress(int completed, int total, qint64 bytes, qint64 totalBytes, QString currentFile) {
    overallProgress_->setValue(completed);
    overallProgress_->setMaximum(total);
    
    int pct = total > 0 ? (completed * 100 / total) : 0;
    overallProgressLabel_->setText(QString("%1%").arg(pct));
    
    transferStatusLabel_->setText(QString("📄 %1 (%2/%3)")
        .arg(currentFile)
        .arg(completed)
        .arg(total));
    
    static qint64 lastBytes = 0;
    static QDateTime lastTime = QDateTime::currentDateTime();
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 elapsed = lastTime.msecsTo(now);
    
    if (elapsed > 500) {
        double speed = (bytes - lastBytes) / (elapsed / 1000.0);
        speedLabel_->setText(QString("⚡ %1/s").arg(formatSize(static_cast<qint64>(speed))));
        
        if (speed > 0 && totalBytes > bytes) {
            int eta = static_cast<int>((totalBytes - bytes) / speed);
            etaLabel_->setText(QString("⏱ %1 remaining").arg(formatTime(eta)));
        }
        
        lastBytes = bytes;
        lastTime = now;
    }
}

void MainWindow::onTransferComplete() {
    isTransferring_ = false;
    startBtn_->setEnabled(true);
    pauseBtn_->setEnabled(false);
    cancelBtn_->setEnabled(false);
    pauseBtn_->setText("⏸ Pause");
    
    auto stats = transferQueue_->getStats();
    
    overallProgress_->setValue(overallProgress_->maximum());
    overallProgressLabel_->setText("100%");
    transferStatusLabel_->setText(QString("✅ Complete! %1 transferred, %2 skipped, %3 failed")
        .arg(stats.completed)
        .arg(stats.skipped)
        .arg(stats.failed));
    
    statusLabel_->setText("✅ Transfer complete!");
    
    QFile::remove(stateFilePath_);
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("🎉 Transfer Complete");
    msgBox.setText(QString("Successfully transferred %1 files!").arg(stats.completed));
    msgBox.setInformativeText(QString("📦 Total: %1\n⏭ Skipped: %2\n❌ Failed: %3")
        .arg(formatSize(stats.transferred_bytes))
        .arg(stats.skipped)
        .arg(stats.failed));
    msgBox.setStyleSheet(getMessageBoxStyle());
    msgBox.exec();
}

void MainWindow::onTransferError(QString error) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Transfer Error");
    msgBox.setText(error);
    msgBox.setStyleSheet(getMessageBoxStyle());
    msgBox.exec();
    
    statusLabel_->setText("❌ Error: " + error);
}

void MainWindow::onThumbnailLoaded(int index, QIcon thumbnail) {
    if (index < photoList_->count()) {
        photoList_->item(index)->setIcon(thumbnail);
    }
}

void MainWindow::onOpenSettings() {
    SettingsDialog dialog(this);
    
    connect(&dialog, &SettingsDialog::themeChanged, this, [this](const Theme &theme) {
        Colors::loadFromTheme(theme);
        setStyleSheet(getGlobalStyle());
        // Refresh UI elements that need color updates
        connectBtn_->setStyleSheet(isConnected_ ? getDisconnectButtonStyle() : getConnectButtonStyle());
        startBtn_->setStyleSheet(getStartButtonStyle());
        pauseBtn_->setStyleSheet(getSecondaryButtonStyle());
        refreshBtn_->setStyleSheet(getSecondaryButtonStyle());
        browseBtn_->setStyleSheet(getSecondaryButtonStyle());
        selectAllBtn_->setStyleSheet(getSecondaryButtonStyle());
        selectNoneBtn_->setStyleSheet(getSecondaryButtonStyle());
        selectNewBtn_->setStyleSheet(getSecondaryButtonStyle());
    });
    
    dialog.exec();
}

void MainWindow::onAbout() {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("About Photo Transfer");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(QString(
        "<div style='text-align:center;'>"
        "<h2 style='color:%1;'>📱 Photo Transfer</h2>"
        "<p style='font-size:14px;'>Version 1.0.0</p>"
        "<p>A modern, cross-platform application for transferring<br>"
        "photos and videos from mobile devices.</p>"
        "<hr style='border-color:%2;'>"
        "<p><b>Features:</b></p>"
        "<p>✓ Android & iOS support<br>"
        "✓ SHA256 deduplication<br>"
        "✓ Resume interrupted transfers<br>"
        "✓ Photo preview & thumbnails</p>"
        "<hr style='border-color:%2;'>"
        "<p style='color:%3;'>© 2026 Photo Transfer</p>"
        "</div>"
    ).arg(Colors::Accent, Colors::Border, Colors::TextMuted));
    msgBox.setStyleSheet(getMessageBoxStyle());
    msgBox.exec();
}

void MainWindow::updatePhotoList() {
    photoList_->clear();
    
    for (size_t i = 0; i < mediaList_.size(); i++) {
        const auto &media = mediaList_[i];
        
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(QString::fromStdString(media.filename));
        item->setToolTip(QString("%1\n%2\n%3")
            .arg(QString::fromStdString(media.filename))
            .arg(formatSize(media.file_size))
            .arg(QString::fromStdString(media.path)));
        
        // Create gradient placeholder
        QPixmap placeholder(140, 140);
        placeholder.fill(QColor(Colors::SurfaceLight));
        
        QPainter painter(&placeholder);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Draw loading indicator
        painter.setPen(QColor(Colors::TextMuted));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);
        painter.drawText(placeholder.rect(), Qt::AlignCenter, "Loading...");
        
        item->setIcon(QIcon(placeholder));
        item->setSizeHint(QSize(160, 180));
        
        photoList_->addItem(item);
    }
}

void MainWindow::loadThumbnailsAsync() {
    if (!deviceHandler_ || mediaList_.empty()) return;
    
    // Load thumbnails in batches
    QThread *thumbThread = QThread::create([this]() {
        for (size_t i = 0; i < mediaList_.size(); i++) {
            if (!deviceHandler_ || !deviceHandler_->isConnected()) break;
            
            const auto &media = mediaList_[i];
            std::vector<uint8_t> data;
            
            if (deviceHandler_->readFile(media.object_id, data)) {
                QPixmap pixmap;
                pixmap.loadFromData(data.data(), data.size());
                
                if (!pixmap.isNull()) {
                    // Create thumbnail with rounded corners
                    QPixmap thumbnail(140, 140);
                    thumbnail.fill(Qt::transparent);
                    
                    QPainter painter(&thumbnail);
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setRenderHint(QPainter::SmoothPixmapTransform);
                    
                    // Create rounded rect path
                    QPainterPath path;
                    path.addRoundedRect(thumbnail.rect(), 10, 10);
                    painter.setClipPath(path);
                    
                    // Scale and center image
                    QPixmap scaled = pixmap.scaled(140, 140, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                    int x = (140 - scaled.width()) / 2;
                    int y = (140 - scaled.height()) / 2;
                    painter.drawPixmap(x, y, scaled);
                    
                    QMetaObject::invokeMethod(this, [this, i, thumbnail]() {
                        if (static_cast<int>(i) < photoList_->count()) {
                            photoList_->item(i)->setIcon(QIcon(thumbnail));
                        }
                    }, Qt::QueuedConnection);
                }
            }
            
            // Small delay to prevent UI freezing
            if (i % 10 == 0) {
                QThread::msleep(10);
            }
        }
    });
    
    connect(thumbThread, &QThread::finished, thumbThread, &QThread::deleteLater);
    thumbThread->start();
}

void MainWindow::loadSettings() {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    destinationEdit_->setText(settings.value("destination", 
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/PhonePhotos").toString());
}

void MainWindow::saveSettings() {
    QSettings settings("PhotoTransfer", "PhotoTransfer");
    
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("destination", destinationEdit_->text());
}

QString MainWindow::formatSize(qint64 bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', unit > 0 ? 2 : 0).arg(units[unit]);
}

QString MainWindow::formatTime(int seconds) {
    if (seconds < 60) {
        return QString("%1s").arg(seconds);
    } else if (seconds < 3600) {
        return QString("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    } else {
        return QString("%1h %2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
    }
}

// DeviceWorker implementation
DeviceWorker::DeviceWorker(QObject *parent) : QObject(parent) {}

void DeviceWorker::refreshDevices() {
    QStringList devices;
    emit devicesFound(devices);
}

void DeviceWorker::connectToDevice(QString deviceType) {
    Q_UNUSED(deviceType);
}

void DeviceWorker::disconnectDevice() {
    if (handler_) {
        handler_->disconnect();
    }
    emit disconnected();
}

void DeviceWorker::enumerateMedia() {
    if (!handler_) return;
    
    auto media = handler_->enumerateMedia();
    emit mediaEnumerated(media.size());
}

void DeviceWorker::performTransfer(QList<int> items, QString destination, std::vector<MediaInfo> *mediaList) {
    Q_UNUSED(items);
    Q_UNUSED(destination);
    Q_UNUSED(mediaList);
}

// ThumbnailLoader implementation
ThumbnailLoader::ThumbnailLoader(QObject *parent) : QObject(parent) {}

void ThumbnailLoader::loadThumbnail(int index, uint32_t objectId, QString filename) {
    if (!handler_) return;
    
    std::vector<uint8_t> data;
    if (handler_->readFile(objectId, data)) {
        QIcon icon = createThumbnailFromData(data, filename);
        emit thumbnailLoaded(index, icon);
    }
}

void ThumbnailLoader::loadThumbnails(std::vector<MediaInfo> *mediaList) {
    if (!handler_ || !mediaList) return;
    
    for (size_t i = 0; i < mediaList->size(); i++) {
        const auto &media = (*mediaList)[i];
        loadThumbnail(i, media.object_id, QString::fromStdString(media.filename));
    }
    
    emit allThumbnailsLoaded();
}

QIcon ThumbnailLoader::createThumbnailFromData(const std::vector<uint8_t> &data, const QString &filename) {
    Q_UNUSED(filename);
    
    QPixmap pixmap;
    pixmap.loadFromData(data.data(), data.size());
    
    if (pixmap.isNull()) {
        pixmap = QPixmap(140, 140);
        pixmap.fill(QColor(Colors::SurfaceLight));
    } else {
        pixmap = pixmap.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    return QIcon(pixmap);
}
