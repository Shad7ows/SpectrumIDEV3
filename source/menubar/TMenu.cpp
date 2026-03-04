#include "TMenu.h"

TMenuBar::TMenuBar(QWidget* parent) {

    parent->setStyleSheet(R"(
        QMenuBar {
            color: #dddddd;
            background-color: #1e202e;
            spacing: 5px; /* spacing between menu bar items */
        }

        QMenuBar::item {
            padding: 3px 7px;
            background: transparent;
            border-radius: 1px;
        }

        QMenuBar::item:selected { /* when selected using mouse or keyboard */
            background: #303349;
        }

        QMenuBar::item:pressed {
            background: #373a54;
        }
    )");

    QMenu* fileMenu = addMenu("ملف");
    QMenu* editMenu = addMenu("تحرير");
    QMenu* runMenu = addMenu("تشغيل");
    QMenu* helpMenu = addMenu("مساعدة");

    fileMenu->setMinimumWidth(200);
    editMenu->setMinimumWidth(200);
    runMenu->setMinimumWidth(200);
    helpMenu->setMinimumWidth(200);

    QAction* newAction = new QAction("جديد", parent);
    QAction* openFileAction = new QAction("فتح ملف", parent);
    QAction* openFolderAction = new QAction("فتح مجلد", parent);
    QAction* saveAction = new QAction("حفظ", parent);
    QAction* saveAsAction = new QAction("حفظ باسم", parent);
    QAction* SettingsAction = new QAction("الإعدادات", parent);
    QAction* exitAction = new QAction("خروج", parent);

    QAction* runAction = new QAction("تشغيل", parent);

    QAction* aboutAction = new QAction("عن المحرر", parent);
    QAction* updateAction = new QAction("البحث عن تحديثات", parent);

    undoAct = new QAction(tr("تراجع (Undo)"), this);
    redoAct = new QAction(tr("إعادة (Redo)"), this);
    cutAct = new QAction(tr("قص (Cut)"), this);
    copyAct = new QAction(tr("نسخ (Copy)"), this);
    pasteAct = new QAction(tr("لصق (Paste)"), this);
    findAct = new QAction(tr("بحث (Find...)"), this);
    replaceAct = new QAction(tr("استبدال (Replace...)"), this);


    fileMenu->addAction(newAction);
    fileMenu->addAction(openFileAction);
    fileMenu->addAction(openFolderAction);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(SettingsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    runMenu->addAction(runAction);

    helpMenu->addAction(aboutAction);
    helpMenu->addAction(updateAction);

    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    editMenu->addAction(cutAct);
    editMenu->addAction(copyAct);
    editMenu->addAction(pasteAct);
    editMenu->addSeparator();
    editMenu->addAction(findAct);
    editMenu->addAction(replaceAct);

    undoAct->setShortcuts(QKeySequence::Undo);
    redoAct->setShortcuts(QKeySequence::Redo);
    cutAct->setShortcuts(QKeySequence::Cut);
    copyAct->setShortcuts(QKeySequence::Copy);
    pasteAct->setShortcuts(QKeySequence::Paste);
    findAct->setShortcuts(QKeySequence::Find);
    replaceAct->setShortcuts(QKeySequence::Replace);





    QString style = R"(
        QMenu {
            color: #dddddd;
            background-color: #1e202e;
            border-bottom: 1px solid #10a8f4;
            border-left: 1px solid #10a8f4;
            border-radius: 3px;
        }
        QMenu::item {
            border: 5px solid transparent;
        }
        QMenu::item:selected {
            margin-left: 3px;
        }
        QMenu::separator {
            height: 1px;
            background: #303349;
            margin-left: 15px;
            margin-right: 10px;
        }
        QMenu::indicator {
            width: 1px;
            height: 1px;
        }
)";
    fileMenu->setStyleSheet(style);
    editMenu->setStyleSheet(style);
    runMenu->setStyleSheet(style);
    helpMenu->setStyleSheet(style);


    connect(newAction, &QAction::triggered, this, &TMenuBar::newRequested);
    connect(openFileAction, &QAction::triggered, this, &TMenuBar::openFileRequested);
    connect(openFolderAction, &QAction::triggered, this, &TMenuBar::openFolderRequested);
    connect(saveAction, &QAction::triggered, this, &TMenuBar::saveRequested);
    connect(saveAsAction, &QAction::triggered, this, &TMenuBar::saveAsRequested);
    connect(SettingsAction, &QAction::triggered, this, &TMenuBar::settingsRequest);
    connect(exitAction, &QAction::triggered, this, &TMenuBar::exitRequested);

    connect(runAction, &QAction::triggered, this, &TMenuBar::runRequested);

    connect(aboutAction, &QAction::triggered, this, &TMenuBar::aboutRequested);
    connect(updateAction, &QAction::triggered, this, &TMenuBar::updateRequested);


    // connect(undoAct, &QAction::triggered, this, &TMenuBar::undo); // استبدل QPlainTextEdit بنوع المكون الخاص بك إذا كان مختلفاً

    // connect(redoAct, &QAction::triggered, this, &TMenuBar::redo);

    // connect(cutAct, &QAction::triggered, this, &TMenuBar::cut);

    // connect(copyAct, &QAction::triggered, this, &TMenuBar::copy);

    // connect(pasteAct, &QAction::triggered, this, &TMenuBar::paste);

    // connect(findAct, &QAction::triggered, this, &TMenuBar::showFindBar);

    // connect(replaceAct, &QAction::triggered, this, &TMenuBar::showReplaceBar);

}

