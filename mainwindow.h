#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QToolButton>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QProcess>
#include <QStackedWidget>
#include <QTimer>
#include <QTextEdit>
#include <QNetworkInformation>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QColor>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum Language { TR, EN, ES };
    enum Theme { Light, Dark };

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
    void handleCheckLibFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void startLibraryPackageInstall();

    void nextSlide();
    void prevSlide();
    void showUpdateScreen();
    void showLibraryScreen();
    void showAppStoreScreen();
    void showCarouselScreen();

    void changeLanguageAction(QAction *action);
    void toggleTheme();
    void onNetworkConnectedChanged(bool isConnected);
    
    void openBozokCommunity();
    void dummyAppStoreAction();

    void toggleUpdateLogs();
    void toggleLibraryLogs();

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
    QLabel *libraryStatusLabel;
    
    QPushButton *updateButton;
    QPushButton *libraryInstallButton;
    
    QProgressBar *progressBar;
    QProgressBar *libraryProgressBar;
    
    QProcess *updateProcess;
    QProcess *checkUpdateProcess;
    QProcess *checkLibProcess;

    // Main layout
    QStackedWidget *mainStack;
    QWidget *carouselViewWidget;
    QWidget *updateViewWidget;
    QWidget *libraryViewWidget;
    QWidget *appStoreViewWidget;

    // New UI Elements
    QStackedWidget *carousel;
    QTimer *carouselTimer;

    QTextEdit *logConsole;
    QTextEdit *libraryLogConsole;

    QPushButton *toggleLogBtn;
    QPushButton *toggleLibraryLogBtn;

    QLabel *networkStatusLabel;
    
    QPushButton *langBtn;
    QMenu *langMenu;
    QPushButton *themeToggleBtn;
    QPushButton *prevSlideBtn;
    QPushButton *nextSlideBtn;
    QPushButton *backToCarouselBtn;
    QPushButton *backFromLibraryBtn;
    QPushButton *backFromAppStoreBtn;

    // Slide specific elements
    QLabel *slide1Title;
    QLabel *slide1Desc;
    
    QLabel *slide2Title;
    QToolButton *websiteBtn;
    QToolButton *roAsdGitHubBtn;
    QToolButton *roAssistGitHubBtn;
    
    QLabel *slide3Title;
    QLabel *slide3Desc;
    QPushButton *appStoreSlideBtn;
    
    QLabel *slide4Title;
    QLabel *slide4Desc;
    QPushButton *bozokBtn;
    
    QLabel *slide5Title;
    QLabel *slide5Desc;
    QPushButton *libraryPackageSlideBtn;

    // App Store View special elements
    QLabel *appStoreTitleLabel;
    QLabel *appStorePlaceholderIcon;
    QPushButton *appStoreOpenAppBtn;

    // State
    Language currentLang;
    Theme currentTheme;
    bool transactionPhaseStarted;
    bool isUpToDate;
    bool isTerminatingIntentionally;
    bool isNetworkConnected;
    bool isLibraryInstalled;
    QColor accentColor;
};

#endif // MAINWINDOW_H
