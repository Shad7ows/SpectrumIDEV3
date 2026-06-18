#pragma once

#include <QTimer>
#include <QScrollBar>
#include <QPlainTextEdit>
#include <QCompleter>
#include <QLabel>
#include <memory>

#include "TSettings.h"
#include "TSyntaxHighlighter.h"
#include "AutoComplete.h"
#include "AutoCompleteUI.h"


class LineNumberArea;
class TMinimap;

class TEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    TEditor(TSettings* setting = nullptr, QWidget* parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth() const;
    QString filePath{};

    QString getCurrentLineIndentation(const QTextCursor &cursor) const;
    void curserIndentation();

    void setCompleter(QCompleter *completer);

    void startAutoSave();
    void stopAutoSave();
    void removeBackupFile();

public slots:
    void UpdateTabStopDistance(QFont);
    void updateFontSize(int);
    void updateFontType(QString font);
    void toggleComment();
    void duplicateLine();
    void moveLineUp();
    void moveLineDown();
    void performAutoSave();
    void updateHighlighterTheme(std::shared_ptr<SyntaxTheme>);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void showEvent(QShowEvent* event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    void leaveEvent(QEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

private:
    TSyntaxHighlighter* highlighter{};
    QLabel* hoverLabel{};
    QTimer hoverTimer{};
    int hoverLine = -1;
    int hoverCol = -1;
    QString hoverWord;

    void showHoverAt(int line, int col);
    void hideHover();

    LineNumberArea* lineNumberArea{};
    TMinimap* minimap{};

    struct FoldRegion {
        int startBlockNumber;
        int endBlockNumber;
        bool folded = false;
    };
    QVector<FoldRegion> foldRegions;

    void updateFoldRegions();
    void toggleFold(int blockNum);


    QTimer *autoSaveTimer;

    friend class LineNumberArea;
    friend class TMinimap;

    QCompleter* c{};
    CompletionModel *model{};
    std::vector<std::unique_ptr<ICompletionStrategy>> strategies{};
    QStringList snippetTargets{};
    QTextCursor textUnderCursor() const;
    void performCompletion();
    bool processSnippetNavigation();
    void setupAutoComplete();
    void insertWord(const QString& completion, QTextCursor& tc);
    void insertBuiltinFunction(const QString& functionName, QTextCursor& tc);
    void insertSnippet(const QString& snippet, QTextCursor& tc);
    // Bracket auto-completion methods
    bool handleAutoPairing(QKeyEvent* e);
    bool handleBracketCompletion(QChar openingBracket, QChar closingBracket);
    bool handleQuoteCompletion(QChar quoteChar);
    bool handleBracketSkip(QChar typedChar);

private slots:
    void updateMinimapPosition();
    void updateLineNumberAreaWidth();
    void highlightCurrentLine();
    inline void updateLineNumberArea(const QRect &rect, int dy);
    void insertCompletion(const QString &completion, CompletionType type);
signals:
    void openRequest(QString filePath);
};


class LineNumberArea : public QWidget {
public:
    LineNumberArea(TEditor* editor) : QWidget(editor), tEditor(editor) {
        this->setStyleSheet(
            "QWidget {"
            "   border-left: 1px solid #10a8f4;"
            "   border-top-left-radius: 9px;"
            "   border-bottom-left-radius: 9px;"
            "}"
        );
    }

    QSize sizeHint() const override {
        return QSize(tEditor->lineNumberAreaWidth(), 0);
    }

    void mousePressEvent(QMouseEvent* event) override {
        int y = event->position().y();
        QTextBlock block = tEditor->firstVisibleBlock();
        int top = qRound(tEditor->blockBoundingGeometry(block).translated(tEditor->contentOffset()).top());
        int height = qRound(tEditor->blockBoundingRect(block).height());

        while (block.isValid() && top <= y) {
            if (y >= top && y < top + height) {
                int blockNum = block.blockNumber();
                tEditor->toggleFold(blockNum);
                return;
            }
            block = block.next();
            top += height;
            height = qRound(tEditor->blockBoundingRect(block).height());
        }
    }


protected:
    void paintEvent(QPaintEvent* event) override {
        tEditor->lineNumberAreaPaintEvent(event);
    }


private:
    TEditor* tEditor{};
};
