#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QProcess>
#include <QDir>
#include <QVBoxLayout>

class Terminal : public QWidget {
    Q_OBJECT

public:
    explicit Terminal(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QTextEdit* terminalDisplay;
    QString currentPath;
    int commandStartPosition;

    void executeCommand(const QString& command);
    void handleCdCommand(const QString& path);
    void insertPrompt(bool initial = false);
};