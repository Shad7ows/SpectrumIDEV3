#include "Taif.h"
#include "TWelcomeWindow.h"
#include "TConsole.h"
#include "ProcessWorker.h"
#include "TSearchPanel.h"

#include <QThread>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QShortcut>
#include <QGuiApplication>
#include <QScreen>
#include <QCoreApplication>
#include <QTextStream>
#include <QApplication>
#include <QToolBar>
#include <QHeaderView>
#include <QSettings>
#include <QProcess>
#include <QStyleFactory>
#include <QKeyEvent>
#include <QTimer>
#include <QInputDialog>

Taif::Taif(const QString& filePath, QWidget *parent)
    : QMainWindow(parent)
{

    setAttribute(Qt::WA_DeleteOnClose);

    setting = new TSettings();

    setupUI();
    setupConnections();
    setupStyle();

    installEventFilter(this);

    if (!filePath.isEmpty()) {
        openFile(filePath);
    } else {
        newFile();
    }
}

Taif::~Taif() {
    if (thread) {
        thread->wait();
        thread->quit();
    }
    if (TEditor* editor = currentEditor()) {
        QSettings settings("Alif", "Taif");
        settings.setValue("editorFontSize", editor->font().pixelSize());
        settings.setValue("editorFontType", editor->font().family());
        settings.setValue("editorCodeTheme", setting->getThemeCombo()->currentIndex());
        settings.sync();
    }
}

void Taif::setupUI() {
    tabWidget = new QTabWidget(this);
    tabWidget->setObjectName("MainTabs");
    tabWidget->setDocumentMode(true);
    tabWidget->setTabsClosable(true);
    tabWidget->setMovable(true);

    menuBar = new TMenuBar(this);
    setMenuBar(menuBar);

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    fileTreeView = new QTreeView(this);
    fileSystemModel = new QFileSystemModel(this);

    editorSplitter = new QSplitter(Qt::Vertical, this);
    searchBar = new SearchPanel(this);
    searchBar->hide();

    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeo = screen->availableGeometry();
    int margin = 100;
    int widthFixedNum = 6;
    int x = screenGeo.right() - screenGeo.size().width() + margin * widthFixedNum / 2;
    int y = screenGeo.top() + 30 + margin / 2; // 30 is top system bar height
    int width = screenGeo.size().width() - margin * widthFixedNum;
    int height = screenGeo.size().height() - margin;
    this->setGeometry(x, y, width, height);

    QToolBar *mainToolBar = new QToolBar("Main Toolbar", this);
    mainToolBar->setObjectName("mainToolBar");
    mainToolBar->setMovable(false);
    mainToolBar->setIconSize(QSize(25, 25));
    this->addToolBar(Qt::RightToolBarArea, mainToolBar);

    toggleSidebarAction = new QAction(QIcon(":/icons/resources/panel-right-open.svg"), "فتح القائمة", this);
    toggleSidebarAction->setCheckable(true);
    toggleSidebarAction->setChecked(false);
    mainToolBar->addAction(toggleSidebarAction);

    QAction *runToolbarAction = new QAction(QIcon(":/icons/resources/run.svg"), "تشغيل الملف الحالي", this);

    mainToolBar->addAction(runToolbarAction);
    connect(runToolbarAction, &QAction::triggered, this, &Taif::runAlif);

    fileSystemModel->setRootPath(QDir::homePath());
    fileTreeView->setModel(fileSystemModel);
    fileTreeView->header()->setVisible(false);
    for(int i = 1; i <= 3; ++i) fileTreeView->hideColumn(i);
    fileTreeView->setRootIndex(fileSystemModel->index(QDir::homePath()));
    fileTreeView->setVisible(false);

    consoleTabWidget = new QTabWidget(this);
    consoleTabWidget->setObjectName("consoleTabWidget");
    consoleTabWidget->setDocumentMode(true);
    consoleTabWidget->hide();

    TConsole *cmdConsole = new TConsole(this);
#if defined(Q_OS_LINUX)
    QString terminalName = "طرفية (Bash)";
#elif defined(Q_OS_MACOS)
    QString terminalName = "طرفية (Zsh)";
#else
    QString terminalName = "طرفية (CMD)";
#endif
    consoleTabWidget->addTab(cmdConsole, terminalName);
    cmdConsole->setConsoleRTL();
    cmdConsole->startCmd();

    editorSplitter->addWidget(tabWidget);
    editorSplitter->addWidget(searchBar);
    editorSplitter->addWidget(consoleTabWidget);
    editorSplitter->setSizes({1000, 200});

    mainSplitter->addWidget(fileTreeView);
    mainSplitter->addWidget(editorSplitter);
    mainSplitter->setSizes({200, 700});
    this->setCentralWidget(mainSplitter);

    cursorPositionLabel = new QLabel(this);
    cursorPositionLabel->setStyleSheet("QLabel{ color: #f1f5f9;}");
    cursorPositionLabel->setText("UTF-8  السطر: 1  العمود: 1");
    statusBar()->addPermanentWidget(cursorPositionLabel);
}

void Taif::setupConnections() {

    connect(fileTreeView, &QTreeView::doubleClicked, this, &Taif::onFileTreeDoubleClicked);
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &Taif::closeTab);
    connect(toggleSidebarAction, &QAction::triggered, this, &Taif::toggleSidebar);

    connect(menuBar, &TMenuBar::newRequested, this, &Taif::newFile);
    connect(menuBar, &TMenuBar::openFileRequested, this, [this](){this->openFile("");});
    connect(menuBar, &TMenuBar::saveRequested, this, &Taif::saveFile);
    connect(menuBar, &TMenuBar::saveAsRequested, this, &Taif::saveFileAs);
    connect(menuBar, &TMenuBar::settingsRequest, this, &Taif::openSettings);
    connect(menuBar, &TMenuBar::exitRequested, this, &Taif::exitApp);
    connect(menuBar, &TMenuBar::runRequested, this, &Taif::runAlif);
    connect(menuBar, &TMenuBar::aboutRequested, this, &Taif::aboutTaif);
    connect(menuBar, &TMenuBar::updateRequested, this, &Taif::checkForUpdates);
    connect(menuBar, &TMenuBar::openFolderRequested, this, &Taif::handleOpenFolderMenu);

    connect(tabWidget, &QTabWidget::currentChanged, this, &Taif::updateWindowTitle);
    connect(tabWidget, &QTabWidget::currentChanged, this, &Taif::onCurrentTabChanged);

    connect(searchBar, &SearchPanel::findNext, this, &Taif::findNextText);
    connect(searchBar, &SearchPanel::findText, this, &Taif::findText);
    connect(searchBar, &SearchPanel::findPrevious, this, &Taif::findPrevText);
    connect(searchBar, &SearchPanel::closed, this, &Taif::hideFindBar);

    new QShortcut(QKeySequence::Find, this, SLOT(showFindBar()));
    new QShortcut(QKeySequence::Save, this, SLOT(saveFile()));
    new QShortcut(QKeySequence("Ctrl+G"), this, SLOT(goToLine()));
    new QShortcut(QKeySequence("Ctrl+/"), this, [this](){ if (auto e = currentEditor()) e->toggleComment();});
    new QShortcut(QKeySequence("Ctrl+D"), this, [this](){ if (auto e = currentEditor()) e->duplicateLine();});
    new QShortcut(QKeySequence("Alt+Up"), this, [this](){ if (auto e = currentEditor()) e->moveLineUp(); });
    new QShortcut(QKeySequence("Alt+Down"), this, [this](){ if (auto e = currentEditor()) e->moveLineDown(); });
}

void Taif::setupStyle() {
    QString styleSheet = R"(
        QMainWindow {
            background-color: #0f172a;
            font-size: 13px;
        }

        /* --- تصميم شريط القوائم --- */
        QMenuBar {
            font-family: "Tajawal", "Noto Kufi Arabic";
            background-color: #0f172a;
            color: #f1f5f9;
            padding: 3px;
            font-size: 14px;
        }
        QMenuBar::item {
            background-color: transparent;
            padding: 6px 12px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #1e293b;
            color: #3b82f6;
        }

        /* --- قوائم شريط القوائم --- */
        QMenu {
            font-family: "Tajawal", "Noto Kufi Arabic";
            background-color: #1e293b;
            border: 1px solid #334155;
            color: #f1f5f9;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            background-color: transparent;
            border-radius: 4px;
            padding: 6px 21px 6px 18px;
        }
        QMenu::item:selected {
            background-color: #3b82f6;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: #334155;
            margin: 4px 8px;
        }

        /* --- تصميم شريط الأدوات --- */
        QToolBar {
            background-color: #0f172a;
            border: none;
            padding: 6px;
            spacing: 8px; /* مسافة بين كل زر والآخر */
        }

        /* تصميم أزرار شريط الأدوات */
        QToolBar QToolButton {
            background-color: transparent;
            color: #f1f5f9;
            border: none;
            border-radius: 6px;

            /*  أهم جزء: تحديد حجم مربع الزر ليكون كبيراً ومربعاً */
            min-width: 40px;
            max-width: 40px;
            min-height: 40px;
            max-height: 40px;
        }

        QToolBar QToolButton:hover {
            background-color: #1e293b;
        }

        QToolBar QToolButton:pressed {
            background-color: #334155;
        }

        QToolBar QToolButton:checked {
            background-color: #3b82f6; /* اللون الأزرق */
        }

        /* --- تصميم شجرة المسارات--- */
        QTreeView {
            background-color: #0f172a;
            border: none;
            color: #94a3b8;
            font-size: 14px;
            outline: none;
        }
        QTreeView::item {
            padding: 4px 2px;
        }
        QTreeView::item:hover {
            background-color: #1e293b;
            color: #f1f5f9;
        }
        QTreeView::item:selected {
            background-color: #1e293b;
            color: #3b82f6;
        }

        /* --- تصميم الفاصل --- */
        QSplitter::handle {
            background-color: #094771;
            width: 1px;
        }

        /* --- تصميم التبويبات --- */
        QTabWidget::pane {
            border: none;
            background-color: #0f172a;
        }
        QTabWidget QTabBar { /* شريط التبويبات */
            background-color: #0f172a;
            qproperty-drawBase: 0;
        }
       QTabWidget QTabBar::tab {
            background: #0f172a;
            color: #94a3b8;
            padding: 3px 9px;
            min-width: 100px;
        }
       QTabWidget QTabBar::tab:selected {
            background: #0f172a;
            color: #3b82f6;
            border-bottom: 2px solid #3b82f6;
        }
        QTabWidget QTabBar::tab:hover:!selected {
            background: #334466;
            color: #f1f5f9;
        }
        QTabWidget QTabBar::close-button {
            image: url(:/icons/resources/close.svg);
            background: transparent;
            border-radius: 2px;
            padding: 1px;
            margin: 0px;
        }
        QTabWidget QTabBar::close-button:hover {
            background: #ef4444;
        }

        /* --- Status Bar --- */
        QStatusBar {
            background-color: #0f172a;
            color: #64748b;
            border-top: 1px solid #1e293b;
            font-size: 11px;
            padding-left: 10px;
        }
    )";
    setStyleSheet(styleSheet);
}

void Taif::closeEvent(QCloseEvent *event) {
    int saveResult = needSave();

    if (saveResult == 1) {
        saveFile();
    } else if (saveResult == 0) {
        event->ignore();
        return;
    }

    event->accept();
}

bool Taif::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_F6) {
            toggleConsole();
            return true;
        }
    }
    return QMainWindow::eventFilter(object, event);
}

void Taif::goToLine()
{
    TEditor *editor = currentEditor();
    if (!editor) return;

    bool ok;
    // أقصى رقم هو عدد أسطر الملف الحالي
    int maxLine = editor->blockCount();

    int lineNumber = QInputDialog::getInt(this, "الذهاب إلى سطر",
                                          QString("أدخل رقم السطر (1 - %1):").arg(maxLine),
                                          1, 1, maxLine, 1, &ok);

    if (ok) {
        QTextBlock block = editor->document()->findBlockByNumber(lineNumber - 1);

        if (block.isValid()) {
            QTextCursor cursor = editor->textCursor();

            // تحريك المؤشر الى الكتلة المختارة
            cursor.setPosition(block.position());

            // تحديث المحرر
            editor->setTextCursor(cursor);
            editor->centerCursor();
            editor->setFocus();
        }
    }
}

void Taif::showFindBar() {
    searchBar->show();
    searchBar->setFocusToInput();
}

void Taif::hideFindBar() {
    searchBar->hide();
    if (TEditor* editor = currentEditor()) {
        editor->setFocus();
    }
}


void Taif::performSearch(bool forward, bool next) {
    TEditor* editor = currentEditor();
    QString text = searchBar->getText();
    if (!editor or text.isEmpty()) return;

    QTextDocument::FindFlags flags{};
    if (searchBar->isCaseSensitive()) flags |= QTextDocument::FindCaseSensitively;
    if (!forward) flags |= QTextDocument::FindBackward;

    if (!next) editor->moveCursor(forward ? QTextCursor::Start : QTextCursor::End);

    bool found = editor->find(text, flags);
    if (!found and next) {
        editor->moveCursor(forward ? QTextCursor::Start : QTextCursor::End);
        found = editor->find(text, flags);
    }

    if (!found) QApplication::beep();
}

void Taif::findText() { performSearch(true, false); }
void Taif::findNextText() { performSearch(true, true); }
void Taif::findPrevText() { performSearch(false, true); }


void Taif::toggleConsole()
{
    bool isVisible = !consoleTabWidget->isVisible();
    consoleTabWidget->setVisible(isVisible);

    if (isVisible) {
        int consoleHeight = 250;
        int searchBarHeight = searchBar->isVisible() ? searchBar->height() : 0;

        int editorHeight = editorSplitter->height() - consoleHeight - searchBarHeight;
        editorSplitter->setSizes({editorHeight, 45, consoleHeight});

        if (QWidget* w = consoleTabWidget->currentWidget()) w->setFocus();
    } else {
        if (TEditor* editor = currentEditor()) editor->setFocus();
    }
}

/* ----------------------------------- File Menu Button ----------------------------------- */

int Taif::needSave() {
    if (TEditor* editor = currentEditor()) {
        if (editor->document()->isModified()) {
            QMessageBox msgBox;
            msgBox.setWindowTitle("طيف");
            msgBox.setText("تم التعديل على الملف.\n"    \
                           "هل تريد حفظ التغييرات؟");
            QPushButton *saveButton = msgBox.addButton("حفظ", QMessageBox::AcceptRole);
            QPushButton *discardButton = msgBox.addButton("تجاهل", QMessageBox::DestructiveRole);
            QPushButton *cancelButton = msgBox.addButton("إلغاء", QMessageBox::RejectRole);
            msgBox.setDefaultButton(cancelButton);

            QFont msgFont = this->font();
            msgFont.setPointSize(10);
            saveButton->setFont(msgFont);
            discardButton->setFont(msgFont);
            cancelButton->setFont(msgFont);

            msgBox.exec();

            QAbstractButton *clickedButton = msgBox.clickedButton();
            if (clickedButton == saveButton) {
                return 1;
            } else if (clickedButton == discardButton) {
                return 2;
            } else if (clickedButton == cancelButton) {
                return 0;
            }
        }
    }

    return 2;
}

void Taif::newFile() {

    TEditor* editor = currentEditor();
    if (editor) {
        int isNeedSave = needSave();
        if (!isNeedSave) return;
        if (isNeedSave == 1) this->saveFile();
    }

    TEditor *newEditor = new TEditor(setting, this);
    tabWidget->addTab(newEditor, "غير معنون");
    tabWidget->setCurrentWidget(newEditor);

    connect(newEditor, &TEditor::openRequest, this, [this](QString filePath){this->openFile(filePath);});
    connect(newEditor->document(), &QTextDocument::modificationChanged, this, &Taif::onModificationChanged);
    updateWindowTitle();
}

void Taif::openFile(QString filePath) {
    if (TEditor* current = currentEditor()) {
        int isNeedSave = needSave();
        if (!isNeedSave) return;
        if (isNeedSave == 1) this->saveFile();
    }

    if (filePath.isEmpty()) {
        filePath = QFileDialog::getOpenFileName(this, "فتح ملف", "", "ملف ألف (*.alif *.aliflib);;All Files (*)");
    }

    if (!filePath.isEmpty()) {
        for (int i = 0; i < tabWidget->count(); ++i) {
            TEditor* editor = qobject_cast<TEditor*>(tabWidget->widget(i));
            if (editor && editor->property("filePath").toString() == filePath) {
                tabWidget->setCurrentIndex(i);
                return;
            }
        }
        // -----------------------------------------

        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString content = in.readAll();
            file.close();

            TEditor *newEditor = new TEditor(setting, this);
            connect(newEditor->document(), &QTextDocument::modificationChanged, this, &Taif::onModificationChanged);
            newEditor->setPlainText(content);
            newEditor->setDocumentFilePath(filePath);


            QString backupPath = filePath + ".~";
            if (QFile::exists(backupPath)) {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::warning(this, "استعادة ملف",
                                             "يبدو أن البرنامج أُغلق بشكل غير متوقع.\n"
                                             "يوجد نسخة محفوظة تلقائيًا أحدث من الملف الأصلي.\n\n"
                                             "هل تريد استعادتها؟",
                                             QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    QFile backup(backupPath);
                    if (backup.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        QTextStream in(&backup);
                        newEditor->setPlainText(in.readAll()); // استبدل النص بنسخة الطوارئ
                        newEditor->document()->setModified(true); // نعتبره معدلاً ليقوم المستخدم بحفظه
                        backup.close();
                    }
                } else {
                    // إذا رفض المستخدم، احذف النسخة الاحتياطية القديمة
                    QFile::remove(backupPath);
                }
            }

            connect(newEditor->document(), &QTextDocument::modificationChanged, this, &Taif::onModificationChanged);
            connect(newEditor, &TEditor::openRequest, this, [this](QString filePath){this->openFile(filePath);});

            QFileInfo fileInfo(filePath);
            tabWidget->addTab(newEditor, fileInfo.fileName());
            tabWidget->setCurrentWidget(newEditor);
            tabWidget->setTabToolTip(tabWidget->currentIndex(), filePath);
            updateWindowTitle();


            QSettings settings("Alif", "Taif");
            QStringList recentFiles = settings.value("RecentFiles").toStringList();
            recentFiles.removeAll(filePath);
            recentFiles.prepend(filePath);
            while (recentFiles.size() > 10) {
                recentFiles.removeLast();
            }
            settings.setValue("RecentFiles", recentFiles);
        } else {
            QMessageBox::warning(this, "خطأ", "لا يمكن فتح الملف");
        }
    }
}

void Taif::loadFolder(const QString &folderPath)
{

    if (!folderPath.isEmpty() && QDir(folderPath).exists()) {
        fileTreeView->setVisible(true);

        fileTreeView->setRootIndex(fileSystemModel->index(folderPath));
    } else {
        fileTreeView->setVisible(false);
    }
}

void Taif::handleOpenFolderMenu()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "اختر مجلد", QDir::homePath());
    if (folderPath.isEmpty()) return;

    QFileSystemModel *model = new QFileSystemModel(this);
    model->setFilter(QDir::NoDotAndDotDot | QDir::AllEntries);
    model->setRootPath(folderPath);
    loadFolder(folderPath);

}

void Taif::toggleSidebar()
{
    bool shouldBeVisible = !fileTreeView->isVisible();
    fileTreeView->setVisible(shouldBeVisible);
    toggleSidebarAction->setChecked(shouldBeVisible);

    if (shouldBeVisible && fileTreeView->rootIndex() == QModelIndex()) {
        QString homePath = QDir::homePath();
        fileTreeView->setRootIndex(fileSystemModel->index(homePath));
    }
}

void Taif::onFileTreeDoubleClicked(const QModelIndex &index)
{
    const QString filePath = fileSystemModel->filePath(index);
    if (!fileSystemModel->isDir(index)) {
        openFile(filePath);
    }
}

void Taif::saveFile() {
    TEditor *editor = currentEditor();
    if (!editor) return;

    QString filePath = editor->property("filePath").toString();
    // --------------------------------------------------------

    QString content = editor->toPlainText();

    if (filePath.isEmpty()) {
        saveFileAs();
        return;
    } else {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&file);
            out << content;
            file.close();
            editor->document()->setModified(false);

            int index = tabWidget->indexOf(editor);
            if (index != -1) {
                QFileInfo fileInfo(filePath);
                tabWidget->setTabText(index, fileInfo.fileName());
            }
            editor->removeBackupFile();
            updateWindowTitle();
            return ;
        } else {
            QMessageBox::warning(this, "خطأ", "لا يمكن حفظ الملف");
            return;
        }
    }
}

void Taif::saveFileAs() {
    TEditor *editor = currentEditor();
    if (!editor) return ;

    QString content = editor->toPlainText();
    QString currentPath = editor->property("filePath").toString();
    QString currentName = currentPath.isEmpty() ? "ملف جديد.alif" : QFileInfo(currentPath).fileName();
    QString fileName = QFileDialog::getSaveFileName(this, "حفظ الملف", currentName, "ملف ألف (*.alif);;مكتبة ألف(*.aliflib);;All Files (*)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&file);
            out << content;
            file.close();

            editor->setDocumentFilePath(fileName);
            // ---------------------------------------------------

            editor->document()->setModified(false);

            int index = tabWidget->indexOf(editor);
            if (index != -1) {
                QFileInfo fileInfo(fileName);
                tabWidget->setTabText(index, fileInfo.fileName());
            }

            updateWindowTitle();
            return ;
        } else {
            QMessageBox::warning(this, "خطأ", "لا يمكن حفظ الملف");
            return ;
        }
    }
    return ;
}

void Taif::openSettings() {
    if (setting and setting->isVisible()) return;

    connect(setting, &TSettings::fontSizeChanged, this, [this](int size){
        for (int i = 0; i < tabWidget->count(); ++i) {
            qobject_cast<TEditor*>(tabWidget->widget(i))->updateFontSize(size);
        }
    });
    connect(setting, &TSettings::fontTypeChanged, this, [this](QString font){
        for (int i = 0; i < tabWidget->count(); ++i) {
            qobject_cast<TEditor*>(tabWidget->widget(i))->updateFontType(font);
        }
    });
    connect(setting, &TSettings::highlighterThemeChanged, this, [this](int themeIdx){
        QVector<std::shared_ptr<SyntaxTheme>> availableThemes = setting->getAvailableThemes();
        std::shared_ptr<SyntaxTheme> theme = availableThemes.at(themeIdx);
        for (int i = 0; i < tabWidget->count(); ++i) {
            qobject_cast<TEditor*>(tabWidget->widget(i))->updateHighlighterTheme(theme);
        }
    });

    setting->show();
}


void Taif::exitApp() {
    int isNeedSave = needSave();
    if (!isNeedSave) {
        return;
    }
    else if (isNeedSave == 1) {
        this->saveFile();
        return;
    }

    WelcomeWindow *welcome = new WelcomeWindow();
    welcome->show();
    this->close();
}

void Taif::onCurrentTabChanged()
{
    updateWindowTitle();
    updateCursorPosition();

    TEditor* editor = currentEditor();
    if (editor) {
        connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &Taif::updateCursorPosition);
    }
}

void Taif::updateCursorPosition()
{
    TEditor* editor = currentEditor();
    if (editor) {
        const QTextCursor cursor = editor->textCursor();
        int line = cursor.blockNumber() + 1;
        int column = cursor.columnNumber() + 1;

        cursorPositionLabel->setText(QString("UTF-8    السطر: %1   العمود: %2 ").arg(line).arg(column));
    } else {
        cursorPositionLabel->setText("");
    }
}


/* ----------------------------------- Run Menu Button ----------------------------------- */

// void Taif::runAlif() {
//     QString program{};
//     QStringList args{};
//     QString command{};
//     TEditor *editor = currentEditor(); // ✅ احصل على المحرر النشط
//     QStringList arguments{editor->filePath};
//     QString workingDirectory = QCoreApplication::applicationDirPath();

//     if (editor->filePath.isEmpty() or (currentEditor() && currentEditor()->document()->isModified())) {
//         QMessageBox::warning(nullptr, "تنبيه", "قم بحفظ الملف لتشغيله");
//         return;
//     }

// #if defined(Q_OS_WINDOWS)
//     // Windows: Start cmd.exe with /K to keep the window open
//     program = "cmd.exe";
//     command = "alif\\alif.exe";
//     args << "/C" << "start" << program << "/K" << command << arguments;
// #elif defined(Q_OS_LINUX)
//     // Linux: Use x-terminal-emulator with -e to execute the command
//     program = "x-terminal-emulator";
//     command = "./alif/alif";
//     if (!arguments.isEmpty()) {
//         command += " " + arguments.join(" ");
//     }
//     command += "; exec bash";
//     args << "-e" << "bash" << "-c" << command;
// #elif defined(Q_OS_MACOS)
//     // macOS: Use AppleScript to run the command in Terminal.app
//     program = "osascript";
//     command = "./alif/alif";

//     // Escape each part for shell execution
//     QStringList allParts = QStringList() << command << arguments;
//     QStringList escapedShellParts;
//     for (const QString &part : allParts) {
//         QString escaped = part;
//         escaped.replace("'", "'\"'\"'"); // Escape single quotes for AppleScript
//         escapedShellParts << "'" + escaped + "'";
//     }
//     QString shellCommand = escapedShellParts.join(" ");

//     // Escape double quotes for AppleScript
//     QString escapedAppleScriptCommand = shellCommand.replace("\"", "\\\"");

//     // Construct AppleScript
//     QString script = QString(
//                          "tell application \"Terminal\"\n"
//                          "    activate\n"
//                          "    do script \"cd '%1' && %2\"\n"
//                          "end tell"
//                          ).arg(workingDirectory, escapedAppleScriptCommand);

//     args << "-e" << script;
// #endif

//     QProcess* process = new QProcess(this);
//     process->setWorkingDirectory(workingDirectory);

//     process->start(program, args);
// }

//----------------

void Taif::runAlif() {
    TEditor *editor = currentEditor();
    if (!editor) return;

    QString filePath = editor->property("filePath").toString();
    if (filePath.isEmpty() || editor->document()->isModified()) {
        QMessageBox::warning(this, "تنبيه", "يجب حفظ الملف قبل التشغيل.");
        saveFile();
        filePath = editor->property("filePath").toString();
        if (filePath.isEmpty() || editor->document()->isModified()) return;
    }

    TConsole *console = nullptr;
    for (int i = 0; i < consoleTabWidget->count(); i++) {
        auto *tab = consoleTabWidget->widget(i);
        if (tab->objectName() == "interactiveConsole")
            console = qobject_cast<TConsole*>(tab);
    }

    if (!console) {
        console = new TConsole(this);
        console->setObjectName("interactiveConsole");
        consoleTabWidget->addTab(console, "مخرجات ألف");
        console->setConsoleRTL();
    }

    consoleTabWidget->setCurrentWidget(console);

    if (!consoleTabWidget->isVisible() || consoleTabWidget->height() < 50) {
        int totalHeight = editorSplitter->height();
        int consoleHeight = 250;

        int searchBarHeight = 0;
        if (searchBar && searchBar->isVisible()) {
            searchBarHeight = searchBar->height();
        }
        int editorHeight = totalHeight - consoleHeight - searchBarHeight;

        editorSplitter->setSizes({editorHeight, 45, consoleHeight});
    }

    consoleTabWidget->setVisible(true);

    QString program;
    QString appDir = QCoreApplication::applicationDirPath();

#if defined(Q_OS_WIN)
    QString localAlif = QDir(appDir).filePath("alif/alif.exe");
    qDebug() <<  " -------------------------------------------------------------------------------------------- "  << localAlif <<  " -------------------------------------------------------------------------------------------- ";

    if (QFile::exists(localAlif)) {
        program = localAlif;
    } else {
        program = "alif/alif.exe";
    }
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    program = QDir(appDir).filePath("alif/alif");
#endif

    if (!QFile::exists(program)) {
        console->clear();
        console->appendPlainTextThreadSafe("❌ خطأ: لم يتم العثور على مترجم ألف!");
        console->appendPlainTextThreadSafe("المسار المتوقع: " + program);

#if defined(Q_OS_LINUX)
        console->appendPlainTextThreadSafe("تأكد من أن ملف 'alif' موجود ولديه صلاحية التنفيذ (chmod +x).");
#endif
        return;
    }

    QStringList args = { filePath };
    QString workingDir = QFileInfo(filePath).absolutePath();

    if (worker) {
        worker->disconnect();
        worker->deleteLater();
        worker = nullptr;
    }

    console->clear();

    // Fetch and print the version (alif -ن)
    QProcess versionCheck{};
    versionCheck.setWorkingDirectory(QFileInfo(program).absolutePath());
    versionCheck.start(program, {"-ن"});

    // We wait briefly (max 2s) for the version check as it's a fast command
    if (versionCheck.waitForFinished(2000)) {
        QString versionOutput = versionCheck.readAll();
        if (!versionOutput.isEmpty()) {
            console->appendPlainTextThreadSafe(versionOutput);
        }
    }

    console->appendPlainTextThreadSafe("🚀 بدء تشغيل ملف ألف: " + QFileInfo(filePath).fileName() + "\n\n");

    worker = new ProcessWorker(program, args, workingDir);
    QThread *thread = new QThread();

    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &ProcessWorker::start);
    connect(worker, &ProcessWorker::outputReady,
            console, &TConsole::appendPlainTextThreadSafe);
    connect(worker, &ProcessWorker::errorReady,
            console, &TConsole::appendPlainTextThreadSafe);

    connect(worker, &ProcessWorker::finished, this, [=](int code){
        console->appendPlainTextThreadSafe(
            "\n──────────────────────────────\n✅ انتهى التنفيذ (رمز الخروج = "
            + QString::number(code) + ")\n"
            );
        thread->quit();
    });

    connect(worker, &QObject::destroyed, this, [this]() {
        worker = nullptr;
    });
    // Clean up thread and worker memory when finished
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(worker, &QObject::destroyed, thread, &QThread::quit); // Quit thread when worker is gone
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    connect(console, &TConsole::commandEntered,
            worker, &ProcessWorker::sendInput);

    thread->start();
}

//----------------

TEditor* Taif::currentEditor() {
    return qobject_cast<TEditor*>(tabWidget->currentWidget());
}

void Taif::closeTab(int index)
{

    if (tabWidget->count() <= 1) {
        return;
    }

    QWidget *tab = tabWidget->widget(index);

    if (!tab) return;

    TEditor* editor = qobject_cast<TEditor*>(tabWidget->widget(index));
    if (!editor) return;

    if (editor && editor->document()->isModified()) {
        int saveResult = needSave();

        if (!saveResult) {
            return;
        }
        else if (saveResult == 1) {
            this->saveFile();
            return;
        }

    }
    tabWidget->removeTab(index);

}

/* ----------------------------------- Help Menu Button ----------------------------------- */

void Taif::aboutTaif() {
    QMessageBox messageDialog{};
    messageDialog.setWindowTitle("عن محرر طيف");
    messageDialog.setText(R"(
        محرر طيف (نـ3) 1445-1446

        © الحقوق محفوظة لصالح
        برمجيات ألف - عبدالرحمن ومحمد الخطيب - سوريا

        محرر نصي خاص بلغة ألف نـ5
        يعمل على جميع المنصات "ويندوز - لينكس - ماك"
        ـــــــــــــــــــــــــــــــــــــــــــــــــــــ
        المحرر لا يزال تحت التطوير وقد يحتوي بعض الاخطاء
        نرجو تبليغ مجتمع ألف في حال وجود أي خطأ
        https://t.me/aliflang
        ـــــــــــــــــــــــــــــــــــــــــــــــــــــ
        فريق التطوير لا يمتلك أي ضمانات وغير مسؤول
        عن أي خطأ او خلل قد يحدث بسبب المحرر.

        المحرر يخضع لرخصة برمجيات ألف
        يجب قراءة الرخصة جيداً قبل البدأ بإستخدام المحرر
                                    )"
                          );
    messageDialog.setStyleSheet("background: #1e293b; color: #f1f5f9; font-family: 'Tajawal', 'Noto Kufi Arabic';");

    messageDialog.exec();
}

void Taif::checkForUpdates() {
    #if defined(Q_OS_WIN)
    QString path = QCoreApplication::applicationDirPath() + "/MaintenanceTool.exe";
    #elif defined(Q_OS_LINUX)
    QString path = QCoreApplication::applicationDirPath() + "/maintenancetool";
    #elif defined(Q_OS_MACOS)
    QString path = QCoreApplication::applicationDirPath() + "/../../../maintenancetool.app/contents/MacOS/maintenancetool";
    qDebug() << path;
    #endif
    // Use --updater to skip the "Add/Remove" page and go straight to updates
    QProcess::startDetached(path, {"--su"});
}

/* ----------------------------------- Other Functions ----------------------------------- */

void Taif::updateWindowTitle() {
    TEditor* editor = currentEditor();
    QString title{};

    if (!editor) {
        title = "طيف";
    } else {
        QString filePath = editor->property("filePath").toString();
        // --------------------------------------------------------

        if (filePath.isEmpty()) {
            title = "غير معنون";
        } else {
            title = QFileInfo(filePath).fileName();
        }
        if (editor->document()->isModified()) {
            title += "[*]";
        }
        title += " - طيف";
    }
    setWindowTitle(title);
    setWindowModified(editor && editor->document()->isModified()); // تحديث علامة التعديل للنافذة
}

void Taif::onModificationChanged(bool modified) {
    updateWindowTitle(); // استدعِ الدالة لتحديث علامة [*]
    // قد تحتاج أيضًا لتحديث اسم التبويب نفسه لإضافة [*]
    TEditor* editor = currentEditor(); // الحصول على المحرر المرتبط بالإشارة
    if (editor) {
        int index = tabWidget->indexOf(editor);
        if (index != -1) {
            QString currentText = tabWidget->tabText(index);
            if (modified && !currentText.endsWith("[*]")) {
                tabWidget->setTabText(index, currentText + "[*]");
            } else if (!modified && currentText.endsWith("[*]")) {
                tabWidget->setTabText(index, currentText.left(currentText.length() - 3));
            }
        }
    }
}

