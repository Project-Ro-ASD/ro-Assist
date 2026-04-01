#include "mainwindow.h"
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLocale>
#include <QMessageBox>
#include <QPalette>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollBar>
#include <QStyleHints>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), updateProcess(new QProcess(this)),
      checkUpdateProcess(new QProcess(this)),
      checkLibProcess(new QProcess(this)), carousel(new QStackedWidget(this)),
      carouselTimer(new QTimer(this)), logConsole(new QTextEdit(this)),
      libraryLogConsole(new QTextEdit(this)), transactionPhaseStarted(false),
      isUpToDate(false), isTerminatingIntentionally(false),
      isNetworkConnected(true), isLibraryInstalled(false),
      accentColor(QColor("#0066cc")) {
  detectSystemLanguageAndTheme();

  QNetworkInformation::loadBackendByFeatures(
      QNetworkInformation::Feature::Reachability);
  if (QNetworkInformation::instance()) {
    isNetworkConnected = QNetworkInformation::instance()->reachability() ==
                         QNetworkInformation::Reachability::Online;
    connect(QNetworkInformation::instance(),
            &QNetworkInformation::reachabilityChanged, this,
            [this](QNetworkInformation::Reachability r) {
              onNetworkConnectedChanged(
                  r == QNetworkInformation::Reachability::Online);
            });
  }

  setupUi();
  updateUiTextAndImages();
  setupStyle();

  logConsole->hide();
  libraryLogConsole->hide();
  if (QScreen *screen = QGuiApplication::primaryScreen()) {
    QRect screenGeometry = screen->geometry();
    int width = screenGeometry.width() * 0.66;
    int height = screenGeometry.height() * 0.66;
    resize(width, height);
  }

  connect(updateProcess, &QProcess::readyReadStandardOutput, this,
          &MainWindow::handleUpdateOutput);
  connect(updateProcess, &QProcess::readyReadStandardError, this,
          &MainWindow::handleUpdateErrorOutput);
  connect(updateProcess, &QProcess::finished, this,
          &MainWindow::handleUpdateFinished);
  connect(updateProcess, &QProcess::errorOccurred, this,
          &MainWindow::handleUpdateProcessError);

  connect(checkUpdateProcess, &QProcess::finished, this,
          &MainWindow::handleCheckUpdateFinished);
  connect(checkLibProcess, &QProcess::finished, this,
          &MainWindow::handleCheckLibFinished);

  connect(carouselTimer, &QTimer::timeout, this, &MainWindow::nextSlide);
  carouselTimer->start(5000);

  setInitialUpdateStatus();
  checkUpdateProcess->start("dnf", QStringList() << "check-update");
}

MainWindow::~MainWindow() {
  if (updateProcess && updateProcess->state() != QProcess::NotRunning) {
    updateProcess->terminate();
    if (!updateProcess->waitForFinished(1000)) {
      updateProcess->kill();
      updateProcess->waitForFinished(1000);
    }
  }
  if (checkUpdateProcess &&
      checkUpdateProcess->state() != QProcess::NotRunning) {
    checkUpdateProcess->kill();
    checkUpdateProcess->waitForFinished(1000);
  }
}

void MainWindow::detectSystemLanguageAndTheme() {
  QLocale locale = QLocale::system();
  if (locale.language() == QLocale::Turkish)
    currentLang = TR;
  else if (locale.language() == QLocale::Spanish)
    currentLang = ES;
  else
    currentLang = EN;

  currentTheme = Light;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  if (const QStyleHints *hints = QGuiApplication::styleHints()) {
    if (hints->colorScheme() == Qt::ColorScheme::Dark)
      currentTheme = Dark;
  }
#else
  // Fallback for older Qt versions (Qt < 6.5)
  if (QGuiApplication::palette().color(QPalette::WindowText).lightness() >
      QGuiApplication::palette().color(QPalette::Window).lightness()) {
    currentTheme = Dark;
  }
#endif
}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  // TOP BAR
  QWidget *topBarWidget = new QWidget(this);
  QHBoxLayout *topLayout = new QHBoxLayout(topBarWidget);
  topLayout->setContentsMargins(20, 20, 20, 10);
  networkStatusLabel = new QLabel(this);
  networkStatusLabel->setStyleSheet(
      "font-weight: bold; color: #ffcc00; font-size: 14px;");
  networkStatusLabel->setVisible(!isNetworkConnected);

  topLayout->addWidget(networkStatusLabel);
  topLayout->addStretch();
  themeToggleBtn = new QPushButton(this);

  langBtn = new QPushButton(this);
  langBtn->setCursor(Qt::PointingHandCursor);
  langBtn->setFixedSize(130, 42);

  langMenu = new QMenu(langBtn);
  QAction *actTR = langMenu->addAction("🇹🇷 Türkçe");
  actTR->setData(QVariant::fromValue((int)TR));
  QAction *actEN = langMenu->addAction("🇬🇧 English");
  actEN->setData(QVariant::fromValue((int)EN));
  QAction *actES = langMenu->addAction("🇪🇸 Español");
  actES->setData(QVariant::fromValue((int)ES));

  langBtn->setMenu(langMenu);

  themeToggleBtn->setFixedSize(110, 42);
  connect(themeToggleBtn, &QPushButton::clicked, this,
          &MainWindow::toggleTheme);
  connect(langMenu, &QMenu::triggered, this, &MainWindow::changeLanguageAction);

  topLayout->addWidget(themeToggleBtn);
  topLayout->addWidget(langBtn);
  mainLayout->addWidget(topBarWidget);

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
  carouselLayout->addWidget(prevSlideBtn, 0, Qt::AlignCenter);
  carouselLayout->addSpacing(10);
  carouselLayout->addWidget(carousel, 1);
  carouselLayout->addSpacing(10);
  carouselLayout->addWidget(nextSlideBtn, 0, Qt::AlignCenter);

  // 2. UPDATE VIEW
  updateViewWidget = new QWidget(this);
  QVBoxLayout *updateLayout = new QVBoxLayout(updateViewWidget);
  updateLayout->setContentsMargins(20, 0, 20, 0);
  backToCarouselBtn = new QPushButton(this);
  backToCarouselBtn->setObjectName("backButton");
  backToCarouselBtn->setMinimumSize(120, 40);
  backToCarouselBtn->setCursor(Qt::PointingHandCursor);
  connect(backToCarouselBtn, &QPushButton::clicked, this,
          &MainWindow::showCarouselScreen);

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
  connect(toggleLogBtn, &QPushButton::clicked, this,
          &MainWindow::toggleUpdateLogs);

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

  // 3. LIBRARY PACKAGE VIEW
  libraryViewWidget = new QWidget(this);
  QVBoxLayout *libraryLayout = new QVBoxLayout(libraryViewWidget);
  libraryLayout->setContentsMargins(20, 0, 20, 0);

  backFromLibraryBtn = new QPushButton(this);
  backFromLibraryBtn->setObjectName("backButton");
  backFromLibraryBtn->setMinimumSize(120, 40);
  backFromLibraryBtn->setCursor(Qt::PointingHandCursor);
  connect(backFromLibraryBtn, &QPushButton::clicked, this,
          &MainWindow::showCarouselScreen);

  QHBoxLayout *libraryTopLayout = new QHBoxLayout();
  libraryTopLayout->addWidget(backFromLibraryBtn);
  libraryTopLayout->addStretch();

  QWidget *libraryPanel = new QWidget(this);
  libraryPanel->setObjectName("panelWidget");
  QVBoxLayout *libraryPanelLayout = new QVBoxLayout(libraryPanel);
  libraryPanelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

  libraryStatusLabel = new QLabel("", this);
  libraryStatusLabel->setAlignment(Qt::AlignCenter);
  libraryStatusLabel->setObjectName("statusLabel");
  libraryStatusLabel->setWordWrap(true);
  libraryStatusLabel->setMinimumWidth(400);

  libraryInstallButton = new QPushButton(this);
  libraryInstallButton->setObjectName("updateButton");
  libraryInstallButton->setFixedSize(320, 70);
  libraryInstallButton->setCursor(Qt::PointingHandCursor);
  connect(libraryInstallButton, &QPushButton::clicked, this,
          &MainWindow::startLibraryPackageInstall);

  libraryProgressBar = new QProgressBar(this);
  libraryProgressBar->setRange(0, 100);
  libraryProgressBar->setValue(0);
  libraryProgressBar->hide();
  libraryProgressBar->setFixedWidth(400);

  toggleLibraryLogBtn = new QPushButton(this);
  toggleLibraryLogBtn->setObjectName("backButton");
  toggleLibraryLogBtn->setCursor(Qt::PointingHandCursor);
  connect(toggleLibraryLogBtn, &QPushButton::clicked, this,
          &MainWindow::toggleLibraryLogs);

  libraryLogConsole->setReadOnly(true);
  libraryLogConsole->setObjectName("logConsole");
  libraryLogConsole->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  libraryLogConsole->setMaximumHeight(150);
  libraryLogConsole->setMinimumHeight(100);

  libraryPanelLayout->addSpacing(40);
  libraryPanelLayout->addWidget(libraryStatusLabel, 0, Qt::AlignCenter);
  libraryPanelLayout->addSpacing(20);
  libraryPanelLayout->addWidget(libraryInstallButton, 0, Qt::AlignCenter);
  libraryPanelLayout->addSpacing(20);
  libraryPanelLayout->addWidget(libraryProgressBar, 0, Qt::AlignCenter);
  libraryPanelLayout->addSpacing(10);
  libraryPanelLayout->addWidget(toggleLibraryLogBtn, 0, Qt::AlignCenter);
  libraryPanelLayout->addSpacing(5);
  libraryPanelLayout->addWidget(libraryLogConsole, 1);

  libraryLayout->addLayout(libraryTopLayout);
  libraryLayout->addSpacing(10);
  libraryLayout->addWidget(libraryPanel, 1);

  // 4. CUSTOM APP STORE VIEW
  appStoreViewWidget = new QWidget(this);
  QVBoxLayout *storeLayout = new QVBoxLayout(appStoreViewWidget);
  storeLayout->setContentsMargins(20, 0, 20, 0);

  backFromAppStoreBtn = new QPushButton(this);
  backFromAppStoreBtn->setObjectName("backButton");
  backFromAppStoreBtn->setMinimumSize(120, 40);
  backFromAppStoreBtn->setCursor(Qt::PointingHandCursor);
  connect(backFromAppStoreBtn, &QPushButton::clicked, this,
          &MainWindow::showCarouselScreen);

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
  appStorePlaceholderIcon->setStyleSheet(
      "font-size: 150px; background-color: transparent; border: none;");
  appStoreOpenAppBtn = new QPushButton(this);
  appStoreOpenAppBtn->setObjectName("actionButton");
  appStoreOpenAppBtn->setMinimumSize(320, 70);
  appStoreOpenAppBtn->setCursor(Qt::PointingHandCursor);
  connect(appStoreOpenAppBtn, &QPushButton::clicked, this,
          &MainWindow::dummyAppStoreAction);

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
  mainStack->addWidget(carouselViewWidget); // 0
  mainStack->addWidget(updateViewWidget);   // 1
  mainStack->addWidget(libraryViewWidget);  // 2
  mainStack->addWidget(appStoreViewWidget); // 3

  mainLayout->addWidget(mainStack, 1);

  // BOTTOM BAR
  QWidget *bottomBarWidget = new QWidget(this);
  QHBoxLayout *aboutLayout = new QHBoxLayout(bottomBarWidget);
  aboutLayout->setContentsMargins(20, 10, 20, 20);

  QPushButton *aboutBtn = new QPushButton("ℹ️", this);
  aboutBtn->setObjectName("aboutButton");
  aboutBtn->setFixedSize(50, 50);
  aboutBtn->setCursor(Qt::PointingHandCursor);
  connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::showAboutDialog);

  aboutLayout->addWidget(aboutBtn);
  aboutLayout->addStretch();
  mainLayout->addWidget(bottomBarWidget);
}

void MainWindow::createCarouselSlides() {
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
  updateSlideBtn = new QPushButton(this);
  updateSlideBtn->setObjectName("actionButton");
  updateSlideBtn->setMinimumSize(280, 70);
  updateSlideBtn->setCursor(Qt::PointingHandCursor);
  connect(updateSlideBtn, &QPushButton::clicked, this, &MainWindow::showUpdateScreen);
  l1->addStretch();
  l1->addWidget(slide1Title);
  l1->addSpacing(20);
  l1->addWidget(slide1Desc);
  l1->addSpacing(40);
  l1->addWidget(updateSlideBtn, 0, Qt::AlignCenter);
  l1->addStretch();
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
  websiteBtn = new QToolButton(this);
  roAsdGitHubBtn = new QToolButton(this);
  roAssistGitHubBtn = new QToolButton(this);
  websiteBtn->setObjectName("squareSoftButton");
  roAsdGitHubBtn->setObjectName("squareSoftButton");
  roAssistGitHubBtn->setObjectName("squareSoftButton");

  websiteBtn->setFixedSize(160, 160);
  roAsdGitHubBtn->setFixedSize(160, 160);
  roAssistGitHubBtn->setFixedSize(160, 160);

  websiteBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  roAsdGitHubBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  roAssistGitHubBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

  websiteBtn->setIconSize(QSize(64, 64));
  roAsdGitHubBtn->setIconSize(QSize(64, 64));
  roAssistGitHubBtn->setIconSize(QSize(64, 64));

  websiteBtn->setIcon(QIcon(":/globe.svg"));
  roAsdGitHubBtn->setIcon(QIcon(":/github.svg"));
  roAssistGitHubBtn->setIcon(QIcon(":/github.svg"));
  websiteBtn->setCursor(Qt::PointingHandCursor);
  roAsdGitHubBtn->setCursor(Qt::PointingHandCursor);
  roAssistGitHubBtn->setCursor(Qt::PointingHandCursor);

  socialLayout->addStretch();
  socialLayout->addWidget(websiteBtn);
  socialLayout->addWidget(roAsdGitHubBtn);
  socialLayout->addWidget(roAssistGitHubBtn);
  socialLayout->addStretch();

  connect(websiteBtn, &QPushButton::clicked, this, &MainWindow::openWebsite);
  connect(roAsdGitHubBtn, &QPushButton::clicked, this,
          &MainWindow::openRoAsdGitHub);
  connect(roAssistGitHubBtn, &QPushButton::clicked, this,
          &MainWindow::openRoAssistGitHub);

  l2->addStretch();
  l2->addWidget(slide2Title);
  l2->addSpacing(30);
  l2->addLayout(socialLayout);
  l2->addStretch();
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
  connect(appStoreSlideBtn, &QPushButton::clicked, this,
          &MainWindow::showAppStoreScreen);
  l3->addStretch();
  l3->addWidget(slide3Title);
  l3->addSpacing(20);
  l3->addWidget(slide3Desc);
  l3->addSpacing(40);
  l3->addWidget(appStoreSlideBtn, 0, Qt::AlignCenter);
  l3->addStretch();
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
  connect(bozokBtn, &QPushButton::clicked, this,
          &MainWindow::openBozokCommunity);
  l4->addStretch();
  l4->addWidget(slide4Title);
  l4->addSpacing(20);
  l4->addWidget(slide4Desc);
  l4->addSpacing(40);
  l4->addWidget(bozokBtn, 0, Qt::AlignCenter);
  l4->addStretch();
  carousel->addWidget(slide4);

  // Slide 5: Library Package
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
  libraryPackageSlideBtn = new QPushButton(this);
  libraryPackageSlideBtn->setObjectName("actionButton");
  libraryPackageSlideBtn->setMinimumSize(280, 70);
  libraryPackageSlideBtn->setCursor(Qt::PointingHandCursor);
  connect(libraryPackageSlideBtn, &QPushButton::clicked, this,
          &MainWindow::showLibraryScreen);
  l5->addStretch();
  l5->addWidget(slide5Title);
  l5->addSpacing(20);
  l5->addWidget(slide5Desc);
  l5->addSpacing(40);
  l5->addWidget(libraryPackageSlideBtn, 0, Qt::AlignCenter);
  l5->addStretch();
  carousel->addWidget(slide5);
}

void MainWindow::updateUiTextAndImages() {
  if (currentLang == TR)
    langBtn->setText("🇹🇷 Türkçe");
  else if (currentLang == ES)
    langBtn->setText("🇪🇸 Español");
  else
    langBtn->setText("🇬🇧 English");
  bool isTr = (currentLang == TR);
  bool isEs = (currentLang == ES);

  if (isTr) {
    themeToggleBtn->setText(currentTheme == Dark ? "🌙 Koyu" : "☀️ Açık");
    networkStatusLabel->setText("⚠ İnternet Bağlantısı Yok");
    backToCarouselBtn->setText("⬅ Geri");
    backFromLibraryBtn->setText("⬅ Geri");
    backFromAppStoreBtn->setText("⬅ Geri");
    toggleLogBtn->setText(logConsole->isVisible() ? "Gizle ⬆"
                                                  : "Logları Göster ⬇");
    toggleLibraryLogBtn->setText(
        libraryLogConsole->isVisible() ? "Gizle ⬆" : "Logları Göster ⬇");
    versionLabel->setText("Mevcut Sürüm: 0.1.0");
    updateButton->setText("Sistemi Güncelle");
    slide1Title->setText("Sisteminizi Tek Tuşla Güncelleyin");
    slide1Desc->setText(
        "Sistemdeki tüm paketleri, flatpak ve snap uygulamalarını günceller.");
    updateSlideBtn->setText("🚀 Güncelleme Ekranına Git");
    slide2Title->setText("Sosyal Mecralardan Bizi Takip Edin");
    slide3Title->setText("Uygulama Mağazamızı Keşfedin");
    slide3Desc->setText("Kendi mağazamızdan uygulamalara göz atın.");
    appStoreSlideBtn->setText("Mağazaya Git");
    appStoreTitleLabel->setText("Özel Uygulama Mağazamızı Keşfedin");
    appStoreOpenAppBtn->setText("Mağazayı / Uygulamayı Aç");
    slide4Title->setText("Biz Kimiz? Topluluğumuzu Keşfedin");
    slide4Desc->setText(
        "Yozgat Bozok Üniversitesi Açık Kaynak Yazılım Geliştirme Kulübü "
        "olarak ro-ASD projesi için çalışıyoruz.");
    bozokBtn->setText("Kulübe Katıl");
    slide5Title->setText("Oyun Kütüphanesi");
    slide5Desc->setText(
        "Oyun kütüphanelerini indirebilir ve güncelleyebilirsiniz.");
    libraryPackageSlideBtn->setText("Oyun Kütüphanesi Ekranına Geç");
    libraryStatusLabel->setText("İndirmeyi Başlat");

  } else if (isEs) {
    themeToggleBtn->setText(currentTheme == Dark ? "🌙 Oscuro" : "☀️ Claro");
    networkStatusLabel->setText("⚠ Sin conexión a Internet");
    backToCarouselBtn->setText("⬅ Volver");
    backFromLibraryBtn->setText("⬅ Volver");
    backFromAppStoreBtn->setText("⬅ Volver");
    toggleLogBtn->setText(logConsole->isVisible() ? "Ocultar ⬆"
                                                  : "Mostrar Registros ⬇");
    toggleLibraryLogBtn->setText(
        libraryLogConsole->isVisible() ? "Ocultar ⬆" : "Mostrar Registros ⬇");
    versionLabel->setText("Versión Actual: 0.1.0");
    updateButton->setText("Actualizar Sistema");
    slide1Title->setText("Actualiza tu Sistema con un Clic");
    slide1Desc->setText(
        "Actualiza todos los paquetes del sistema, incluyendo flatpak y snap.");
    updateSlideBtn->setText("🚀 Ir a Pantalla de Actualización");
    slide2Title->setText("Síganos en las Redes Sociales");
    slide3Title->setText("Descubre nuestra Tienda de Apps");
    slide3Desc->setText(
        "Navega hasta nuestra tienda de aplicaciones personalizada.");
    appStoreSlideBtn->setText("Ir a Tienda de Apps");
    appStoreTitleLabel->setText("Descubre Nuestra Tienda Especial");
    appStoreOpenAppBtn->setText("Abrir Aplicación de la Tienda");
    slide4Title->setText("¿Quiénes somos? Nuestra comunidad");
    slide4Desc->setText(
        "Como el Club de Desarrollo de Código Abierto de la Universidad de "
        "Yozgat Bozok, trabajamos para el proyecto ro-ASD.");
    bozokBtn->setText("Únete al Club");
    slide5Title->setText("Biblioteca de Juegos");
    slide5Desc->setText(
        "Puede descargar y actualizar bibliotecas para juegos.");
    libraryPackageSlideBtn->setText("Ir a Pantalla de Biblioteca de Juegos");
    libraryStatusLabel->setText("Iniciar la Descarga");

  } else {
    themeToggleBtn->setText(currentTheme == Dark ? "🌙 Dark" : "☀️ Light");
    networkStatusLabel->setText("⚠ No Internet Connection");
    backToCarouselBtn->setText("⬅ Back");
    backFromLibraryBtn->setText("⬅ Back");
    backFromAppStoreBtn->setText("⬅ Back");
    toggleLogBtn->setText(logConsole->isVisible() ? "Hide ⬆" : "Show Logs ⬇");
    toggleLibraryLogBtn->setText(
        libraryLogConsole->isVisible() ? "Hide ⬆" : "Show Logs ⬇");
    versionLabel->setText("Current Version: 0.1.0");
    updateButton->setText("Update System");
    slide1Title->setText("Update Your System With One Click");
    slide1Desc->setText(
        "Updates all system packages, including flatpak and snap.");
    updateSlideBtn->setText("🚀 Go to Update Screen");
    slide2Title->setText("Follow Us on Social Media");
    slide3Title->setText("Discover Our App Store");
    slide3Desc->setText("Browse our custom application store.");
    appStoreSlideBtn->setText("Go to App Store");
    appStoreTitleLabel->setText("Explore Our Custom App Store");
    appStoreOpenAppBtn->setText("Open App / Store");
    slide4Title->setText("Who Are We? Discover Our Community");
    slide4Desc->setText("As Yozgat Bozok University Open Source Software "
                        "Development Club, we work for the ro-ASD project.");
    bozokBtn->setText("Join the Club");
    slide5Title->setText("Game Library");
    slide5Desc->setText("You can download and update game libraries.");
    libraryPackageSlideBtn->setText("Open Library Screen");
    libraryStatusLabel->setText("Start Download");
  }

  logConsole->setPlaceholderText(
      currentLang == TR
          ? "Log kayıtları / Logs..."
          : (currentLang == ES ? "Registros / Logs..." : "Logs..."));
  libraryLogConsole->setPlaceholderText(
      currentLang == TR
          ? "Log kayıtları / Logs..."
          : (currentLang == ES ? "Registros / Logs..." : "Logs..."));
  roAsdGitHubBtn->setText("ro-ASD OS\nRepo");
  roAssistGitHubBtn->setText("ro-Assist\nRepo");

  websiteBtn->setText(currentLang == TR
                          ? "Web Sitesi"
                          : (currentLang == ES ? "Sitio Web" : "Website"));
  if (isLibraryInstalled) {
    libraryInstallButton->setText(
        currentLang == TR ? "Kütüphaneleri Güncelle"
                          : (currentLang == ES ? "Actualizar bibliotecas"
                                               : "Update Libraries"));
  } else {
    libraryInstallButton->setText(
        currentLang == TR ? "Kütüphaneleri İndir"
                          : (currentLang == ES ? "Descargar bibliotecas"
                                               : "Download Libraries"));
  }

  setInitialUpdateStatus();
}

void MainWindow::setInitialUpdateStatus() {
  if (updateProcess->state() != QProcess::NotRunning || transactionPhaseStarted)
    return;

  if (currentLang == TR)
    statusLabel->setText("Güncellemeler denetleniyor...");
  else if (currentLang == ES)
    statusLabel->setText("Buscando actualizaciones...");
  else
    statusLabel->setText("Checking for updates...");
}

void MainWindow::setupStyle() {
  QString baseBg = currentTheme == Dark ? "#352F44" : "#FBF9F1";
  QString surface = currentTheme == Dark ? "#352F44" : "#FBF9F1";
  QString textCol = currentTheme == Dark ? "#FAF0E6" : "#352F44";
  QString subTextCol = currentTheme == Dark ? "#B9B4C7" : "#5C5470";
  QString borderCol = currentTheme == Dark ? "#5C5470" : "#E5E1DA";
  QString surfaceSoft = currentTheme == Dark ? "#5C5470" : "#E5E1DA";

  QString primary = currentTheme == Dark ? "#5C5470" : "#92C7CF";
  QString accent1 = currentTheme == Dark ? "#352F44" : "#AAD7D9";
  QString accent2 = currentTheme == Dark ? "#B9B4C7" : "#92C7CF";

  QString style =
      QString(R"(
        * { font-family: 'Segoe UI', 'Inter', 'Roboto', sans-serif; outline: none; }
        QMainWindow { background-color: %1; }
        QWidget#panelWidget { 
            background-color: transparent; 
            border: none; 
        }
        QLabel { color: %3; }
        QLabel#versionLabel { font-size: 14px; font-weight: 600; color: %4; }
        QLabel#statusLabel { font-size: 16px; color: %4; font-weight: 600; }
        QLabel#slideTitle { font-size: 34px; font-weight: 800; margin-bottom: 8px; color: %3; }
        QLabel#slideDesc { font-size: 17px; color: %4; line-height: 1.5; }
        
        QTextEdit#logConsole { 
            background-color: %10; color: %3; border: 1px solid %5; 
            border-radius: 10px; font-family: 'Cascadia Code', 'Consolas', monospace; font-size: 12px;
            padding: 8px;
        }
        QScrollBar:vertical {
            border: none; background: %10; width: 10px; border-radius: 5px;
        }
        QScrollBar::handle:vertical { background: %5; border-radius: 5px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        
        QPushButton {
            background-color: %2; border: 1px solid %5;
            border-radius: 10px; padding: 8px 16px; color: %3; font-size: 14px; font-weight: 600;
        }
        QPushButton:hover { background-color: %6; border-color: %7; }
        
        QMenu {
            background-color: %2; color: %3; border: 1px solid %5;
            border-radius: 8px; padding: 4px; font-size: 14px; font-weight: 600; 
        }
        QMenu::item { padding: 8px 24px; border-radius: 6px; }
        QMenu::item:selected { background-color: %7; color: white; }
        
        QPushButton#navButton { background-color: transparent; border: none; color: %4; font-size: 36px; font-weight: bold; }
        QPushButton#navButton:hover { color: %7; }
        
        QPushButton#backButton { background-color: %10; border: 1px solid %5; color: %3; border-radius: 10px; font-size: 14px; font-weight: bold; }
        QPushButton#backButton:hover { background-color: %2; border: 1px solid %7; color: %7; }
        
        QPushButton#updateButton, QPushButton#actionButton { 
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %7, stop:1 %8); 
            color: white; border: none; font-size: 18px; font-weight: 700; border-radius: 14px; 
            padding: 12px;
        }
        QPushButton#updateButton:hover, QPushButton#actionButton:hover { 
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %9, stop:1 %7); 
        }
        QPushButton#updateButton:disabled, QPushButton#actionButton:disabled { 
            background: %5; color: %4; 
        }
        
        QToolButton#squareSoftButton { 
            border-radius: 20px; font-size: 15px; font-weight: 600; 
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %7, stop:1 %8); 
            border: none; color: white; padding: 12px; 
        }
        QToolButton#squareSoftButton:hover { 
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %8, stop:1 %7); 
            color: white; 
        }
        
        QPushButton#roundButton { border-radius: 60px; font-size: 46px; background-color: %10; border: 2px solid %7; }
        QPushButton#roundButton:hover { background-color: %2; border: 2px solid %8; color: %7; }
        
        QPushButton#aboutButton { border-radius: 25px; font-size: 22px; border: none; background-color: transparent; }
        QPushButton#aboutButton:hover { background-color: %6; }
        
        QProgressBar { 
            border: none; border-radius: 8px; text-align: center; 
            background-color: %10; color: transparent; font-weight: bold; height: 16px;
        }
        QProgressBar::chunk { background-color: %7; border-radius: 8px; }
    )")
          .arg(baseBg, surface, textCol, subTextCol, borderCol, surfaceSoft)
          .arg(primary, accent1, accent2, surfaceSoft);

  setStyleSheet(style);
}

void MainWindow::nextSlide() {
  int next = (carousel->currentIndex() + 1) % carousel->count();
  carousel->setCurrentIndex(next);
  carouselTimer->start(5000);
}

void MainWindow::prevSlide() {
  int prev =
      (carousel->currentIndex() - 1 + carousel->count()) % carousel->count();
  carousel->setCurrentIndex(prev);
  carouselTimer->start(5000);
}

void MainWindow::showUpdateScreen() {
  mainStack->setCurrentIndex(1);
  carouselTimer->stop();
}
void MainWindow::showLibraryScreen() {
  mainStack->setCurrentIndex(2);
  carouselTimer->stop();

  libraryInstallButton->setEnabled(false);
  libraryInstallButton->setText(
      currentLang == TR
          ? "Kontrol ediliyor..."
          : (currentLang == ES ? "Comprobando..." : "Checking..."));

  if (checkLibProcess->state() == QProcess::NotRunning) {
    checkLibProcess->start("rpm", QStringList()
                                      << "-q" << "gamemode" << "mangohud"
                                      << "vulkan-loader" << "vulkan-tools");
  }
}
void MainWindow::showAppStoreScreen() {
  mainStack->setCurrentIndex(3);
  carouselTimer->stop();
}
void MainWindow::showCarouselScreen() {
  mainStack->setCurrentIndex(0);
  carouselTimer->start(5000);
  if (!transactionPhaseStarted &&
      checkUpdateProcess->state() == QProcess::NotRunning) {
    setInitialUpdateStatus();
    checkUpdateProcess->start("dnf", QStringList() << "check-update");
  }
}

void MainWindow::changeLanguageAction(QAction *action) {
  if (!action)
    return;
  currentLang = static_cast<Language>(action->data().toInt());
  updateUiTextAndImages();
}

void MainWindow::toggleTheme() {
  currentTheme = (currentTheme == Light) ? Dark : Light;
  updateUiTextAndImages();
  setupStyle();
}

void MainWindow::toggleUpdateLogs() {
  logConsole->setVisible(!logConsole->isVisible());
  updateUiTextAndImages();
}

void MainWindow::toggleLibraryLogs() {
  libraryLogConsole->setVisible(!libraryLogConsole->isVisible());
  updateUiTextAndImages();
}

void MainWindow::onNetworkConnectedChanged(bool isConnected) {
  isNetworkConnected = isConnected;
  networkStatusLabel->setVisible(!isNetworkConnected);
}

void MainWindow::openWebsite() {
  QDesktopServices::openUrl(QUrl("https://github.com/Project-Ro-ASD"));
}
void MainWindow::openRoAsdGitHub() {
  QDesktopServices::openUrl(QUrl("https://github.com/Project-Ro-ASD/ro-asd"));
}
void MainWindow::openRoAssistGitHub() {
  QDesktopServices::openUrl(
      QUrl("https://github.com/Project-Ro-ASD/ro-Assist"));
}
void MainWindow::openBozokCommunity() {
  QDesktopServices::openUrl(QUrl("https://github.com/Project-Ro-ASD"));
}
void MainWindow::showAboutDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle(currentLang == TR
                            ? "Hakkında"
                            : (currentLang == ES ? "Acerca de" : "About"));
  dialog.setFixedSize(550, 400);
  dialog.setStyleSheet(this->styleSheet() +
                       QString(" QDialog { background-color: %1; "
                               "border-radius: 16px; border: 1px solid %2; }")
                           .arg(currentTheme == Dark ? "#352F44" : "#FBF9F1",
                                currentTheme == Dark ? "#5C5470" : "#E5E1DA"));

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(40, 40, 40, 40);
  layout->setSpacing(25);

  QLabel *titleLabel = new QLabel("ro-Assist", &dialog);
  titleLabel->setAlignment(Qt::AlignCenter);
  titleLabel->setStyleSheet(
      QString("font-size: 42px; font-weight: 900; color: %1;")
          .arg(currentTheme == Dark ? "#FAF0E6" : "#352F44"));

  QLabel *descLabel = new QLabel(&dialog);
  descLabel->setWordWrap(true);
  descLabel->setAlignment(Qt::AlignCenter);
  QString desc =
      currentLang == TR
          ? "Yozgat Bozok Üniversitesi Açık Kaynak Yazılım Geliştirme Kulübü "
            "ro-ASD projesi sistem yönetim aracı."
          : (currentLang == ES
                 ? "Herramienta de gestión del sistema del proyecto ro-ASD del "
                   "Club de Desarrollo de Software de Código Abierto de la "
                   "Universidad Yozgat Bozok."
                 : "Yozgat Bozok University Open Source Software Development "
                   "Club ro-ASD project system management tool.");
  descLabel->setText(desc);
  descLabel->setStyleSheet(
      "font-size: 16px; color: " +
      QString(currentTheme == Dark ? "#B9B4C7" : "#5C5470") + ";");

  QLabel *infoLabel = new QLabel(&dialog);
  infoLabel->setAlignment(Qt::AlignCenter);
  infoLabel->setText(
      QString("<span style='font-size: 15px;'><b>%1:</b> Ebubekir "
              "Bulut<br><br><b>%2:</b> 2026</span>")
          .arg(currentLang == TR
                   ? "Geliştirici"
                   : (currentLang == ES ? "Desarrollador" : "Developer"))
          .arg(currentLang == TR ? "Yıl"
                                 : (currentLang == ES ? "Año" : "Year")));
  infoLabel->setStyleSheet(
      "color: " + QString(currentTheme == Dark ? "#B9B4C7" : "#5C5470") + ";");

  QPushButton *okBtn = new QPushButton(
      currentLang == TR ? "Kapat" : (currentLang == ES ? "Cerrar" : "Close"),
      &dialog);
  okBtn->setObjectName("actionButton");
  okBtn->setFixedSize(200, 50);
  okBtn->setCursor(Qt::PointingHandCursor);
  QObject::connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

  layout->addStretch();
  layout->addWidget(titleLabel);
  layout->addSpacing(10);
  layout->addWidget(descLabel);
  layout->addSpacing(20);
  layout->addWidget(infoLabel);
  layout->addStretch();
  layout->addWidget(okBtn, 0, Qt::AlignHCenter);

  dialog.exec();
}

void MainWindow::dummyAppStoreAction() {
  QMessageBox::information(
      this,
      currentLang == TR
          ? "Mağaza Sürümü"
          : (currentLang == ES ? "Versión de la Tienda" : "Store Version"),
      currentLang == TR ? "Bu sistem yakında eklenecektir!"
                        : (currentLang == ES ? "¡Integración próximamente!"
                                             : "Integration coming soon!"));
}

void MainWindow::startLibraryPackageInstall() {
  if (!isNetworkConnected) {
    QMessageBox::warning(
        this,
        currentLang == TR ? "Hata" : (currentLang == ES ? "Error" : "Error"),
        currentLang == TR ? "İnternet bağlantısı yok!"
                          : (currentLang == ES ? "¡Sin conexión a internet!"
                                               : "No internet connection!"));
    return;
  }
  libraryInstallButton->setEnabled(false);
  libraryProgressBar->setRange(0, 100);
  libraryProgressBar->setValue(0);
  libraryProgressBar->show();
  libraryStatusLabel->setText(
      currentLang == TR ? "Kütüphaneler kuruluyor..."
                        : (currentLang == ES ? "Instalando paquetes..."
                                             : "Installing packages..."));

  libraryLogConsole->clear();
  appendLog(currentLang == TR ? "Kütüphaneler kuruluyor..."
                              : (currentLang == ES ? "Instalando bibliotecas..."
                                                   : "Installing libraries..."),
            "#0066cc");

  if (updateProcess->state() != QProcess::NotRunning)
    return;

  transactionPhaseStarted = false;
  isUpToDate = false;
  isTerminatingIntentionally = false;

  QString action = isLibraryInstalled ? "upgrade" : "install";
  updateProcess->start("pkexec", QStringList()
                                     << "dnf" << action << "-y" << "gamemode"
                                     << "mangohud" << "vulkan-loader"
                                     << "vulkan-tools");
}

void MainWindow::startUpdate() {
  if (!isNetworkConnected) {
    QMessageBox::warning(
        this,
        currentLang == TR ? "Hata" : (currentLang == ES ? "Error" : "Error"),
        currentLang == TR ? "İnternet bağlantısı yok!"
                          : (currentLang == ES ? "¡Sin conexión a internet!"
                                               : "No internet connection!"));
    return;
  }
  updateButton->setEnabled(false);
  progressBar->setRange(0, 100);
  progressBar->setValue(0);
  progressBar->show();

  statusLabel->setText(currentLang == TR
                           ? "Güncelleme İşlemi Başlıyor..."
                           : (currentLang == ES ? "Iniciando Actualización..."
                                                : "Update Starting..."));
  logConsole->clear();
  appendLog(currentLang == TR
                ? "Sistem güncellemesi başlatılıyor..."
                : (currentLang == ES ? "Comenzando actualización del sistema..."
                                     : "System update starting..."),
            "#0066cc");

  transactionPhaseStarted = false;
  isUpToDate = false;
  isTerminatingIntentionally = false;

  QString cmd =
      "dnf upgrade -y; "
      "if command -v flatpak > /dev/null; then flatpak update -y; fi; "
      "if command -v snap > /dev/null; then snap refresh; fi";

  updateProcess->start("pkexec", QStringList() << "sh" << "-c" << cmd);
}

void MainWindow::appendLog(const QString &text, const QString &color) {
  QString formattedText = QString("<span style='color:%1'>%2</span>")
                              .arg(color, text.toHtmlEscaped());
  int actIdx = mainStack->currentIndex();
  if (actIdx == 2) {
    libraryLogConsole->append(formattedText);
    QScrollBar *sb = libraryLogConsole->verticalScrollBar();
    sb->setValue(sb->maximum());
  } else {
    logConsole->append(formattedText);
    QScrollBar *sb = logConsole->verticalScrollBar();
    sb->setValue(sb->maximum());
  }
}

void MainWindow::checkDnfErrors(const QString &output) {
  int actIdx = mainStack->currentIndex();
  if (output.contains("Waiting for process", Qt::CaseInsensitive) ||
      output.contains("Another app is currently holding the yum lock",
                      Qt::CaseInsensitive)) {
    QString msg = currentLang == TR
                      ? "⚠️ Sistem şu an başka bir işlemle meşgul..."
                      : (currentLang == ES ? "⚠️ El sistema está ocupado..."
                                           : "⚠️ System is busy...");
    if (actIdx == 2)
      libraryStatusLabel->setText(msg);
    else
      statusLabel->setText(msg);
  }
  if (output.contains("Error: Failed to download metadata",
                      Qt::CaseInsensitive) ||
      output.contains("Could not resolve host", Qt::CaseInsensitive)) {
    QString msg = currentLang == TR ? "🌐 İnternet Hatası"
                                    : (currentLang == ES ? "🌐 Error de red"
                                                         : "🌐 Network Error");
    if (actIdx == 2) {
      libraryStatusLabel->setText(msg);
      libraryInstallButton->setEnabled(true);
    } else {
      statusLabel->setText(msg);
      updateButton->setEnabled(true);
    }
  }
}

void MainWindow::handleUpdateOutput() {
  QString output = QString::fromUtf8(updateProcess->readAllStandardOutput());
  QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    appendLog(line, currentTheme == Dark ? "#cccccc" : "#e60909ff");
  }
  checkDnfErrors(output);

  if (output.contains("Nothing to do", Qt::CaseInsensitive) ||
      output.contains("Yapılacak bir şey yok", Qt::CaseInsensitive)) {
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
    QString t = QString("%1 (%2/%3)")
                    .arg(currentLang == TR
                             ? "Paketler kuruluyor..."
                             : (currentLang == ES ? "Instalando paquetes..."
                                                  : "Installing packages..."))
                    .arg(current)
                    .arg(total);
    if (actIdx == 2) {
      libraryProgressBar->setRange(0, total);
      libraryProgressBar->setValue(current);
      libraryStatusLabel->setText(t);
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
      QString t =
          QString("%1 %2%")
              .arg(currentLang == TR ? "İndiriliyor..."
                                     : (currentLang == ES ? "Descargando..."
                                                          : "Downloading..."))
              .arg(lastPercent);
      if (actIdx == 2) {
        libraryProgressBar->setRange(0, 100);
        libraryProgressBar->setValue(lastPercent);
        libraryStatusLabel->setText(t);
      } else {
        progressBar->setRange(0, 100);
        progressBar->setValue(lastPercent);
        statusLabel->setText(t);
      }
    }
  }
}

void MainWindow::handleUpdateErrorOutput() {
  QString errorOutput =
      QString::fromUtf8(updateProcess->readAllStandardError());
  QStringList lines = errorOutput.split('\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    if (!line.contains("[sudo]")) {
      appendLog(line, "#cc7700");
    }
  }
  checkDnfErrors(errorOutput);

  if (errorOutput.contains("standard input:1", Qt::CaseInsensitive) ||
      errorOutput.contains("incorrect password", Qt::CaseInsensitive) ||
      errorOutput.contains("try again", Qt::CaseInsensitive)) {
    isTerminatingIntentionally = true;
    updateProcess->terminate();
    QString t = currentLang == TR
                    ? "❌ Şifre Yanlış!"
                    : (currentLang == ES ? "❌ ¡Contraseña incorrecta!"
                                         : "❌ Wrong Password!");
    int actIdx = mainStack->currentIndex();
    if (actIdx == 2) {
      libraryProgressBar->hide();
      libraryProgressBar->setValue(0);
      libraryStatusLabel->setText(t);
      libraryInstallButton->setEnabled(true);
      if (!libraryLogConsole->isVisible())
        toggleLibraryLogs();
    } else {
      progressBar->hide();
      progressBar->setValue(0);
      statusLabel->setText(t);
      updateButton->setEnabled(true);
      if (!logConsole->isVisible())
        toggleUpdateLogs();
    }
    QMessageBox::critical(
        this,
        currentLang == TR ? "Hata" : (currentLang == ES ? "Error" : "Error"),
        currentLang == TR
            ? "Yanlış şifre girdiniz veya sudo yetkiniz yok!"
            : (currentLang == ES
                   ? "¡Contraseña incorrecta o sin permisos de sudo!"
                   : "Wrong password or no sudo permission!"));
  }
}

void MainWindow::handleCheckUpdateFinished(int exitCode,
                                           QProcess::ExitStatus exitStatus) {
  if (transactionPhaseStarted)
    return;
  if (exitStatus == QProcess::NormalExit && exitCode == 100) {
    if (currentLang == TR)
      statusLabel->setText("Sistem Güncellemesi Mevcut");
    else if (currentLang == ES)
      statusLabel->setText("Actualización del Sistema Disponible");
    else
      statusLabel->setText("System Update Available");
  } else {
    if (currentLang == TR)
      statusLabel->setText("Sisteminiz şu anda güncel!");
    else if (currentLang == ES)
      statusLabel->setText("¡Su sistema está actualizado!");
    else
      statusLabel->setText("Your system is currently up to date!");
  }
}

void MainWindow::handleCheckLibFinished(int exitCode,
                                        QProcess::ExitStatus exitStatus) {
  if (exitStatus == QProcess::NormalExit && exitCode == 0) {
    isLibraryInstalled = true;
  } else {
    isLibraryInstalled = false;
  }
  roAsdGitHubBtn->setText("ro-ASD OS\nRepo");
  roAssistGitHubBtn->setText("ro-Assist\nRepo");

  websiteBtn->setText(currentLang == TR
                          ? "Web Sitesi"
                          : (currentLang == ES ? "Sitio Web" : "Website"));
  if (isLibraryInstalled) {
    libraryInstallButton->setText(
        currentLang == TR ? "Kütüphaneleri Güncelle"
                          : (currentLang == ES ? "Actualizar bibliotecas"
                                               : "Update Libraries"));
  } else {
    libraryInstallButton->setText(
        currentLang == TR ? "Kütüphaneleri İndir"
                          : (currentLang == ES ? "Descargar bibliotecas"
                                               : "Download Libraries"));
  }
  libraryInstallButton->setEnabled(true);
}

void MainWindow::handleUpdateFinished(int exitCode,
                                      QProcess::ExitStatus exitStatus) {
  int actIdx = mainStack->currentIndex();
  if (actIdx == 2) {
    libraryInstallButton->setEnabled(true);
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
      libraryProgressBar->setRange(0, 100);
      libraryProgressBar->setValue(100);
      libraryStatusLabel->setText(
          currentLang == TR ? "✅ İşlem Başarıyla Tamamlandı!"
                            : (currentLang == ES ? "✅ ¡Completado con Éxito!"
                                                 : "✅ Process Completed!"));
      appendLog(currentLang == TR
                    ? "Kütüphaneler kuruldu."
                    : (currentLang == ES ? "Bibliotecas instaladas."
                                         : "Libraries installed."),
                "#00cc00");
    } else {
      if (!isTerminatingIntentionally) {
        libraryProgressBar->hide();
        libraryStatusLabel->setText(
            currentLang == TR ? "İşlem Başarısız!"
                              : (currentLang == ES ? "¡Proceso fallido!"
                                                   : "Process Failed!"));
        appendLog(currentLang == TR
                      ? "İşlem sırasında hata oluştu."
                      : (currentLang == ES ? "Ocurrió un error en el proceso."
                                           : "Error occurred in process."),
                  "#ff4444");
        if (!libraryLogConsole->isVisible())
          toggleLibraryLogs();
      }
    }
  } else {
    updateButton->setEnabled(true);
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
      progressBar->setRange(0, 100);
      progressBar->setValue(100);
      statusLabel->setText(
          currentLang == TR ? "✅ İşlem Başarıyla Tamamlandı!"
                            : (currentLang == ES ? "✅ ¡Completado con Éxito!"
                                                 : "✅ Process Completed!"));
      appendLog(currentLang == TR
                    ? "Tüm işlemler başarıyla tamamlandı."
                    : (currentLang == ES
                           ? "Todas las operaciones se completaron con éxito."
                           : "All operations completed successfully."),
                "#00cc00");
    } else {
      if (!isTerminatingIntentionally) {
        progressBar->hide();
        statusLabel->setText(currentLang == TR
                                 ? "İşlem Başarısız!"
                                 : (currentLang == ES ? "¡Proceso fallido!"
                                                      : "Process Failed!"));
        appendLog(currentLang == TR
                      ? "İşlem sırasında hata oluştu."
                      : (currentLang == ES
                             ? "Ocurrió un error durante el proceso."
                             : "Error occurred during process."),
                  "#ff4444");
        if (!logConsole->isVisible())
          toggleUpdateLogs();
      }
    }
  }
}

void MainWindow::handleUpdateProcessError(QProcess::ProcessError error) {
  if (isTerminatingIntentionally)
    return;

  QString errorMsg =
      (error == QProcess::FailedToStart)
          ? (currentLang == TR
                 ? "Bileşen başlatılamadı."
                 : (currentLang == ES ? "El componente no pudo iniciarse."
                                      : "Component failed to start."))
          : (currentLang == TR ? "Bileşen çöktü."
                               : (currentLang == ES ? "El componente falló."
                                                    : "Component crashed."));

  QMessageBox::critical(
      this,
      currentLang == TR
          ? "Kritik Hata"
          : (currentLang == ES ? "Error crítico" : "Critical Error"),
      errorMsg);

  int actIdx = mainStack->currentIndex();
  if (actIdx == 2) {
    libraryProgressBar->hide();
    libraryInstallButton->setEnabled(true);
    libraryStatusLabel->setText(
        (currentLang == TR
             ? "Kritik Hata: "
             : (currentLang == ES ? "Error crítico: " : "Critical Error: ")) +
        errorMsg);
    if (!libraryLogConsole->isVisible())
      toggleLibraryLogs();
  } else {
    progressBar->hide();
    updateButton->setEnabled(true);
    statusLabel->setText(
        (currentLang == TR
             ? "Kritik Hata: "
             : (currentLang == ES ? "Error crítico: " : "Critical Error: ")) +
        errorMsg);
    if (!logConsole->isVisible())
      toggleUpdateLogs();
  }
}
