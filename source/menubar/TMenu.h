#pragma once

#include <QMenuBar>
#include <QFileSystemModel>
#include <QTreeView>



class TMenuBar : public QMenuBar {

	Q_OBJECT
public:
    TMenuBar(QWidget* parent = nullptr);

    QAction* newAction;
    QAction* openFileAction;
    QAction* openFolderAction;
    QAction* saveAction;
    QAction* saveAsAction;
    QAction* SettingsAction;
    QAction* exitAction;
    QAction* runAction;
    QAction* aboutAction;
    QAction* undoAct;
    QAction* redoAct;
    QAction* cutAct;
    QAction* copyAct;
    QAction* pasteAct;
    QAction* findAct;
    QAction* replaceAct;
    // أزرار قائمة عرض
    QAction* toggleSidebarAct;
    QAction* toggleConsoleAct;
    QAction* fullScreenAct;

signals:
    void newRequested();
    void openFileRequested();
    void openFolderRequested();
    void saveRequested();
    void saveAsRequested();
    void settingsRequest();
    void exitRequested();
    void runRequested();
    void aboutRequested();
    void updateRequested();
};
