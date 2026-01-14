#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <memory>

#include "../src/device_handler.h"
#include "../src/transfer_queue.h"
#include "../src/photo_db.h"
#include "settingsdialog.h"

class DeviceWorker;
class ThumbnailLoader;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // Device operations
    void onRefreshDevices();
    void onDeviceSelected(int index);
    void onConnectDevice();
    void onDisconnectDevice();
    
    // Photo operations
    void onSelectAll();
    void onSelectNone();
    void onSelectNew();
    void onPreviewPhoto(QListWidgetItem *item);
    
    // Transfer operations
    void onBrowseDestination();
    void onStartTransfer();
    void onPauseTransfer();
    void onCancelTransfer();
    void onResumeIncomplete();
    
    // Progress updates
    void onTransferProgress(int completed, int total, qint64 bytes, qint64 totalBytes, QString currentFile);
    void onTransferComplete();
    void onTransferError(QString error);
    
    // Thumbnail loading
    void onThumbnailLoaded(int index, QIcon thumbnail);
    
    // Settings
    void onOpenSettings();
    void onAbout();

signals:
    void requestDeviceRefresh();
    void requestConnect(QString deviceType);
    void requestDisconnect();
    void requestEnumerate();
    void requestTransfer(QList<int> selectedItems, QString destination);

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();
    
    void createDevicePanel();
    void createPhotoPanel();
    void createTransferPanel();
    void createPreviewPanel();
    
    void updateDeviceInfo();
    void updatePhotoList();
    void updateTransferStatus();
    void loadSettings();
    void saveSettings();
    void loadThumbnailsAsync();
    
    QString formatSize(qint64 bytes);
    QString formatTime(int seconds);
    
    // Style helpers
    QString getGlobalStyle();
    QString getConnectButtonStyle();
    QString getDisconnectButtonStyle();
    QString getStartButtonStyle();
    QString getSecondaryButtonStyle();
    QString getMessageBoxStyle();
    
    // UI Components
    // Device panel
    QComboBox *deviceCombo_;
    QPushButton *refreshBtn_;
    QPushButton *connectBtn_;
    QLabel *deviceInfoLabel_;
    QLabel *storageLabel_;
    QProgressBar *storageProgress_;
    
    // Photo panel
    QListWidget *photoList_;
    QPushButton *selectAllBtn_;
    QPushButton *selectNoneBtn_;
    QPushButton *selectNewBtn_;
    QLabel *photoCountLabel_;
    
    // Preview panel
    QLabel *previewImage_;
    QLabel *previewInfo_;
    
    // Transfer panel
    QLineEdit *destinationEdit_;
    QPushButton *browseBtn_;
    QPushButton *startBtn_;
    QPushButton *pauseBtn_;
    QPushButton *cancelBtn_;
    QProgressBar *overallProgress_;
    QLabel *overallProgressLabel_;
    QLabel *transferStatusLabel_;
    QLabel *speedLabel_;
    QLabel *etaLabel_;
    
    // iOS options (shown when iOS device connected)
    QWidget *heicOptionsWidget_;
    QCheckBox *convertHeicCheck_;
    QComboBox *jpegQualityCombo_;
    
    // Status bar
    QLabel *statusLabel_;
    
    // Backend
    std::unique_ptr<DeviceHandler> deviceHandler_;
    std::unique_ptr<TransferQueue> transferQueue_;
    std::unique_ptr<PhotoDB> database_;
    std::vector<MediaInfo> mediaList_;
    
    // Workers
    QThread *workerThread_;
    DeviceWorker *deviceWorker_;
    ThumbnailLoader *thumbnailLoader_;
    QThread *thumbnailThread_;
    
    // State
    bool isConnected_ = false;
    bool isTransferring_ = false;
    QString currentDestination_;
    QString stateFilePath_;
};

/**
 * Worker class for device operations (runs in separate thread)
 */
class DeviceWorker : public QObject {
    Q_OBJECT

public:
    explicit DeviceWorker(QObject *parent = nullptr);
    void setDeviceHandler(DeviceHandler *handler) { handler_ = handler; }

public slots:
    void refreshDevices();
    void connectToDevice(QString deviceType);
    void disconnectDevice();
    void enumerateMedia();
    void performTransfer(QList<int> items, QString destination, std::vector<MediaInfo> *mediaList);

signals:
    void devicesFound(QStringList devices);
    void connected(QString deviceName, QString manufacturer, QString model);
    void disconnected();
    void storageInfo(qint64 total, qint64 free);
    void mediaEnumerated(int count);
    void transferProgress(int completed, int total, qint64 bytes, qint64 totalBytes, QString currentFile);
    void transferComplete();
    void error(QString message);

private:
    DeviceHandler *handler_ = nullptr;
};

/**
 * Thumbnail loader (runs in separate thread)
 */
class ThumbnailLoader : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailLoader(QObject *parent = nullptr);
    void setDeviceHandler(DeviceHandler *handler) { handler_ = handler; }

public slots:
    void loadThumbnail(int index, uint32_t objectId, QString filename);
    void loadThumbnails(std::vector<MediaInfo> *mediaList);

signals:
    void thumbnailLoaded(int index, QIcon thumbnail);
    void allThumbnailsLoaded();

private:
    DeviceHandler *handler_ = nullptr;
    QIcon createThumbnailFromData(const std::vector<uint8_t> &data, const QString &filename);
};

#endif // MAINWINDOW_H
