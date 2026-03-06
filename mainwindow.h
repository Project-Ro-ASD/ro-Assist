#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QProcess>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
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

private:
    void setupUi();
    void setupStyle();
    void checkDnfErrors(const QString &output);

    QLabel *versionLabel;
    QLabel *statusLabel;
    QPushButton *updateButton;
    QProgressBar *progressBar;
    QProcess *updateProcess;

    bool transactionPhaseStarted;
    bool isUpToDate;
    bool isTerminatingIntentionally;
};

#endif // MAINWINDOW_H
