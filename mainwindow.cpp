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
#include <QLocale>
#include <QStyleHints>
#include <QScrollBar>
#include <QPalette>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), updateProcess(new QProcess(this)), checkUpdateProcess(new QProcess(this)),
      carousel(new QStackedWidget(this)), carouselTimer(new QTimer(this)),
      logConsole(new QTextEdit(this)), gameLogConsole(new QTextEdit(this)), officeLogConsole(new QTextEdit(this)),
      transactionPhaseStarted(false), isUpToDate(false), isTerminatingIntentionally(false),
      isNetworkConnected(true)
{
    detectSystemLanguageAndTheme();
    
    QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability);
    if (QNetworkInformation::instance()) {
        isNetworkConnected = QNetworkInformation::instance()->reachability() == QNetworkInformation::Reachability::Online;
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this, [this](QNetworkInformation::Reachability r) {
            onNetworkConnectedChanged(r == QNetworkInformation::Reachability::Online);
        });
    }

    setupUi();
    updateUiTextAndImages();
    setupStyle();

    logConsole->hide();
    gameLogConsole->hide();
    officeLogConsole->hide();

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        int width = screenGeometry.width() * 0.66;
        int height = screenGeometry.height() * 0.66;
        resize(width, height);
    }

    connect(updateProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::handleUpdateOutput);
    connect(updateProcess, &QProcess::readyReadStandardError, this, &MainWindow::handleUpdateErrorOutput);
    connect(updateProcess, &QProcess::finished, this, &MainWindow::handleUpdateFinished);
    connect(updateProcess, &QProcess::errorOccurred, this, &MainWindow::handleUpdateProcessError);

    connect(checkUpdateProcess, &QProcess::finished, this, &MainWindow::handleCheckUpdateFinished);

    connect(carouselTimer, &QTimer::timeout, this, &MainWindow::nextSlide);
    carouselTimer->start(5000);

    setInitialUpdateStatus();
    checkUpdateProcess->start("dnf", QStringList() << "check-update");
}

MainWindow::~MainWindow()
{
    if (updateProcess && updateProcess->state() != QProcess::NotRunning) {
        updateProcess->terminate();
        if (!updateProcess->waitForFinished(1000)) {
            updateProcess->kill();
            updateProcess->waitForFinished(1000);
        }
    }
    if (checkUpdateProcess && checkUpdateProcess->state() != QProcess::NotRunning) {
        checkUpdateProcess->kill();
        checkUpdateProcess->waitForFinished(1000);
    }
}

void MainWindow::detectSystemLanguageAndTheme()
{
    QLocale locale = QLocale::system();
    if (locale.language() == QLocale::Turkish) currentLang = TR;
    else if (locale.language() == QLocale::Spanish) currentLang = ES;
    else currentLang = EN;

    currentTheme = Light; 

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (const QStyleHints *hints = QGuiApplication::styleHints()) {
        if (hints->colorScheme() == Qt::ColorScheme::Dark) currentTheme = Dark;
    }
#else
    // Fallback for older Qt versions (Qt < 6.5)
    if (QGuiApplication::palette().color(QPalette::WindowText).lightness() > 
        QGuiApplication::palette().color(QPalette::Window).lightness()) {
        currentTheme = Dark;
    }
#endif
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // TOP BAR
    QHBoxLayout *topLayout = new QHBoxLayout();
    networkStatusLabel = new QLabel(this);
    networkStatusLabel->setStyleSheet("font-weight: bold; color: #ffcc00; font-size: 14px;");
    networkStatusLabel->setVisible(!isNetworkConnected);
    
    topLayout->addWidget(networkStatusLabel);
    topLayout->addStretch();
    themeToggleBtn = new QPushButton(this);
    langComboBox = new QComboBox(this);
    langComboBox->addItem("🇹🇷 TR", QVariant::fromValue((int)TR));
    langComboBox->addItem("🇬🇧 EN", QVariant::fromValue((int)EN));
    langComboBox->addItem("🇪🇸 ES", QVariant::fromValue((int)ES));
    
    int defaultIdx = (currentLang == TR) ? 0 : ((currentLang == ES) ? 2 : 1);
    langComboBox->setCurrentIndex(defaultIdx);

    themeToggleBtn->setMinimumSize(100, 40);
    langComboBox->setMinimumSize(100, 40);
    connect(themeToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    connect(langComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::changeLanguage);

    topLayout->addWidget(themeToggleBtn);
    topLayout->addWidget(langComboBox);
    mainLayout->addLayout(topLayout);

    mainStack = new QStackedWidget(this);

    // 1. CAROUSEL
    carouselViewWidget = new QWidget(this);
    QHBoxLayout *carouselLayout = new QHBoxLayout(carouselViewWidget);
    carouselLayout->setContentsMargins(0, 0, 0, 0);
    prevSlideBtn = new QPushButton("◀", this);
    prevSlideBtn->setObjectName("navButton");
    prevSlideBtn->setCursor(Qt::PointingHandCursor);
    nextSlideBtn = new QPushButton("▶", this);
    nextSlideBtn->setObjectName("navButton");
    nextSlideBtn->setCursor(Qt::PointingHandCursor);
    connect(prevSlideBtn, &QPushButton::clicked, this, &MainWindow::prevSlide);
    connect(nextSlideBtn, &QPushButton::clicked, this, &MainWindow::nextSlide);
    carousel->setObjectName("panelWidget");
    createCarouselSlides();
    carouselLayout->addStretch(2);
    carouselLayout->addWidget(prevSlideBtn, 0, Qt::AlignCenter);
    carouselLayout->addSpacing(20);
    carouselLayout->addWidget(carousel, 6);
    carouselLayout->addSpacing(20);
    carouselLayout->addWidget(nextSlideBtn, 0, Qt::AlignCenter);
    carouselLayout->addStretch(2);
    
    // 2. UPDATE VIEW
    updateViewWidget = new QWidget(this);
    QVBoxLayout *updateLayout = new QVBoxLayout(updateViewWidget);
    updateLayout->setContentsMargins(0, 0, 0, 0);
    backToCarouselBtn = new QPushButton(this);
    backToCarouselBtn->setObjectName("backButton");
    backToCarouselBtn->setMinimumSize(120, 40);
    backToCarouselBtn->setCursor(Qt::PointingHandCursor);
    connect(backToCarouselBtn, &QPushButton::clicked, this, &MainWindow::showCarouselScreen);
    
    QHBoxLayout *updateTopLayout = new QHBoxLayout();
    updateTopLayout->addWidget(backToCarouselBtn);
    updateTopLayout->addStretch();
    
    QWidget *updatePanel = new QWidget(this);
    updatePanel->setObjectName("panelWidget");
    QVBoxLayout *panelLayout = new QVBoxLayout(updatePanel);
    panelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    versionLabel = new QLabel("0.1.0", this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setObjectName("versionLabel");
    statusLabel = new QLabel("", this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setObjectName("statusLabel");
    statusLabel->setWordWrap(true);
    statusLabel->setMinimumWidth(400);

    updateButton = new QPushButton(this);
    updateButton->setObjectName("updateButton");
    updateButton->setFixedSize(320, 70);
    updateButton->setCursor(Qt::PointingHandCursor);
    connect(updateButton, &QPushButton::clicked, this, &MainWindow::startUpdate);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->hide(); 
    progressBar->setFixedWidth(400);

    toggleLogBtn = new QPushButton(this);
    toggleLogBtn->setObjectName("backButton");
    toggleLogBtn->setCursor(Qt::PointingHandCursor);
    connect(toggleLogBtn, &QPushButton::clicked, this, &MainWindow::toggleUpdateLogs);

    logConsole->setReadOnly(true);
    logConsole->setObjectName("logConsole");
    logConsole->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    logConsole->setMaximumHeight(150);
    logConsole->setMinimumHeight(100);

    panelLayout->addWidget(versionLabel, 0, Qt::AlignCenter);
    panelLayout->addSpacing(10);
    panelLayout->addWidget(statusLabel, 0, Qt::AlignCenter);
    panelLayout->addSpacing(20);
    panelLayout->addWidget(updateButton, 0, Qt::AlignCenter);
    panelLayout->addSpacing(20);
    panelLayout->addWidget(progressBar, 0, Qt::AlignCenter);
    panelLayout->addSpacing(10);
    panelLayout->addWidget(toggleLogBtn, 0, Qt::AlignCenter);
    panelLayout->addSpacing(5);
    panelLayout->addWidget(logConsole, 1);
    
    updateLayout->addLayout(updateTopLayout);
    updateLayout->addSpacing(10);
    updateLayout->addWidget(updatePanel, 1);

    // 3. GAME PACKAGE VIEW
    gameViewWidget = new QWidget(this);
    QVBoxLayout *gameLayout = new QVBoxLayout(gameViewWidget);
    gameLayout->setContentsMargins(0, 0, 0, 0);

    backFromGameBtn = new QPushButton(this);
    backFromGameBtn->setObjectName("backButton");
    backFromGameBtn->setMinimumSize(120, 40);
    backFromGameBtn->setCursor(Qt::PointingHandCursor);
    connect(backFromGameBtn, &QPushButton::clicked, this, &MainWindow::showCarouselScreen);
    
    QHBoxLayout *gameTopLayout = new QHBoxLayout();
    gameTopLayout->addWidget(backFromGameBtn);
    gameTopLayout->addStretch();

    QWidget *gamePanel = new QWidget(this);
    gamePanel->setObjectName("panelWidget");
    QVBoxLayout *gamePanelLayout = new QVBoxLayout(gamePanel);
    gamePanelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    gameStatusLabel = new QLabel("", this);
    gameStatusLabel->setAlignment(Qt::AlignCenter);
    gameStatusLabel->setObjectName("statusLabel");
    gameStatusLabel->setWordWrap(true);
    gameStatusLabel->setMinimumWidth(400);

    gameInstallButton = new QPushButton(this);
    gameInstallButton->setObjectName("updateButton");
    gameInstallButton->setFixedSize(320, 70);
    gameInstallButton->setCursor(Qt::PointingHandCursor);
    connect(gameInstallButton, &QPushButton::clicked, this, &MainWindow::startGamePackageInstall);

    gameProgressBar = new QProgressBar(this);
    gameProgressBar->setRange(0, 100);
    gameProgressBar->setValue(0);
    gameProgressBar->hide(); 
    gameProgressBar->setFixedWidth(400);

    toggleGameLogBtn = new QPushButton(this);
    toggleGameLogBtn->setObjectName("backButton");
    toggleGameLogBtn->setCursor(Qt::PointingHandCursor);
    connect(toggleGameLogBtn, &QPushButton::clicked, this, &MainWindow::toggleGameLogs);

    gameLogConsole->setReadOnly(true);
    gameLogConsole->setObjectName("logConsole");
    gameLogConsole->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    gameLogConsole->setMaximumHeight(150);
    gameLogConsole->setMinimumHeight(100);

    gamePanelLayout->addSpacing(40);
    gamePanelLayout->addWidget(gameStatusLabel, 0, Qt::AlignCenter);
    gamePanelLayout->addSpacing(20);
    gamePanelLayout->addWidget(gameInstallButton, 0, Qt::AlignCenter);
    gamePanelLayout->addSpacing(20);
    gamePanelLayout->addWidget(gameProgressBar, 0, Qt::AlignCenter);
    gamePanelLayout->addSpacing(10);
    gamePanelLayout->addWidget(toggleGameLogBtn, 0, Qt::AlignCenter);
    gamePanelLayout->addSpacing(5);
    gamePanelLayout->addWidget(gameLogConsole, 1);

    gameLayout->addLayout(gameTopLayout);
    gameLayout->addSpacing(10);
    gameLayout->addWidget(gamePanel, 1);

    // 4. CUSTOM APP STORE VIEW
    appStoreViewWidget = new QWidget(this);
    QVBoxLayout *storeLayout = new QVBoxLayout(appStoreViewWidget);
    storeLayout->setContentsMargins(0, 0, 0, 0);

    backFromAppStoreBtn = new QPushButton(this);
    backFromAppStoreBtn->setObjectName("backButton");
    backFromAppStoreBtn->setMinimumSize(120, 40);
    backFromAppStoreBtn->setCursor(Qt::PointingHandCursor);
    connect(backFromAppStoreBtn, &QPushButton::clicked, this, &MainWindow::showCarouselScreen);
    
    QHBoxLayout *storeTopLayout = new QHBoxLayout();
    storeTopLayout->addWidget(backFromAppStoreBtn);
    storeTopLayout->addStretch();

    QWidget *storePanel = new QWidget(this);
    storePanel->setObjectName("panelWidget");
    QVBoxLayout *storePanelLayout = new QVBoxLayout(storePanel);
    storePanelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    appStoreTitleLabel = new QLabel(this);
    appStoreTitleLabel->setObjectName("slideTitle");
    appStoreTitleLabel->setAlignment(Qt::AlignCenter);
    appStorePlaceholderIcon = new QLabel("🛍️", this);
    appStorePlaceholderIcon->setAlignment(Qt::AlignCenter);
    appStorePlaceholderIcon->setStyleSheet("font-size: 150px; background-color: transparent; border: none;");
    appStoreOpenAppBtn = new QPushButton(this);
    appStoreOpenAppBtn->setObjectName("actionButton");
    appStoreOpenAppBtn->setMinimumSize(320, 70);
    appStoreOpenAppBtn->setCursor(Qt::PointingHandCursor);
    connect(appStoreOpenAppBtn, &QPushButton::clicked, this, &MainWindow::dummyAppStoreAction);

    storePanelLayout->addStretch();
    storePanelLayout->addWidget(appStoreTitleLabel, 0, Qt::AlignCenter);
    storePanelLayout->addSpacing(40);
    storePanelLayout->addWidget(appStorePlaceholderIcon, 0, Qt::AlignCenter);
    storePanelLayout->addSpacing(40);
    storePanelLayout->addWidget(appStoreOpenAppBtn, 0, Qt::AlignCenter);
    storePanelLayout->addStretch();

    storeLayout->addLayout(storeTopLayout);
    storeLayout->addSpacing(10);
    storeLayout->addWidget(storePanel, 1);
    
    // 5. OFFICE PACKAGE VIEW
    officeViewWidget = new QWidget(this);
    QVBoxLayout *officeLayout = new QVBoxLayout(officeViewWidget);
    officeLayout->setContentsMargins(0, 0, 0, 0);

    backFromOfficeBtn = new QPushButton(this);
    backFromOfficeBtn->setObjectName("backButton");
    backFromOfficeBtn->setMinimumSize(120, 40);
    backFromOfficeBtn->setCursor(Qt::PointingHandCursor);
    connect(backFromOfficeBtn, &QPushButton::clicked, this, &MainWindow::showCarouselScreen);
    
    QHBoxLayout *officeTopLayout = new QHBoxLayout();
    officeTopLayout->addWidget(backFromOfficeBtn);
    officeTopLayout->addStretch();
    
    officeLevelComboBox = new QComboBox(this);
    officeLevelComboBox->setMinimumSize(150, 40);
    officeLevelComboBox->addItem("LibreOffice", QVariant::fromValue((int)LibreOfficeBasic));
    officeLevelComboBox->addItem("LibreOffice (Full)", QVariant::fromValue((int)LibreOfficeFull));
    officeLevelComboBox->addItem("OnlyOffice", QVariant::fromValue((int)OnlyOffice));
    officeLevelComboBox->addItem("WPS Office", QVariant::fromValue((int)WPSOffice));
    connect(officeLevelComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onOfficeLevelChanged);
    
    officeTopLayout->addWidget(officeLevelComboBox);

    QWidget *officePanel = new QWidget(this);
    officePanel->setObjectName("panelWidget");
    QVBoxLayout *officePanelLayout = new QVBoxLayout(officePanel);
    officePanelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    officeStatusLabel = new QLabel("", this);
    officeStatusLabel->setAlignment(Qt::AlignCenter);
    officeStatusLabel->setObjectName("statusLabel");
    officeStatusLabel->setWordWrap(true);
    officeStatusLabel->setMinimumWidth(400);
    
    officeDescLabel = new QLabel("", this);
    officeDescLabel->setAlignment(Qt::AlignCenter);
    officeDescLabel->setWordWrap(true);
    officeDescLabel->setObjectName("slideDesc");

    officeInstallButton = new QPushButton(this);
    officeInstallButton->setObjectName("updateButton");
    officeInstallButton->setFixedSize(320, 70);
    officeInstallButton->setCursor(Qt::PointingHandCursor);
    connect(officeInstallButton, &QPushButton::clicked, this, &MainWindow::startOfficePackageInstall);

    officeProgressBar = new QProgressBar(this);
    officeProgressBar->setRange(0, 100);
    officeProgressBar->setValue(0);
    officeProgressBar->hide(); 
    officeProgressBar->setFixedWidth(400);

    toggleOfficeLogBtn = new QPushButton(this);
    toggleOfficeLogBtn->setObjectName("backButton");
    toggleOfficeLogBtn->setCursor(Qt::PointingHandCursor);
    connect(toggleOfficeLogBtn, &QPushButton::clicked, this, &MainWindow::toggleOfficeLogs);

    officeLogConsole->setReadOnly(true);
    officeLogConsole->setObjectName("logConsole");
    officeLogConsole->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    officeLogConsole->setMaximumHeight(150);
    officeLogConsole->setMinimumHeight(100);

    officePanelLayout->addSpacing(30);
    officePanelLayout->addWidget(officeStatusLabel, 0, Qt::AlignCenter);
    officePanelLayout->addSpacing(10);
    officePanelLayout->addWidget(officeDescLabel, 0, Qt::AlignCenter);
    officePanelLayout->addSpacing(20);
    officePanelLayout->addWidget(officeInstallButton, 0, Qt::AlignCenter);
    officePanelLayout->addSpacing(20);
    officePanelLayout->addWidget(officeProgressBar, 0, Qt::AlignCenter);
    officePanelLayout->addSpacing(10);
    officePanelLayout->addWidget(toggleOfficeLogBtn, 0, Qt::AlignCenter);
    officePanelLayout->addSpacing(5);
    officePanelLayout->addWidget(officeLogConsole, 1);

    officeLayout->addLayout(officeTopLayout);
    officeLayout->addSpacing(10);
    officeLayout->addWidget(officePanel, 1);

    mainStack->addWidget(carouselViewWidget); // 0
    mainStack->addWidget(updateViewWidget);   // 1
    mainStack->addWidget(gameViewWidget);     // 2
    mainStack->addWidget(appStoreViewWidget); // 3
    mainStack->addWidget(officeViewWidget);   // 4

    mainLayout->addWidget(mainStack, 1);

    // BOTTOM BAR
    QPushButton *aboutBtn = new QPushButton("ℹ️", this);
    aboutBtn->setObjectName("aboutButton");
    aboutBtn->setFixedSize(50, 50);
    aboutBtn->setCursor(Qt::PointingHandCursor);
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::showAboutDialog);
    
    QHBoxLayout *aboutLayout = new QHBoxLayout();
    aboutLayout->addWidget(aboutBtn);
    aboutLayout->addStretch();
    mainLayout->addLayout(aboutLayout);
}

void MainWindow::createCarouselSlides()
{
    // Slide 1: Update System
    QWidget *slide1 = new QWidget();
    QVBoxLayout *l1 = new QVBoxLayout(slide1);
    slide1Title = new QLabel(this);
    slide1Title->setObjectName("slideTitle");
    slide1Title->setAlignment(Qt::AlignCenter);
    slide1Title->setWordWrap(true);
    slide1Desc = new QLabel(this);
    slide1Desc->setObjectName("slideDesc");
    slide1Desc->setAlignment(Qt::AlignCenter);
    slide1Desc->setWordWrap(true);
    QPushButton *s1Btn = new QPushButton("🔄");
    s1Btn->setFixedSize(120, 120);
    s1Btn->setObjectName("roundButton");
    s1Btn->setCursor(Qt::PointingHandCursor);
    connect(s1Btn, &QPushButton::clicked, this, &MainWindow::showUpdateScreen);
    l1->addStretch(); l1->addWidget(slide1Title); l1->addSpacing(20); l1->addWidget(slide1Desc); l1->addSpacing(40); l1->addWidget(s1Btn, 0, Qt::AlignCenter); l1->addStretch();
    carousel->addWidget(slide1);

    // Slide 2: Social Media
    QWidget *slide2 = new QWidget();
    QVBoxLayout *l2 = new QVBoxLayout(slide2);
    slide2Title = new QLabel(this);
    slide2Title->setObjectName("slideTitle");
    slide2Title->setAlignment(Qt::AlignCenter);
    slide2Title->setWordWrap(true);
    
    QHBoxLayout *socialLayout = new QHBoxLayout();
    socialLayout->setSpacing(20);
    websiteBtn = new QPushButton(this);
    roAsdGitHubBtn = new QPushButton(this);
    roAssistGitHubBtn = new QPushButton(this);
    websiteBtn->setObjectName("squareSoftButton");
    roAsdGitHubBtn->setObjectName("squareSoftButton");
    roAssistGitHubBtn->setObjectName("squareSoftButton");
    websiteBtn->setFixedSize(140, 140);
    roAsdGitHubBtn->setFixedSize(140, 140);
    roAssistGitHubBtn->setFixedSize(140, 140);
    websiteBtn->setCursor(Qt::PointingHandCursor);
    roAsdGitHubBtn->setCursor(Qt::PointingHandCursor);
    roAssistGitHubBtn->setCursor(Qt::PointingHandCursor);
    
    socialLayout->addStretch();
    socialLayout->addWidget(websiteBtn);
    socialLayout->addWidget(roAsdGitHubBtn);
    socialLayout->addWidget(roAssistGitHubBtn);
    socialLayout->addStretch();
    
    connect(websiteBtn, &QPushButton::clicked, this, &MainWindow::openWebsite);
    connect(roAsdGitHubBtn, &QPushButton::clicked, this, &MainWindow::openRoAsdGitHub);
    connect(roAssistGitHubBtn, &QPushButton::clicked, this, &MainWindow::openRoAssistGitHub);
    
    l2->addStretch(); l2->addWidget(slide2Title); l2->addSpacing(30); l2->addLayout(socialLayout); l2->addStretch();
    carousel->addWidget(slide2);

    // Slide 3: App Store
    QWidget *slide3 = new QWidget();
    QVBoxLayout *l3 = new QVBoxLayout(slide3);
    slide3Title = new QLabel(this);
    slide3Title->setObjectName("slideTitle");
    slide3Title->setAlignment(Qt::AlignCenter);
    slide3Title->setWordWrap(true);
    slide3Desc = new QLabel(this);
    slide3Desc->setObjectName("slideDesc");
    slide3Desc->setAlignment(Qt::AlignCenter);
    slide3Desc->setWordWrap(true);
    appStoreSlideBtn = new QPushButton(this);
    appStoreSlideBtn->setObjectName("actionButton");
    appStoreSlideBtn->setMinimumSize(280, 70);
    appStoreSlideBtn->setCursor(Qt::PointingHandCursor);
    connect(appStoreSlideBtn, &QPushButton::clicked, this, &MainWindow::showAppStoreScreen);
    l3->addStretch(); l3->addWidget(slide3Title); l3->addSpacing(20); l3->addWidget(slide3Desc); l3->addSpacing(40); l3->addWidget(appStoreSlideBtn, 0, Qt::AlignCenter); l3->addStretch();
    carousel->addWidget(slide3);

    // Slide 4: Bozok Community
    QWidget *slide4 = new QWidget();
    QVBoxLayout *l4 = new QVBoxLayout(slide4);
    slide4Title = new QLabel(this);
    slide4Title->setObjectName("slideTitle");
    slide4Title->setAlignment(Qt::AlignCenter);
    slide4Title->setWordWrap(true);
    slide4Desc = new QLabel(this);
    slide4Desc->setObjectName("slideDesc");
    slide4Desc->setAlignment(Qt::AlignCenter);
    slide4Desc->setWordWrap(true);
    bozokBtn = new QPushButton(this);
    bozokBtn->setObjectName("actionButton");
    bozokBtn->setMinimumSize(280, 70);
    bozokBtn->setCursor(Qt::PointingHandCursor);
    connect(bozokBtn, &QPushButton::clicked, this, &MainWindow::openBozokCommunity);
    l4->addStretch(); l4->addWidget(slide4Title); l4->addSpacing(20); l4->addWidget(slide4Desc); l4->addSpacing(40); l4->addWidget(bozokBtn, 0, Qt::AlignCenter); l4->addStretch();
    carousel->addWidget(slide4);

    // Slide 5: Game Package
    QWidget *slide5 = new QWidget();
    QVBoxLayout *l5 = new QVBoxLayout(slide5);
    slide5Title = new QLabel(this);
    slide5Title->setObjectName("slideTitle");
    slide5Title->setAlignment(Qt::AlignCenter);
    slide5Title->setWordWrap(true);
    slide5Desc = new QLabel(this);
    slide5Desc->setObjectName("slideDesc");
    slide5Desc->setAlignment(Qt::AlignCenter);
    slide5Desc->setWordWrap(true);
    gamePackageSlideBtn = new QPushButton(this);
    gamePackageSlideBtn->setObjectName("actionButton");
    gamePackageSlideBtn->setMinimumSize(280, 70);
    gamePackageSlideBtn->setCursor(Qt::PointingHandCursor);
    connect(gamePackageSlideBtn, &QPushButton::clicked, this, &MainWindow::showGameScreen);
    l5->addStretch(); l5->addWidget(slide5Title); l5->addSpacing(20); l5->addWidget(slide5Desc); l5->addSpacing(40); l5->addWidget(gamePackageSlideBtn, 0, Qt::AlignCenter); l5->addStretch();
    carousel->addWidget(slide5);
    
    // Slide 6: Office Package
    QWidget *slide6 = new QWidget();
    QVBoxLayout *l6 = new QVBoxLayout(slide6);
    slide6Title = new QLabel(this);
    slide6Title->setObjectName("slideTitle");
    slide6Title->setAlignment(Qt::AlignCenter);
    slide6Title->setWordWrap(true);
    slide6Desc = new QLabel(this);
    slide6Desc->setObjectName("slideDesc");
    slide6Desc->setAlignment(Qt::AlignCenter);
    slide6Desc->setWordWrap(true);
    officePackageSlideBtn = new QPushButton(this);
    officePackageSlideBtn->setObjectName("actionButton");
    officePackageSlideBtn->setMinimumSize(280, 70);
    officePackageSlideBtn->setCursor(Qt::PointingHandCursor);
    connect(officePackageSlideBtn, &QPushButton::clicked, this, &MainWindow::showOfficeScreen);
    l6->addStretch(); l6->addWidget(slide6Title); l6->addSpacing(20); l6->addWidget(slide6Desc); l6->addSpacing(40); l6->addWidget(officePackageSlideBtn, 0, Qt::AlignCenter); l6->addStretch();
    carousel->addWidget(slide6);
}

void MainWindow::onOfficeLevelChanged(int index)
{
    int enumVal = officeLevelComboBox->itemData(index).toInt();
    
    QString desc = "LibreOffice (Temel): Word(Writer), Excel(Calc) ve PowerPoint(Impress).";
    if (enumVal == LibreOfficeFull) desc = "LibreOffice Tam Sürüm: Ekstra PDF okuyucu özelliği dahildir.";
    else if (enumVal == OnlyOffice) desc = "OnlyOffice Dekstop Editors, MS Office dökümanlarıyla yüksek uyumluluk.";
    else if (enumVal == WPSOffice) desc = "WPS Office, Çin yapımı modern arayüzlü ve MS Office uyumlu bir ofis seti.";
    
    if (currentLang == EN) {
        desc = "LibreOffice (Basic): Installs Writer(Word), Calc(Excel), Impress(PowerPoint).";
        if (enumVal == LibreOfficeFull) desc = "LibreOffice Full Suite: Includes extra PDF reader features.";
        else if (enumVal == OnlyOffice) desc = "OnlyOffice Desktop Editors with ultra-high MS Office compatibility.";
        else if (enumVal == WPSOffice) desc = "WPS Office, high MS Office compatibility and modern ribbon UI.";
    } else if (currentLang == ES) {
        desc = "LibreOffice (Básico): Instala Writer(Word), Calc(Excel) e Impress(PowerPoint).";
        if (enumVal == LibreOfficeFull) desc = "LibreOffice Completo: Incluye funciones extra para leer PDF.";
        else if (enumVal == OnlyOffice) desc = "Editores OnlyOffice Desktop, altísima compatibilidad con MS Office.";
        else if (enumVal == WPSOffice) desc = "WPS Office, alta compatibilidad MS Office y una interfaz moderna.";
    }
    officeDescLabel->setText(desc);
}

void MainWindow::updateUiTextAndImages()
{
    bool isTr = (currentLang == TR);
    bool isEs = (currentLang == ES);
    
    if (isTr) {
        themeToggleBtn->setText(currentTheme == Dark ? "🌙 Koyu" : "☀️ Açık");
        networkStatusLabel->setText("⚠ İnternet Bağlantısı Yok");
        backToCarouselBtn->setText("⬅ Geri");
        backFromGameBtn->setText("⬅ Geri");
        backFromAppStoreBtn->setText("⬅ Geri");
        backFromOfficeBtn->setText("⬅ Geri");
        toggleLogBtn->setText(logConsole->isVisible() ? "Gizle ⬆" : "Logları Göster ⬇");
        toggleGameLogBtn->setText(gameLogConsole->isVisible() ? "Gizle ⬆" : "Logları Göster ⬇");
        toggleOfficeLogBtn->setText(officeLogConsole->isVisible() ? "Gizle ⬆" : "Logları Göster ⬇");
        versionLabel->setText("Mevcut Sürüm: 0.1.0");
        updateButton->setText("Sistemi Güncelle");
        slide1Title->setText("Sisteminizi Tek Tuşla Güncelleyin");
        slide1Desc->setText("Sistemdeki tüm paketleri, flatpak ve snap uygulamalarını günceller.");
        slide2Title->setText("Sosyal Mecralardan Bizi Takip Edin");
        websiteBtn->setText("🌐\n\nWeb Sitemiz");
        slide3Title->setText("Uygulama Mağazamızı Keşfedin");
        slide3Desc->setText("Kendi mağazamızdan uygulamalara göz atın.");
        appStoreSlideBtn->setText("Mağazaya Git");
        appStoreTitleLabel->setText("Özel Uygulama Mağazamızı Keşfedin");
        appStoreOpenAppBtn->setText("Mağazayı / Uygulamayı Aç");
        slide4Title->setText("Biz Kimiz? Topluluğumuzu Keşfedin");
        slide4Desc->setText("Bozok Topluluğu olarak Açık Kaynak için çalışıyoruz.");
        bozokBtn->setText("Topluluğa Katıl");
        slide5Title->setText("Oyun Paketi");
        slide5Desc->setText("Steam ve Lutris kurulum sayfasına gidin.");
        gamePackageSlideBtn->setText("Oyun Ekranına Geç");
        gameInstallButton->setText("Hepsini İndir");
        gameStatusLabel->setText("İndirmeyi Başlat");
        slide6Title->setText("Ofis Programları");
        slide6Desc->setText("İhtiyacınıza göre ofis yüklemeleri gerçekleştirin.");
        officePackageSlideBtn->setText("Ofis Ekranına Geç");
        officeInstallButton->setText("Kurulumu Başlat");
        officeStatusLabel->setText("Seçilen Paketi İndir");
        officeLevelComboBox->setItemText(0, "LibreOffice (Basit)");
        officeLevelComboBox->setItemText(1, "LibreOffice (Tam Kapsamlı)");
        officeLevelComboBox->setItemText(2, "OnlyOffice");
        officeLevelComboBox->setItemText(3, "WPS Office");
    } else if (isEs) {
        themeToggleBtn->setText(currentTheme == Dark ? "🌙 Oscuro" : "☀️ Claro");
        networkStatusLabel->setText("⚠ Sin conexión a Internet");
        backToCarouselBtn->setText("⬅ Volver");
        backFromGameBtn->setText("⬅ Volver");
        backFromAppStoreBtn->setText("⬅ Volver");
        backFromOfficeBtn->setText("⬅ Volver");
        toggleLogBtn->setText(logConsole->isVisible() ? "Ocultar ⬆" : "Mostrar Registros ⬇");
        toggleGameLogBtn->setText(gameLogConsole->isVisible() ? "Ocultar ⬆" : "Mostrar Registros ⬇");
        toggleOfficeLogBtn->setText(officeLogConsole->isVisible() ? "Ocultar ⬆" : "Mostrar Registros ⬇");
        versionLabel->setText("Versión Actual: 0.1.0");
        updateButton->setText("Actualizar Sistema");
        slide1Title->setText("Actualiza tu Sistema con un Clic");
        slide1Desc->setText("Actualiza todos los paquetes del sistema, incluyendo flatpak y snap.");
        slide2Title->setText("Síganos en las Redes Sociales");
        websiteBtn->setText("🌐\n\nSitio Web");
        slide3Title->setText("Descubre nuestra Tienda de Apps");
        slide3Desc->setText("Navega hasta nuestra tienda de aplicaciones personalizada.");
        appStoreSlideBtn->setText("Ir a Tienda de Apps");
        appStoreTitleLabel->setText("Descubre Nuestra Tienda Especial");
        appStoreOpenAppBtn->setText("Abrir Aplicación de la Tienda");
        slide4Title->setText("¿Quiénes somos? Nuestra comunidad");
        slide4Desc->setText("Como la comunidad Bozok, trabajamos por el Código Abierto.");
        bozokBtn->setText("Únete a la Comunidad");
        slide5Title->setText("Paquete de Juegos");
        slide5Desc->setText("Vaya a la página de instalación de Steam y Lutris.");
        gamePackageSlideBtn->setText("Ir a Pantalla de Juegos");
        gameInstallButton->setText("Descargar todo");
        gameStatusLabel->setText("Iniciar la Descarga");
        slide6Title->setText("Programas de Oficina");
        slide6Desc->setText("Realiza instalaciones de oficina según tus necesidades.");
        officePackageSlideBtn->setText("Ir a Pantalla de Oficina");
        officeInstallButton->setText("Iniciar Instalación");
        officeStatusLabel->setText("Descargar Paquete");
        officeLevelComboBox->setItemText(0, "LibreOffice (Básico)");
        officeLevelComboBox->setItemText(1, "LibreOffice (Completo)");
        officeLevelComboBox->setItemText(2, "OnlyOffice");
        officeLevelComboBox->setItemText(3, "WPS Office");
    } else {
        themeToggleBtn->setText(currentTheme == Dark ? "🌙 Dark" : "☀️ Light");
        networkStatusLabel->setText("⚠ No Internet Connection");
        backToCarouselBtn->setText("⬅ Back");
        backFromGameBtn->setText("⬅ Back");
        backFromAppStoreBtn->setText("⬅ Back");
        backFromOfficeBtn->setText("⬅ Back");
        toggleLogBtn->setText(logConsole->isVisible() ? "Hide ⬆" : "Show Logs ⬇");
        toggleGameLogBtn->setText(gameLogConsole->isVisible() ? "Hide ⬆" : "Show Logs ⬇");
        toggleOfficeLogBtn->setText(officeLogConsole->isVisible() ? "Hide ⬆" : "Show Logs ⬇");
        versionLabel->setText("Current Version: 0.1.0");
        updateButton->setText("Update System");
        slide1Title->setText("Update Your System With One Click");
        slide1Desc->setText("Updates all system packages, including flatpak and snap.");
        slide2Title->setText("Follow Us on Social Media");
        websiteBtn->setText("🌐\n\nWebsite");
        slide3Title->setText("Discover Our App Store");
        slide3Desc->setText("Browse our custom application store.");
        appStoreSlideBtn->setText("Go to App Store");
        appStoreTitleLabel->setText("Explore Our Custom App Store");
        appStoreOpenAppBtn->setText("Open App / Store");
        slide4Title->setText("Who Are We? Discover Our Community");
        slide4Desc->setText("As the Bozok Community, we work for Open Source.");
        bozokBtn->setText("Join Community");
        slide5Title->setText("Game Package");
        slide5Desc->setText("Go to the Steam and Lutris installation page.");
        gamePackageSlideBtn->setText("Open Game Screen");
        gameInstallButton->setText("Download All");
        gameStatusLabel->setText("Start Download");
        slide6Title->setText("Office Programs");
        slide6Desc->setText("Perform office installations according to your needs.");
        officePackageSlideBtn->setText("Open Office Screen");
        officeInstallButton->setText("Start Installation");
        officeStatusLabel->setText("Download Selected");
        officeLevelComboBox->setItemText(0, "LibreOffice (Basic)");
        officeLevelComboBox->setItemText(1, "LibreOffice (Full)");
        officeLevelComboBox->setItemText(2, "OnlyOffice");
        officeLevelComboBox->setItemText(3, "WPS Office");
    }

    logConsole->setPlaceholderText(currentLang == TR ? "Log kayıtları / Logs..." : (currentLang == ES ? "Registros / Logs..." : "Logs..."));
    gameLogConsole->setPlaceholderText(currentLang == TR ? "Log kayıtları / Logs..." : (currentLang == ES ? "Registros / Logs..." : "Logs..."));
    officeLogConsole->setPlaceholderText(currentLang == TR ? "Log kayıtları / Logs..." : (currentLang == ES ? "Registros / Logs..." : "Logs..."));
    
    roAsdGitHubBtn->setText("📦\n\nro-asd GitHub");
    roAssistGitHubBtn->setText("🚀\n\nro-Assist GitHub");

    onOfficeLevelChanged(officeLevelComboBox->currentIndex());
    setInitialUpdateStatus();
}

void MainWindow::setInitialUpdateStatus()
{
    if (updateProcess->state() != QProcess::NotRunning || transactionPhaseStarted) return;
    
    if (currentLang == TR) statusLabel->setText("Güncellemeler denetleniyor...");
    else if (currentLang == ES) statusLabel->setText("Buscando actualizaciones...");
    else statusLabel->setText("Checking for updates...");
}

void MainWindow::setupStyle()
{
    QString baseBg = currentTheme == Dark ? "#121212" : "#f4f5f7";
    QString panelBg = currentTheme == Dark ? "#1e1e1e" : "#ffffff";
    QString textCol = currentTheme == Dark ? "#e0e0e0" : "#333333";
    QString subTextCol = currentTheme == Dark ? "#aaaaaa" : "#666666";
    QString borderCol = currentTheme == Dark ? "#333333" : "#dcdfe6";
    QString btnHover = currentTheme == Dark ? "#2c2c2e" : "#e5e7eb";

    QString style = QString(R"(
        QMainWindow { background-color: %1; }
        QWidget#panelWidget { background-color: %2; border-radius: 12px; border: 1px solid %5; }
        QLabel { color: %3; }
        QLabel#versionLabel { font-size: 16px; font-weight: bold; }
        QLabel#statusLabel { font-size: 16px; color: %4; font-weight: bold; }
        QLabel#slideTitle { font-size: 32px; font-weight: bold; margin-bottom: 10px; color: #0066cc; }
        QLabel#slideDesc { font-size: 18px; color: %4; }
        QTextEdit#logConsole { 
            background-color: %1; color: %3; border: 1px inset %5; 
            border-radius: 6px; font-family: monospace; font-size: 11px;
        }
        QPushButton {
            background-color: %2; border: 1px solid %5;
            border-radius: 8px; padding: 6px 14px; color: %3; font-size: 14px; font-weight: bold;
        }
        QPushButton:hover { background-color: %6; }
        QComboBox {
            background-color: %2; border: 1px solid %5;
            border-radius: 8px; padding: 6px 10px; color: %3; font-size: 14px; font-weight: bold;
        }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView { background-color: %2; color: %3; border: 1px solid %5; selection-background-color: #0066cc; outline: 0px; }
        QPushButton#navButton { background-color: transparent; border: none; color: %3; font-size: 40px; font-weight: bold; }
        QPushButton#navButton:hover { color: #0066cc; }
        QPushButton#backButton { background-color: %2; border: 1px solid %5; color: %3; border-radius: 8px; font-size: 14px; font-weight: bold; }
        QPushButton#backButton:hover { background-color: %6; border: 1px solid #0066cc; color: #0066cc; }
        QPushButton#updateButton { background-color: #00a859; color: white; border: none; font-size: 20px; font-weight: bold; border-radius: 10px; }
        QPushButton#updateButton:hover { background-color: #008f4c; }
        QPushButton#updateButton:disabled { background-color: #8cceaa; color: #f0f0f0; }
        QPushButton#squareSoftButton { border-radius: 20px; font-size: 16px; font-weight: bold; background-color: %2; border: 2px solid %5; color: %3; padding: 10px; }
        QPushButton#squareSoftButton:hover { background-color: %6; border: 2px solid #0066cc; }
        QPushButton#actionButton { background-color: #0066cc; color: white; border: none; font-weight: bold; border-radius: 12px; font-size: 18px; }
        QPushButton#actionButton:hover { background-color: #0052a3; }
        QPushButton#roundButton { border-radius: 60px; font-size: 50px; background-color: %2; border: 3px solid #0066cc; }
        QPushButton#roundButton:hover { background-color: %6; }
        QPushButton#aboutButton { border-radius: 25px; font-size: 20px; border: none; background-color: transparent;}
        QPushButton#aboutButton:hover { background-color: %6; }
        QProgressBar { border: 1px solid %5; border-radius: 6px; text-align: center; background-color: %1; color: %3; font-weight: bold; }
        QProgressBar::chunk { background-color: #00a859; width: 10px; }
    )").arg(baseBg, panelBg, textCol, subTextCol, borderCol, btnHover);

    setStyleSheet(style);
}

void MainWindow::nextSlide()
{
    int next = (carousel->currentIndex() + 1) % carousel->count();
    carousel->setCurrentIndex(next);
    carouselTimer->start(5000);
}

void MainWindow::prevSlide()
{
    int prev = (carousel->currentIndex() - 1 + carousel->count()) % carousel->count();
    carousel->setCurrentIndex(prev);
    carouselTimer->start(5000);
}

void MainWindow::showUpdateScreen() { mainStack->setCurrentIndex(1); carouselTimer->stop(); }
void MainWindow::showGameScreen() { mainStack->setCurrentIndex(2); carouselTimer->stop(); }
void MainWindow::showAppStoreScreen() { mainStack->setCurrentIndex(3); carouselTimer->stop(); }
void MainWindow::showOfficeScreen() { mainStack->setCurrentIndex(4); carouselTimer->stop(); }
void MainWindow::showCarouselScreen()
{
    mainStack->setCurrentIndex(0);
    carouselTimer->start(5000);
    if (!transactionPhaseStarted && checkUpdateProcess->state() == QProcess::NotRunning) {
        setInitialUpdateStatus();
        checkUpdateProcess->start("dnf", QStringList() << "check-update");
    }
}

void MainWindow::changeLanguage(int index)
{
    currentLang = static_cast<Language>(langComboBox->itemData(index).toInt());
    updateUiTextAndImages();
}

void MainWindow::toggleTheme()
{
    currentTheme = (currentTheme == Light) ? Dark : Light;
    updateUiTextAndImages();
    setupStyle();
}

void MainWindow::toggleUpdateLogs()
{
    logConsole->setVisible(!logConsole->isVisible());
    updateUiTextAndImages();
}

void MainWindow::toggleGameLogs()
{
    gameLogConsole->setVisible(!gameLogConsole->isVisible());
    updateUiTextAndImages();
}

void MainWindow::toggleOfficeLogs()
{
    officeLogConsole->setVisible(!officeLogConsole->isVisible());
    updateUiTextAndImages();
}

void MainWindow::onNetworkConnectedChanged(bool isConnected)
{
    isNetworkConnected = isConnected;
    networkStatusLabel->setVisible(!isNetworkConnected);
}

void MainWindow::openWebsite() { QDesktopServices::openUrl(QUrl("https://www.example.com")); }
void MainWindow::openRoAsdGitHub() { QDesktopServices::openUrl(QUrl("https://github.com/example/ro-asd")); }
void MainWindow::openRoAssistGitHub() { QDesktopServices::openUrl(QUrl("https://github.com/example/ro-Assist")); }
void MainWindow::openBozokCommunity() { QDesktopServices::openUrl(QUrl("https://github.com/Bozok-Toplulugu")); }
void MainWindow::showAboutDialog() { QMessageBox::about(this, "Hakkında", "Geliştirici: Ebubekir Bulut\nYıl: 2026"); }

void MainWindow::dummyAppStoreAction()
{
    QMessageBox::information(this, currentLang == TR ? "Mağaza Sürümü" : (currentLang == ES ? "Versión de la Tienda" : "Store Version"), 
                             currentLang == TR ? "Bu sistem yakında eklenecektir!" : (currentLang == ES ? "¡Integración próximamente!" : "Integration coming soon!"));
}

void MainWindow::startGamePackageInstall()
{
    if (!isNetworkConnected) {
        QMessageBox::warning(this, currentLang == TR ? "Hata" : (currentLang == ES ? "Error" : "Error"), 
                             currentLang == TR ? "İnternet bağlantısı yok!" : (currentLang == ES ? "¡Sin conexión a internet!" : "No internet connection!"));
        return;
    }
    
    bool ok;
    QString password = QInputDialog::getText(this, "Sudo", currentLang == TR ? "Lütfen sudo şifrenizi girin:" : (currentLang == ES ? "Ingrese su contraseña de sudo:" : "Please enter your sudo password:"), QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;

    gameInstallButton->setEnabled(false);
    gameProgressBar->setRange(0, 100);
    gameProgressBar->setValue(0);
    gameProgressBar->show();
    gameStatusLabel->setText(currentLang == TR ? "Oyun paketleri kuruluyor..." : (currentLang == ES ? "Instalando paquetes..." : "Installing packages..."));
    
    gameLogConsole->clear();
    appendLog(currentLang == TR ? "Oyun paketleri kuruluyor..." : (currentLang == ES ? "Instalando paquetes de juegos..." : "Installing game packages..."), "#0066cc");
    
    if (updateProcess->state() != QProcess::NotRunning) return;

    transactionPhaseStarted = false;
    isUpToDate = false;
    isTerminatingIntentionally = false;

    updateProcess->start("sudo", QStringList() << "-S" << "dnf" << "install" << "-y" << "steam" << "lutris");
    updateProcess->write((password + "\n").toUtf8());
}

void MainWindow::startOfficePackageInstall()
{
    if (!isNetworkConnected) {
        QMessageBox::warning(this, currentLang == TR ? "Hata" : (currentLang == ES ? "Error" : "Error"), 
                             currentLang == TR ? "İnternet bağlantısı yok!" : (currentLang == ES ? "¡Sin conexión a internet!" : "No internet connection!"));
        return;
    }
    
    bool ok;
    QString password = QInputDialog::getText(this, "Sudo", currentLang == TR ? "Lütfen sudo şifrenizi girin:" : (currentLang == ES ? "Ingrese su contraseña de sudo:" : "Please enter your sudo password:"), QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;

    officeInstallButton->setEnabled(false);
    officeLevelComboBox->setEnabled(false);
    officeProgressBar->setRange(0, 100);
    officeProgressBar->setValue(0);
    officeProgressBar->show();
    officeStatusLabel->setText(currentLang == TR ? "Ofis paketleri kuruluyor..." : (currentLang == ES ? "Instalando la oficina..." : "Installing office..."));
    
    officeLogConsole->clear();
    appendLog(currentLang == TR ? "Ofis paketleri kuruluyor..." : "Installing office packages...", "#0066cc");
    
    if (updateProcess->state() != QProcess::NotRunning) return;

    transactionPhaseStarted = false;
    isUpToDate = false;
    isTerminatingIntentionally = false;

    int enumVal = officeLevelComboBox->itemData(officeLevelComboBox->currentIndex()).toInt();
    QStringList args;
    args << "-S" << "dnf" << "install" << "-y";
    
    if (enumVal == LibreOfficeBasic) {
        args << "libreoffice-writer" << "libreoffice-calc" << "libreoffice-impress";
    } else if (enumVal == LibreOfficeFull) {
        args << "libreoffice" << "evince";
    } else if (enumVal == OnlyOffice) {
        args.clear();
        args << "-S" << "sh" << "-c" << "if command -v flatpak > /dev/null; then flatpak install flathub org.onlyoffice.desktopeditors -y; else echo 'Flatpak is missing. Install Flatpak first.'; exit 1; fi";
    } else if (enumVal == WPSOffice) {
        args.clear();
        args << "-S" << "sh" << "-c" << "if command -v flatpak > /dev/null; then flatpak install flathub com.wps.Office -y; else echo 'Flatpak is missing. Install Flatpak first.'; exit 1; fi";
    }

    updateProcess->start("sudo", args);
    updateProcess->write((password + "\n").toUtf8());
}

void MainWindow::startUpdate()
{
    if (!isNetworkConnected) {
        QMessageBox::warning(this, currentLang == TR ? "Hata" : (currentLang == ES ? "Error" : "Error"), 
                             currentLang == TR ? "İnternet bağlantısı yok!" : (currentLang == ES ? "¡Sin conexión a internet!" : "No internet connection!"));
        return;
    }

    bool ok;
    QString password = QInputDialog::getText(this, "Sudo", currentLang == TR ? "Lütfen sudo şifrenizi girin:" : (currentLang == ES ? "Ingrese su contraseña de sudo:" : "Please enter your sudo password:"), QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;

    updateButton->setEnabled(false);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->show();
    
    statusLabel->setText(currentLang == TR ? "Güncelleme İşlemi Başlıyor..." : (currentLang == ES ? "Iniciando Actualización..." : "Update Starting..."));
    logConsole->clear();
    appendLog(currentLang == TR ? "Sistem güncellemesi başlatılıyor..." : (currentLang == ES ? "Comenzando actualización del sistema..." : "System update starting..."), "#0066cc");

    transactionPhaseStarted = false;
    isUpToDate = false;
    isTerminatingIntentionally = false;

    QString cmd = "dnf upgrade -y; "
                  "if command -v flatpak > /dev/null; then flatpak update -y; fi; "
                  "if command -v snap > /dev/null; then snap refresh; fi";

    updateProcess->start("sudo", QStringList() << "-S" << "sh" << "-c" << cmd);
    updateProcess->write((password + "\n").toUtf8());
}

void MainWindow::appendLog(const QString &text, const QString &color)
{
    QString formattedText = QString("<span style='color:%1'>%2</span>").arg(color, text.toHtmlEscaped());
    int actIdx = mainStack->currentIndex();
    if (actIdx == 2) {
        gameLogConsole->append(formattedText);
        QScrollBar *sb = gameLogConsole->verticalScrollBar();
        sb->setValue(sb->maximum());
    } else if (actIdx == 4) {
        officeLogConsole->append(formattedText);
        QScrollBar *sb = officeLogConsole->verticalScrollBar();
        sb->setValue(sb->maximum());
    } else {
        logConsole->append(formattedText);
        QScrollBar *sb = logConsole->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}

void MainWindow::checkDnfErrors(const QString &output)
{
    int actIdx = mainStack->currentIndex();
    if (output.contains("Waiting for process", Qt::CaseInsensitive) || output.contains("Another app is currently holding the yum lock", Qt::CaseInsensitive)) {
        QString msg = currentLang == TR ? "⚠️ Sistem şu an başka bir işlemle meşgul..." : (currentLang == ES ? "⚠️ El sistema está ocupado..." : "⚠️ System is busy...");
        if (actIdx == 2) gameStatusLabel->setText(msg);
        else if (actIdx == 4) officeStatusLabel->setText(msg);
        else statusLabel->setText(msg);
    }
    if (output.contains("Error: Failed to download metadata", Qt::CaseInsensitive) || output.contains("Could not resolve host", Qt::CaseInsensitive)) {
        QString msg = currentLang == TR ? "🌐 İnternet Hatası" : (currentLang == ES ? "🌐 Error de red" : "🌐 Network Error");
        if (actIdx == 2) {
            gameStatusLabel->setText(msg);
            gameInstallButton->setEnabled(true);
        } else if (actIdx == 4) {
            officeStatusLabel->setText(msg);
            officeInstallButton->setEnabled(true);
            officeLevelComboBox->setEnabled(true);
        } else {
            statusLabel->setText(msg);
            updateButton->setEnabled(true);
        }
    }
}

void MainWindow::handleUpdateOutput()
{
    QString output = QString::fromUtf8(updateProcess->readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        appendLog(line, currentTheme == Dark ? "#cccccc" : "#666666");
    }
    checkDnfErrors(output);

    if (output.contains("Nothing to do", Qt::CaseInsensitive) || output.contains("Yapılacak bir şey yok", Qt::CaseInsensitive)) {
        isUpToDate = true;
    }

    QRegularExpression txRegex("\\((\\d+)/(\\d+)\\)");
    QRegularExpressionMatchIterator i = txRegex.globalMatch(output);
    int actIdx = mainStack->currentIndex();
    
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        transactionPhaseStarted = true;
        int current = match.captured(1).toInt();
        int total = match.captured(2).toInt();
        QString t = QString("%1 (%2/%3)").arg(currentLang == TR ? "Paketler kuruluyor..." : (currentLang == ES ? "Instalando paquetes..." : "Installing packages...")).arg(current).arg(total);
        if (actIdx == 2) {
            gameProgressBar->setRange(0, total);
            gameProgressBar->setValue(current);
            gameStatusLabel->setText(t);
        } else if (actIdx == 4) {
            officeProgressBar->setRange(0, total);
            officeProgressBar->setValue(current);
            officeStatusLabel->setText(t);
        } else {
            progressBar->setRange(0, total);
            progressBar->setValue(current);
            statusLabel->setText(t);
        }
    }

    if (!transactionPhaseStarted) {
        QRegularExpression dlRegex("(\\d+)%");
        QRegularExpressionMatchIterator j = dlRegex.globalMatch(output);
        int lastPercent = -1;
        while (j.hasNext()) {
            QRegularExpressionMatch match = j.next();
            lastPercent = match.captured(1).toInt();
        }
        if (lastPercent != -1) {
            QString t = QString("%1 %2%").arg(currentLang == TR ? "İndiriliyor..." : (currentLang == ES ? "Descargando..." : "Downloading...")).arg(lastPercent);
            if (actIdx == 2) {
                gameProgressBar->setRange(0, 100);
                gameProgressBar->setValue(lastPercent);
                gameStatusLabel->setText(t);
            } else if (actIdx == 4) {
                officeProgressBar->setRange(0, 100);
                officeProgressBar->setValue(lastPercent);
                officeStatusLabel->setText(t);
            } else {
                progressBar->setRange(0, 100);
                progressBar->setValue(lastPercent);
                statusLabel->setText(t);
            }
        }
    }
}

void MainWindow::handleUpdateErrorOutput()
{
    QString errorOutput = QString::fromUtf8(updateProcess->readAllStandardError());
    QStringList lines = errorOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (!line.contains("[sudo]")) {
            appendLog(line, "#cc7700");
        }
    }
    checkDnfErrors(errorOutput);

    if (errorOutput.contains("standard input:1", Qt::CaseInsensitive) || errorOutput.contains("incorrect password", Qt::CaseInsensitive) || errorOutput.contains("try again", Qt::CaseInsensitive)) {
        isTerminatingIntentionally = true;
        updateProcess->terminate(); 
        QString t = currentLang == TR ? "❌ Şifre Yanlış!" : (currentLang == ES ? "❌ ¡Contraseña incorrecta!" : "❌ Wrong Password!");
        int actIdx = mainStack->currentIndex();
        if (actIdx == 2) {
            gameProgressBar->hide();
            gameProgressBar->setValue(0);
            gameStatusLabel->setText(t);
            gameInstallButton->setEnabled(true);
            if (!gameLogConsole->isVisible()) toggleGameLogs();
        } else if (actIdx == 4) {
            officeProgressBar->hide();
            officeProgressBar->setValue(0);
            officeStatusLabel->setText(t);
            officeInstallButton->setEnabled(true);
            officeLevelComboBox->setEnabled(true);
            if (!officeLogConsole->isVisible()) toggleOfficeLogs();
        } else {
            progressBar->hide();
            progressBar->setValue(0);
            statusLabel->setText(t);
            updateButton->setEnabled(true);
            if (!logConsole->isVisible()) toggleUpdateLogs();
        }
        QMessageBox::critical(this, currentLang == TR ? "Hata" : (currentLang == ES ? "Error" : "Error"), currentLang == TR ? "Yanlış şifre girdiniz veya sudo yetkiniz yok!" : (currentLang == ES ? "¡Contraseña incorrecta o sin permisos de sudo!" : "Wrong password or no sudo permission!"));
    }
}

void MainWindow::handleCheckUpdateFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (transactionPhaseStarted) return; 
    if (exitStatus == QProcess::NormalExit && exitCode == 100) {
        if (currentLang == TR) statusLabel->setText("Sistem Güncellemesi Mevcut");
        else if (currentLang == ES) statusLabel->setText("Actualización del Sistema Disponible");
        else statusLabel->setText("System Update Available");
    } else {
        if (currentLang == TR) statusLabel->setText("Sisteminiz şu anda güncel!");
        else if (currentLang == ES) statusLabel->setText("¡Su sistema está actualizado!");
        else statusLabel->setText("Your system is currently up to date!");
    }
}

void MainWindow::handleUpdateFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    int actIdx = mainStack->currentIndex();
    if (actIdx == 2) {
        gameInstallButton->setEnabled(true);
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            gameProgressBar->setRange(0, 100);
            gameProgressBar->setValue(100);
            gameStatusLabel->setText(currentLang == TR ? "✅ İşlem Başarıyla Tamamlandı!" : (currentLang == ES ? "✅ ¡Completado con Éxito!" : "✅ Process Completed!"));
            appendLog(currentLang == TR ? "Oyun paketleri kuruldu." : (currentLang == ES ? "Juegos instalados." : "Game packages installed."), "#00cc00");
        } else {
            if (!isTerminatingIntentionally) {
                gameProgressBar->hide();
                gameStatusLabel->setText(currentLang == TR ? "İşlem Başarısız!" : (currentLang == ES ? "¡Proceso fallido!" : "Process Failed!"));
                appendLog(currentLang == TR ? "İşlem sırasında hata oluştu." : (currentLang == ES ? "Ocurrió un error en el proceso." : "Error occurred in process."), "#ff4444");
                if (!gameLogConsole->isVisible()) toggleGameLogs();
            }
        }
    } else if (actIdx == 4) {
        officeInstallButton->setEnabled(true);
        officeLevelComboBox->setEnabled(true);
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            officeProgressBar->setRange(0, 100);
            officeProgressBar->setValue(100);
            officeStatusLabel->setText(currentLang == TR ? "✅ İşlem Başarıyla Tamamlandı!" : (currentLang == ES ? "✅ ¡Completado con Éxito!" : "✅ Process Completed!"));
            appendLog(currentLang == TR ? "Ofis paketleri kuruldu." : "Office packages installed.", "#00cc00");
        } else {
            if (!isTerminatingIntentionally) {
                officeProgressBar->hide();
                officeStatusLabel->setText(currentLang == TR ? "İşlem Başarısız!" : (currentLang == ES ? "¡Proceso fallido!" : "Process Failed!"));
                appendLog(currentLang == TR ? "İşlem sırasında hata oluştu." : "Error occurred in process.", "#ff4444");
                if (!officeLogConsole->isVisible()) toggleOfficeLogs();
            }
        }
    } else {
        updateButton->setEnabled(true);
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            progressBar->setRange(0, 100);
            progressBar->setValue(100);
            statusLabel->setText(currentLang == TR ? "✅ İşlem Başarıyla Tamamlandı!" : (currentLang == ES ? "✅ ¡Completado con Éxito!" : "✅ Process Completed!"));
            appendLog(currentLang == TR ? "Tüm işlemler başarıyla tamamlandı." : (currentLang == ES ? "Todas las operaciones se completaron con éxito." : "All operations completed successfully."), "#00cc00");
        } else {
            if (!isTerminatingIntentionally) {
                progressBar->hide();
                statusLabel->setText(currentLang == TR ? "İşlem Başarısız!" : (currentLang == ES ? "¡Proceso fallido!" : "Process Failed!"));
                appendLog(currentLang == TR ? "İşlem sırasında hata oluştu." : (currentLang == ES ? "Ocurrió un error durante el proceso." : "Error occurred during process."), "#ff4444");
                if (!logConsole->isVisible()) toggleUpdateLogs();
            }
        }
    }
}

void MainWindow::handleUpdateProcessError(QProcess::ProcessError error)
{
    if (isTerminatingIntentionally) return;
    
    QString errorMsg = (error == QProcess::FailedToStart) 
        ? (currentLang == TR ? "Bileşen başlatılamadı." : (currentLang == ES ? "El componente no pudo iniciarse." : "Component failed to start."))
        : (currentLang == TR ? "Bileşen çöktü." : (currentLang == ES ? "El componente falló." : "Component crashed."));
        
    QMessageBox::critical(this, currentLang == TR ? "Kritik Hata" : (currentLang == ES ? "Error crítico" : "Critical Error"), errorMsg);
    
    int actIdx = mainStack->currentIndex();
    if (actIdx == 2) {
        gameProgressBar->hide();
        gameInstallButton->setEnabled(true);
        gameStatusLabel->setText((currentLang == TR ? "Kritik Hata: " : (currentLang == ES ? "Error crítico: " : "Critical Error: ")) + errorMsg);
        if (!gameLogConsole->isVisible()) toggleGameLogs();
    } else if (actIdx == 4) {
        officeProgressBar->hide();
        officeInstallButton->setEnabled(true);
        officeLevelComboBox->setEnabled(true);
        officeStatusLabel->setText((currentLang == TR ? "Kritik Hata: " : (currentLang == ES ? "Error crítico: " : "Critical Error: ")) + errorMsg);
        if (!officeLogConsole->isVisible()) toggleOfficeLogs();
    } else {
        progressBar->hide();
        updateButton->setEnabled(true);
        statusLabel->setText((currentLang == TR ? "Kritik Hata: " : (currentLang == ES ? "Error crítico: " : "Critical Error: ")) + errorMsg);
        if (!logConsole->isVisible()) toggleUpdateLogs();
    }
}
