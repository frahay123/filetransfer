#include <QApplication>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QDir>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Application info
    app.setApplicationName("Photo Transfer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("PhotoTransfer");
    app.setOrganizationDomain("phototransfer.app");
    
    // Set modern style
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Modern color palette
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(250, 250, 250));
    palette.setColor(QPalette::WindowText, QColor(30, 30, 30));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    palette.setColor(QPalette::ToolTipText, QColor(30, 30, 30));
    palette.setColor(QPalette::Text, QColor(30, 30, 30));
    palette.setColor(QPalette::Button, QColor(240, 240, 240));
    palette.setColor(QPalette::ButtonText, QColor(30, 30, 30));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(74, 144, 217));
    palette.setColor(QPalette::Highlight, QColor(74, 144, 217));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);
    
    // Global stylesheet for modern look
    app.setStyleSheet(R"(
        QMainWindow {
            background: #fafafa;
        }
        
        QGroupBox {
            font-weight: bold;
            border: 1px solid #ddd;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 10px;
            background: white;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 5px;
            color: #333;
        }
        
        QPushButton {
            background: #f0f0f0;
            border: 1px solid #ccc;
            border-radius: 5px;
            padding: 8px 16px;
            min-width: 80px;
        }
        
        QPushButton:hover {
            background: #e0e0e0;
            border-color: #bbb;
        }
        
        QPushButton:pressed {
            background: #d0d0d0;
        }
        
        QPushButton:disabled {
            background: #f5f5f5;
            color: #999;
        }
        
        QLineEdit {
            border: 1px solid #ccc;
            border-radius: 5px;
            padding: 8px;
            background: white;
        }
        
        QLineEdit:focus {
            border-color: #4a90d9;
        }
        
        QComboBox {
            border: 1px solid #ccc;
            border-radius: 5px;
            padding: 8px;
            background: white;
        }
        
        QComboBox:hover {
            border-color: #bbb;
        }
        
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        
        QToolBar {
            background: #f5f5f5;
            border-bottom: 1px solid #ddd;
            spacing: 5px;
            padding: 5px;
        }
        
        QStatusBar {
            background: #f5f5f5;
            border-top: 1px solid #ddd;
        }
        
        QMenuBar {
            background: #f5f5f5;
            border-bottom: 1px solid #ddd;
        }
        
        QMenuBar::item:selected {
            background: #e0e0e0;
        }
        
        QMenu {
            background: white;
            border: 1px solid #ddd;
        }
        
        QMenu::item:selected {
            background: #4a90d9;
            color: white;
        }
        
        QScrollBar:vertical {
            background: #f0f0f0;
            width: 12px;
            border-radius: 6px;
        }
        
        QScrollBar::handle:vertical {
            background: #c0c0c0;
            border-radius: 6px;
            min-height: 30px;
        }
        
        QScrollBar::handle:vertical:hover {
            background: #a0a0a0;
        }
    )");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
