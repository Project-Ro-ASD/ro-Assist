#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QProcess>
#include <QStackedWidget>
#include <QTimer>
#include <QTextEdit>
#include <QNetworkInformation>
#include <QComboBox>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum Language { TR, EN, ES };
    enum Theme { Light, Dark };
    enum OfficeLevel { LibreOfficeBasic, LibreOfficeFull, OnlyOffice, WPSOffice };

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openWebsite();
    void openRoAsdGitHub();
    void openRoAssistGitHub();
    void showAboutDialog();
    
    void startUpdate();
    void handleUpdateOutput();
    void handleUpdateErrorOutput();
    void handleUpdateFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleUpdateProcessError(QProcess::ProcessError error);

    void handleCheckUpdateFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void startGamePackageInstall();
    void startOfficePackageInstall();
    void onOfficeLevelChanged(int index);

    void nextSlide();
    void prevSlide();
    void showUpdateScreen();
    void showGameScreen();
    void showAppStoreScreen();
    void showOfficeScreen();
    void showCarouselScreen();

    void changeLanguage(int index);
    void toggleTheme();
    void onNetworkConnectedChanged(bool isConnected);
    
    void openBozokCommunity();
    void dummyAppStoreAction();

    void toggleUpdateLogs();
    void toggleGameLogs();
    void toggleOfficeLogs();

private:
    void setupUi();
    void setupStyle();
    void checkDnfErrors(const QString &output);
    void createCarouselSlides();
    void updateUiTextAndImages();
    void detectSystemLanguageAndTheme();
    void appendLog(const QString &text, const QString &color = "#666666");
    void setInitialUpdateStatus();

    QLabel *versionLabel;
    QLabel *statusLabel;
    QLabel *gameStatusLabel;
    QLabel *officeStatusLabel;
    
    QPushButton *updateButton;
    QPushButton *gameInstallButton;
    QPushButton *officeInstallButton;
    
    QProgressBar *progressBar;
    QProgressBar *gameProgressBar;
    QProgressBar *officeProgressBar;
    
    QProcess *updateProcess;
    QProcess *checkUpdateProcess;

    // Main layout
    QStackedWidget *mainStack;
    QWidget *carouselViewWidget;
    QWidget *updateViewWidget;
    QWidget *gameViewWidget;
    QWidget *appStoreViewWidget;
    QWidget *officeViewWidget;

    // New UI Elements
    QStackedWidget *carousel;
    QTimer *carouselTimer;

    QTextEdit *logConsole;
    QTextEdit *gameLogConsole;
    QTextEdit *officeLogConsole;

    QPushButton *toggleLogBtn;
    QPushButton *toggleGameLogBtn;
    QPushButton *toggleOfficeLogBtn;

    QLabel *networkStatusLabel;
    
    QComboBox *langComboBox;
    QPushButton *themeToggleBtn;
    QPushButton *prevSlideBtn;
    QPushButton *nextSlideBtn;
    QPushButton *backToCarouselBtn;
    QPushButton *backFromGameBtn;
    QPushButton *backFromAppStoreBtn;
    QPushButton *backFromOfficeBtn;

    // Slide specific elements
    QLabel *slide1Title;
    QLabel *slide1Desc;
    
    QLabel *slide2Title;
    QPushButton *websiteBtn;
    QPushButton *roAsdGitHubBtn;
    QPushButton *roAssistGitHubBtn;
    
    QLabel *slide3Title;
    QLabel *slide3Desc;
    QPushButton *appStoreSlideBtn;
    
    QLabel *slide4Title;
    QLabel *slide4Desc;
    QPushButton *bozokBtn;
    
    QLabel *slide5Title;
    QLabel *slide5Desc;
    QPushButton *gamePackageSlideBtn;

    // Slide 6 elements
    QLabel *slide6Title;
    QLabel *slide6Desc;
    QPushButton *officePackageSlideBtn;

    // App Store View special elements
    QLabel *appStoreTitleLabel;
    QLabel *appStorePlaceholderIcon;
    QPushButton *appStoreOpenAppBtn;

    // Office View special elements
    QComboBox *officeLevelComboBox;
    QLabel *officeDescLabel;

    // State
    Language currentLang;
    Theme currentTheme;
    bool transactionPhaseStarted;
    bool isUpToDate;
    bool isTerminatingIntentionally;
    bool isNetworkConnected;
};

#endif // MAINWINDOW_H
