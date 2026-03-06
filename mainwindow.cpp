#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QScreen>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QRegularExpression>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), updateProcess(new QProcess(this)),
      transactionPhaseStarted(false), isUpToDate(false), isTerminatingIntentionally(false)
{
    setupUi();
    setupStyle();

    // Automatically resize to exactly 66% of the primary screen
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        int width = screenGeometry.width() * 0.66;
        int height = screenGeometry.height() * 0.66;
        resize(width, height);
    }

    // Connect standard process streams and status signals
    connect(updateProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::handleUpdateOutput);
    connect(updateProcess, &QProcess::readyReadStandardError, this, &MainWindow::handleUpdateErrorOutput);
    connect(updateProcess, &QProcess::finished, this, &MainWindow::handleUpdateFinished);
    connect(updateProcess, &QProcess::errorOccurred, this, &MainWindow::handleUpdateProcessError);
}

MainWindow::~MainWindow()
{
    // CLEANUP: Ensure the QProcess is properly deleted/cleaned up if the main window is closed
    if (updateProcess && updateProcess->state() != QProcess::NotRunning) {
        updateProcess->terminate();
        if (!updateProcess->waitForFinished(1000)) {
            updateProcess->kill();
            updateProcess->waitForFinished(1000);
        }
    }
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // Top-Right Layout: 3 directional link buttons
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addStretch();
    
    QPushButton *websiteBtn = new QPushButton("🌐 Web Sitemiz", this);
    QPushButton *roAsdGitBtn = new QPushButton("📦 ro-asd GitHub", this);
    QPushButton *roAssistGitBtn = new QPushButton("🚀 ro-Assist GitHub", this);
    
    topLayout->addWidget(websiteBtn);
    topLayout->addWidget(roAsdGitBtn);
    topLayout->addWidget(roAssistGitBtn);
    mainLayout->addLayout(topLayout);

    connect(websiteBtn, &QPushButton::clicked, this, &MainWindow::openWebsite);
    connect(roAsdGitBtn, &QPushButton::clicked, this, &MainWindow::openRoAsdGitHub);
    connect(roAssistGitBtn, &QPushButton::clicked, this, &MainWindow::openRoAssistGitHub);

    // Middle Layout
    QVBoxLayout *middleLayout = new QVBoxLayout();
    middleLayout->setAlignment(Qt::AlignCenter);
    
    versionLabel = new QLabel("Mevcut Sürüm: 0.1.0", this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setObjectName("versionLabel");

    statusLabel = new QLabel("Güncelleme Bekleniyor", this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setObjectName("statusLabel");

    updateButton = new QPushButton("Sistemi Güncelle", this);
    updateButton->setObjectName("updateButton");
    updateButton->setFixedSize(220, 60);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->hide(); 
    progressBar->setFixedWidth(300);

    middleLayout->addWidget(versionLabel, 0, Qt::AlignCenter);
    middleLayout->addWidget(statusLabel, 0, Qt::AlignCenter);
    middleLayout->addSpacing(20);
    middleLayout->addWidget(updateButton, 0, Qt::AlignCenter);
    middleLayout->addSpacing(20);
    middleLayout->addWidget(progressBar, 0, Qt::AlignCenter);

    connect(updateButton, &QPushButton::clicked, this, &MainWindow::startUpdate);

    mainLayout->addStretch();
    mainLayout->addLayout(middleLayout);
    mainLayout->addStretch();

    // Bottom-Left Layout
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QPushButton *aboutBtn = new QPushButton("ℹ️ Hakkında", this);
    aboutBtn->setObjectName("aboutButton");
    
    bottomLayout->addWidget(aboutBtn);
    bottomLayout->addStretch();
    
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::showAboutDialog);

    mainLayout->addLayout(bottomLayout);
}

void MainWindow::setupStyle()
{
    QString style = R"(
        QMainWindow { background-color: #f4f5f7; }
        QLabel#versionLabel { font-size: 16px; font-weight: bold; color: #333333; }
        QLabel#statusLabel { font-size: 14px; color: #666666; }
        QPushButton {
            background-color: #ffffff; border: 1px solid #dcdfe6;
            border-radius: 8px; padding: 8px 16px; color: #333333; font-size: 14px;
        }
        QPushButton:hover { background-color: #e5e7eb; }
        QPushButton:pressed { background-color: #d1d5db; margin-top: 2px; margin-bottom: -2px; }
        QPushButton#updateButton {
            background-color: #0066cc; color: white; border: none;
            font-size: 18px; font-weight: bold; border-radius: 8px;
        }
        QPushButton#updateButton:hover { background-color: #0052a3; }
        QPushButton#updateButton:pressed { background-color: #003d7a; margin-top: 2px; margin-bottom: -2px; }
        QPushButton#updateButton:disabled { background-color: #99c2ff; color: #f0f0f0; }
        QPushButton#aboutButton { background-color: transparent; border: none; color: #666666; }
        QPushButton#aboutButton:hover { background-color: #e5e7eb; color: #333333; }
        QProgressBar { border: 1px solid #dcdfe6; border-radius: 4px; text-align: center; background-color: white; }
        QProgressBar::chunk { background-color: #0066cc; width: 10px; }
    )";
    setStyleSheet(style);
}

void MainWindow::openWebsite() { QDesktopServices::openUrl(QUrl("https://www.example.com")); }
void MainWindow::openRoAsdGitHub() { QDesktopServices::openUrl(QUrl("https://github.com/example/ro-asd")); }
void MainWindow::openRoAssistGitHub() { QDesktopServices::openUrl(QUrl("https://github.com/example/ro-Assist")); }
void MainWindow::showAboutDialog() { QMessageBox::about(this, "Hakkında", "Geliştirici: Ebubekir Bulut\nYıl: 2026"); }

void MainWindow::startUpdate()
{
    bool ok;
    QString password = QInputDialog::getText(this, tr("Sudo Yetkisi Gerekli"),
                                              tr("Lütfen sudo şifrenizi girin:"), QLineEdit::Password,
                                              "", &ok);
    if (!ok || password.isEmpty()) {
        return; 
    }

    updateButton->setEnabled(false);
    
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->show();
    
    statusLabel->setText("Güncelleme İşlemi Başlıyor...");
    statusLabel->setStyleSheet("font-size: 14px; color: #ff9900;");

    transactionPhaseStarted = false;
    isUpToDate = false;
    isTerminatingIntentionally = false;

    // Direct invocation with -S to read from stdin
    updateProcess->start("sudo", QStringList() << "-S" << "dnf" << "upgrade" << "-y");
    
    // Immediately write the provided password and a newline
    updateProcess->write((password + "\n").toUtf8());
}

void MainWindow::checkDnfErrors(const QString &output)
{
    // DNF LOCK ERROR (Another process is running)
    if (output.contains("Waiting for process", Qt::CaseInsensitive) ||
        output.contains("Waiting for lock", Qt::CaseInsensitive) ||
        output.contains("Another app is currently holding the yum lock", Qt::CaseInsensitive)) {
        statusLabel->setText("⚠️ Sistem şu an başka bir işlemle meşgul, lütfen bekleyin...");
        statusLabel->setStyleSheet("font-size: 14px; color: #ff9900; font-weight: bold;");
    }

    // NETWORK ERRORS
    if (output.contains("Error: Failed to download metadata", Qt::CaseInsensitive) ||
        output.contains("Timeout", Qt::CaseInsensitive) ||
        output.contains("Could not resolve host", Qt::CaseInsensitive)) {
        statusLabel->setText("🌐 İnternet Hatası: Sunucuya bağlanılamadı.");
        statusLabel->setStyleSheet("font-size: 14px; color: red; font-weight: bold;");
        updateButton->setEnabled(true);
    }
}

void MainWindow::handleUpdateOutput()
{
    QString output = QString::fromUtf8(updateProcess->readAllStandardOutput());
    
    checkDnfErrors(output);

    // Check if system is completely up to date
    if (output.contains("Nothing to do", Qt::CaseInsensitive) || 
        output.contains("Yapılacak bir şey yok", Qt::CaseInsensitive)) {
        isUpToDate = true;
        progressBar->setRange(0, 100);
        progressBar->setValue(100);
        statusLabel->setText("Sistem zaten güncel!");
        statusLabel->setStyleSheet("font-size: 14px; color: green; font-weight: bold;");
        return;
    }

    // Match transaction progress like (5/120)
    QRegularExpression txRegex("\\((\\d+)/(\\d+)\\)");
    QRegularExpressionMatchIterator i = txRegex.globalMatch(output);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        transactionPhaseStarted = true;
        int current = match.captured(1).toInt();
        int total = match.captured(2).toInt();
        
        progressBar->setRange(0, total);
        progressBar->setValue(current);
        statusLabel->setText(QString("Paketler kuruluyor... (%1/%2)").arg(current).arg(total));
        statusLabel->setStyleSheet("font-size: 14px; color: #666666;");
    }

    // Match download percentage like 45% (only if transaction phase hasn't started)
    if (!transactionPhaseStarted) {
        QRegularExpression dlRegex("(\\d+)%");
        QRegularExpressionMatchIterator j = dlRegex.globalMatch(output);
        int lastPercent = -1;
        while (j.hasNext()) {
            QRegularExpressionMatch match = j.next();
            lastPercent = match.captured(1).toInt();
        }
        
        if (lastPercent != -1) {
            progressBar->setRange(0, 100);
            progressBar->setValue(lastPercent);
            statusLabel->setText(QString("İndiriliyor... %1%").arg(lastPercent));
            statusLabel->setStyleSheet("font-size: 14px; color: #666666;");
        }
    }
}

void MainWindow::handleUpdateErrorOutput()
{
    QString errorOutput = QString::fromUtf8(updateProcess->readAllStandardError());
    
    checkDnfErrors(errorOutput);

    // WRONG PASSWORD DETECTION
    if (errorOutput.contains("standard input:1", Qt::CaseInsensitive) ||
        errorOutput.contains("incorrect password", Qt::CaseInsensitive) || 
        errorOutput.contains("try again", Qt::CaseInsensitive)) {
        
        isTerminatingIntentionally = true;
        updateProcess->terminate(); // using terminate() instantly
        
        progressBar->hide();
        progressBar->setValue(0);
        statusLabel->setText("❌ Hata: Şifre Yanlış!");
        statusLabel->setStyleSheet("font-size: 14px; color: red; font-weight: bold;");
        updateButton->setEnabled(true);
    }
}

void MainWindow::handleUpdateFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    updateButton->setEnabled(true);

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        if (!isUpToDate) {
            progressBar->setRange(0, 100);
            progressBar->setValue(100);
            statusLabel->setText("✅ Güncelleme Başarıyla Tamamlandı!");
            statusLabel->setStyleSheet("font-size: 14px; color: green; font-weight: bold;");
        }
    } else {
        // Prevent overwriting specific error messages set by other handlers
        if (!statusLabel->text().contains("Hata: Şifre Yanlış!") && 
            !statusLabel->text().contains("İnternet Hatası") &&
            !isTerminatingIntentionally) {
            
            progressBar->hide();
            statusLabel->setText("Güncelleme Başarısız!");
            statusLabel->setStyleSheet("font-size: 14px; color: red; font-weight: bold;");
        }
    }
}

void MainWindow::handleUpdateProcessError(QProcess::ProcessError error)
{
    if (isTerminatingIntentionally) {
        return; // Ignore crash caused by intentional termination for wrong passwords
    }
    
    progressBar->hide();
    updateButton->setEnabled(true);
    
    if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
        QString errorMsg = (error == QProcess::FailedToStart) 
            ? "Sistem bileşeni başlatılamadı." 
            : "Sistem bileşeni çöktü.";
            
        // CRASH PROTECTION MessageBox
        QMessageBox::critical(this, "Kritik Hata", QString("Sistem bileşeni çalıştırılamadı.\nHata detayı: %1").arg(errorMsg));
        
        if (!statusLabel->text().contains("Hata: Şifre Yanlış!") &&
            !statusLabel->text().contains("İnternet Hatası")) {
            statusLabel->setText("Kritik Hata: " + errorMsg);
            statusLabel->setStyleSheet("font-size: 14px; color: red; font-weight: bold;");
        }
    } else {
        if (!statusLabel->text().contains("Hata: Şifre Yanlış!") &&
            !statusLabel->text().contains("İnternet Hatası")) {
            statusLabel->setText("Güncelleme Başlatılamadı!");
            statusLabel->setStyleSheet("font-size: 14px; color: red; font-weight: bold;");
        }
    }
}
