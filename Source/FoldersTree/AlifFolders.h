#pragma once

#include <QDockWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <qurl.h>
#include <qmessagebox.h>
#include <qdesktopservices.h>


class FolderTree : public QDockWidget {
    Q_OBJECT

public:
    explicit FolderTree(class AlifEditor* textEditor, QWidget* parent = nullptr);

    // Method to set root path for the file system model
    void setRootPath(const QString& path);

public slots:
    void openFolder();

private:
    void setupConnections();
    QTreeView* treeView;
    QFileSystemModel* fileModel;
    AlifEditor* textEditor;
    QString projectPath;

signals:
    // Signal to emit when a file is selected
    void fileSelected(const QString& filePath);
    // New signal to emit when a text file is opened
    void textFileOpened(const QString& filePath, const QString& content);
    void folderChanged(const QString& newPath);

private slots:
    // Slot to handle file selection
    void onFileDoubleClicked(const QModelIndex& index);
};

