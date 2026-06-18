#include "TEditor.h"
#include "TMinimap.h"
#include "TSyntaxDefinition.h"

#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QMimeData>
#include <QSettings>
#include <QPainterPath>
#include <QStack>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QRegularExpression>
#include <QTextLayout>


TEditor::TEditor(TSettings* setting, QWidget* parent) {
    setAcceptDrops(true);
    this->setStyleSheet(R"(
    QPlainTextEdit {
        background-color: #091021;
        color: #f1f5f9;
    }
)");

    // set tab distance
    UpdateTabStopDistance(font());

    this->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    this->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QTextDocument* editorDocument = this->document();
    QTextOption option = editorDocument->defaultTextOption();
    option.setTextDirection(Qt::RightToLeft);
    option.setAlignment(Qt::AlignRight);
    editorDocument->setDefaultTextOption(option);


    highlighter = new TSyntaxHighlighter(editorDocument);
    lineNumberArea = new LineNumberArea(this);
    minimap = new TMinimap(this, this);

    // تحديث الخريطة عند التمرير أو تعديل النص
    connect(this->verticalScrollBar(), &QScrollBar::valueChanged, minimap, &TMinimap::updateMinimap);
    connect(this->document(), &QTextDocument::contentsChanged, minimap, &TMinimap::updateMinimap);

    // ضبط الإكمال التلقائي
    setupAutoComplete();

    connect(this, &TEditor::blockCountChanged, this, &TEditor::updateLineNumberAreaWidth);
    connect(this, &TEditor::updateRequest, this, &TEditor::updateLineNumberArea);
    connect(this, &TEditor::cursorPositionChanged, this, &TEditor::highlightCurrentLine);
    connect(this->document(), &QTextDocument::contentsChanged, this, &TEditor::updateFoldRegions);

    updateLineNumberAreaWidth();
    highlightCurrentLine();

    // set saved setting font size to the editor
    QSettings settingsVal("Alif", "Taif");
    int savedSize = settingsVal.value("editorFontSize").toInt();
    if (savedSize > 10) {
        updateFontSize(savedSize);
    } else {
        updateFontSize(18);
    }
    // set saved setting font type to the editor
    QString savedFont = settingsVal.value("editorFontType").toString();
    if (savedFont.isEmpty()) {
        updateFontType("Noto Kufi Arabic");
    } else {
        updateFontType(savedFont);
    }
    // set saved setting theme to the editor
    int savedTheme = settingsVal.value("editorCodeTheme").toInt();
    savedTheme >= 0 ? savedTheme : savedTheme = 0;
    std::shared_ptr<SyntaxTheme> theme = setting->getAvailableThemes().at(savedTheme);
    updateHighlighterTheme(theme);

    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setInterval(60000);
    connect(autoSaveTimer, &QTimer::timeout, this, &TEditor::performAutoSave);

    connect(this->document(), &QTextDocument::contentsChanged, this, &TEditor::startAutoSave);

    installEventFilter(this);
    // هذه لاظهار لقائمة الشرح
    viewport()->setMouseTracking(true);

    hoverLabel = new QLabel(viewport());
    hoverLabel->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet(
        "QLabel { background-color: #0f172a; color: #e2e8f0; "
        "border: 1px solid #334155; border-radius: 6px; "
        "padding: 8px 12px; font-size: 13px; font-family: 'Noto Kufi Arabic'; }"
    );
    hoverLabel->setWordWrap(true);
    hoverLabel->setMaximumWidth(350);
    hoverLabel->hide();

    hoverTimer.setSingleShot(true);
    hoverTimer.setInterval(1000);
    connect(&hoverTimer, &QTimer::timeout, this, [this]() {
        showHoverAt(hoverLine, hoverCol);
    });
}

void TEditor::UpdateTabStopDistance(QFont font) {
    QFontMetricsF metrics(font);
    qreal spaceWidth = metrics.horizontalAdvance(' ');
    setTabStopDistance(8 * spaceWidth);
}

void TEditor::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        const int delta = event->angleDelta().y();
        if (delta == 0) return;

        QFont font = this->font();
        int currentSize = font.pixelSize();

        int step = 1;

        if (delta > 0) {
            currentSize += step;
        } else {
            currentSize -= step;
        }

        if (currentSize < 12) currentSize = 12;
        if (currentSize > 36) currentSize = 36;

        font.setPixelSize(currentSize);
        UpdateTabStopDistance(font);
        this->setFont(font);

        if (lineNumberArea) {
            QFont lineFont = lineNumberArea->font();
            lineFont.setPixelSize(currentSize);
            lineNumberArea->setFont(lineFont);
        }
        updateLineNumberAreaWidth();

        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void TEditor::updateFontSize(int size) {
    if (size < 10) {
        size = 18;
    }

    QFont font = this->font();
    font.setPixelSize(size);

    UpdateTabStopDistance(font);

    this->setFont(font);

    QFont fontNums = lineNumberArea->font();
    fontNums.setPixelSize(size);
    lineNumberArea->setFont(fontNums);
}

void TEditor::updateFontType(QString font) {
    QFont currentFont = this->font();
    currentFont.setFamily(font);

    UpdateTabStopDistance(currentFont);

    this->setFont(currentFont);
}


// 1. دالة تعليق/إلغاء تعليق الأكواد
void TEditor::toggleComment()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock(); // لبدء عملية تراجع (Undo) واحدة

    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();

    // تحديد بداية ونهاية الأسطر المحددة
    cursor.setPosition(startPos);
    int startBlock = cursor.blockNumber();
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
    int endBlock = cursor.blockNumber();

    if (cursor.atBlockStart() && endBlock > startBlock) {
        endBlock--;
    }

    bool shouldComment = false;

    QTextBlock block = document()->findBlockByNumber(startBlock);
    if (!block.text().trimmed().startsWith("#")) {
        shouldComment = true;
    }

    for (int i = startBlock; i <= endBlock; ++i) {
        block = document()->findBlockByNumber(i);
        QTextCursor lineCursor(block);

        if (shouldComment) {
            lineCursor.movePosition(QTextCursor::StartOfBlock);
            lineCursor.insertText("#");
        } else {
            QString text = block.text();
            int idx = text.indexOf("#");
            lineCursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, idx);
            lineCursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
            lineCursor.removeSelectedText();
        }
    }

    cursor.endEditBlock();
}

void TEditor::duplicateLine()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    QString lineText = cursor.block().text();

    cursor.movePosition(QTextCursor::EndOfBlock);

    cursor.insertText("\n" + lineText);

    cursor.endEditBlock();
}

void TEditor::moveLineUp()
{
    QTextCursor cursor = textCursor();
    QTextBlock currentBlock = cursor.block();
    QTextBlock prevBlock = currentBlock.previous();

    if (!prevBlock.isValid()) return;

    cursor.beginEditBlock();

    QString currentText = currentBlock.text();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.deletePreviousChar();

    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.insertText(currentText + "\n");

    cursor.movePosition(QTextCursor::Up);
    setTextCursor(cursor);

    cursor.endEditBlock();
}

void TEditor::moveLineDown()
{
    QTextCursor cursor = textCursor();
    QTextBlock currentBlock = cursor.block();
    QTextBlock nextBlock = currentBlock.next();

    if (!nextBlock.isValid()) return;

    cursor.beginEditBlock();

    QString currentText = currentBlock.text();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    if (cursor.atBlockStart()) cursor.deleteChar();

    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText("\n" + currentText);

    setTextCursor(cursor);
    cursor.endEditBlock();
}

bool TEditor::eventFilter(QObject* obj, QEvent* event) {
    if (obj == this and event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Return
             or keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return true;
            }
            curserIndentation();
            return true;
        }
    }
    return QPlainTextEdit::eventFilter(obj, event);
}

void TEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();

    menu->addSeparator();

    QAction *commentAction = new QAction("تعليق/إلغاء تعليق", this);
    commentAction->setShortcut(QKeySequence("Ctrl+/"));
    connect(commentAction, &QAction::triggered, this, &TEditor::toggleComment);
    menu->addAction(commentAction);

    QAction *duplicateAction = new QAction("تكرار السطر", this);
    duplicateAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(duplicateAction, &QAction::triggered, this, &TEditor::duplicateLine);
    menu->addAction(duplicateAction);

    menu->exec(event->globalPos());

    delete menu;
}

int TEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 36 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return space;
}

void TEditor::updateMinimapPosition() {
    int mapX = 0;
    // بسبب الاتجاه من اليمين لليسار قد يكون شريط التمرير على يمين الخريطة المصغرة
    if (verticalScrollBar()->isVisible() && verticalScrollBar()->x() < width() / 2) {
        mapX = verticalScrollBar()->width();
    }
    // تم تنقيص 3 من الاعلى لكي لا يقوم بالتغطية على حواف المحرر
    minimap->move(mapX, 3);
}

void TEditor::updateLineNumberAreaWidth() {
    int numsWidth = lineNumberAreaWidth();
    int mapWidth = 100;

    setViewportMargins(mapWidth, 0, numsWidth, 0);
}

inline void TEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void TEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    int numsWidth = lineNumberAreaWidth();

    lineNumberArea->setGeometry(this->width() - numsWidth, cr.top(), numsWidth, cr.height());

    if (minimap) {
        // تم تنقيص 3 من الاسفل لكي لا يقوم بالتغطية على حواف المحرر
        minimap->setGeometry(cr.left(), cr.top(), 100, cr.height() - 3);
        updateMinimapPosition();
    }
}

void TEditor::showEvent(QShowEvent* event) {
    QPlainTextEdit::showEvent(event);

    if (minimap) {
        QRect cr = contentsRect();
        // تم تنقيص 3 من الاسفل لكي لا يقوم بالتغطية على حواف المحرر
        minimap->setGeometry(cr.left(), cr.top(), 100, cr.height() - 3);
        updateMinimapPosition();
    }
}

// كشف الكلمة تحت المؤشر وعرض معلومات التمرير (hover) بعد تأخير

void TEditor::mouseMoveEvent(QMouseEvent *event) {
    QPoint pos = event->pos();

    QString foundWord;
    int foundLine = -1;
    int foundStart = -1;

    QTextBlock block = firstVisibleBlock();
    while (block.isValid()) {
        QRectF blockRect = blockBoundingGeometry(block).translated(contentOffset());
        if (pos.y() >= blockRect.top() && pos.y() <= blockRect.bottom()) {
            foundLine = block.blockNumber();
            QString lineText = block.text();
            QTextLayout *layout = block.layout();
            if (!layout) break;

            // Find which visual line the mouse is on
            QTextLine textLine;
            for (int i = 0; i < layout->lineCount(); ++i) {
                QTextLine l = layout->lineAt(i);
                qreal ly = blockRect.top() + l.y();
                if (pos.y() >= ly && pos.y() <= ly + l.height()) {
                    textLine = l;
                    break;
                }
            }
            if (!textLine.isValid()) break;

            // Find character at mouse x position
            int charIdx = textLine.xToCursor(pos.x() - blockRect.left());
            if (charIdx < 0 || charIdx >= lineText.size()) break;

            // Expand to full word
            if (lineText[charIdx].isLetterOrNumber() || lineText[charIdx] == '_') {
                int s = charIdx;
                while (s > 0 && (lineText[s - 1].isLetterOrNumber() || lineText[s - 1] == '_'))
                    s--;
                int e = charIdx;
                while (e < lineText.size() && (lineText[e].isLetterOrNumber() || lineText[e] == '_'))
                    e++;
                foundWord = lineText.mid(s, e - s);
                foundStart = s;
            }
            break;
        }
        block = block.next();
    }

    if (foundWord == hoverWord && foundLine == hoverLine) {
        QPlainTextEdit::mouseMoveEvent(event);
        return;
    }

    hoverWord = foundWord;
    hoverLine = foundLine;
    hoverCol = foundStart;

    if (hoverWord.isEmpty()) {
        hoverLabel->hide();
    } else {
        hoverTimer.start();
    }

    QPlainTextEdit::mouseMoveEvent(event);
}

void TEditor::hideHover() {
    hoverTimer.stop();
    hoverLabel->hide();
    hoverWord.clear();
}

void TEditor::leaveEvent(QEvent *event) {
    hideHover();
    QPlainTextEdit::leaveEvent(event);
}

void TEditor::showHoverAt(int line, int col) {
    QTextBlock block = document()->findBlockByNumber(line);
    if (!block.isValid()) { hideHover(); return; }

    QString text = block.text();
    if (col < 0 || col >= text.size()) { hideHover(); return; }

    int start = col;
    while (start > 0 && (text[start - 1].isLetterOrNumber() || text[start - 1] == '_'))
        start--;
    int end = col;
    while (end < text.size() && (text[end].isLetterOrNumber() || text[end] == '_'))
        end++;

    QString word = text.mid(start, end - start);
    if (word.isEmpty()) { hideHover(); return; }

    LanguageDefinition langDef;
    QString tooltip;

    // Compound: "والا اذا"
    if (word == "وإلا" || word == "والا") {
        int peek = end;
        while (peek < text.size() && text[peek] == ' ') peek++;
        if (text.mid(peek, 2) == "اذا" || text.mid(peek, 2) == "إذا") {
            tooltip = QString("<div style='font-size:15px;font-weight:bold;color:#60a5fa'>وإلا اذا</div>"
                              "<div style='color:#94a3b8;margin-top:4px'>جملة شرطية بديلة — تُنفذ إذا لم يتحقق الشرط السابق</div>");
            hoverLabel->setText(tooltip);
            hoverLabel->adjustSize();
            QTextBlock b = document()->findBlockByNumber(hoverLine);
            QTextCursor tc(b);
            tc.setPosition(b.position() + end + 4);
            QRect cr = QPlainTextEdit::cursorRect(tc);
            QPoint globalPos = viewport()->mapToGlobal(cr.bottomLeft());
            hoverLabel->move(globalPos.x(), globalPos.y() + 4);
            hoverLabel->show();
            return;
        }
    }
    if (word == "أوإذا" || word == "اواذا") {
        tooltip = QString("<div style='font-size:15px;font-weight:bold;color:#60a5fa'>أوإذا</div>"
                          "<div style='color:#94a3b8;margin-top:4px'>جملة شرطية بديلة — تُنفذ إذا لم يتحقق الشرط السابق</div>");
        hoverLabel->setText(tooltip);
        hoverLabel->adjustSize();
        QTextBlock b = document()->findBlockByNumber(hoverLine);
        QTextCursor tc(b);
        tc.setPosition(b.position() + end);
        QRect cr = QPlainTextEdit::cursorRect(tc);
        QPoint globalPos = viewport()->mapToGlobal(cr.bottomLeft());
        hoverLabel->move(globalPos.x(), globalPos.y() + 4);
        hoverLabel->show();
        return;
    }

    // "دالة" or "صنف" keyword — show keyword info + the name after it
    if (word == "دالة" || word == "صنف") {
        bool isFunc = (word == "دالة");
        QString kindColor = isFunc ? "#34d399" : "#fbbf24";
        QString kindName = isFunc ? "دالة" : "صنف";
        QString keywordDesc = isFunc ? "تعريف دالة جديدة" : "تعريف صنف جديد";

        int peek = end;
        while (peek < text.size() && text[peek] == ' ') peek++;
        int nameStart = peek;
        while (peek < text.size() && (text[peek].isLetterOrNumber() || text[peek] == '_'))
            peek++;
        QString defName = (peek > nameStart) ? text.mid(nameStart, peek - nameStart) : QString();

        QString doc;
        QTextBlock prevBlock = document()->findBlockByNumber(line - 1);
        while (prevBlock.isValid()) {
            QString prevText = prevBlock.text().trimmed();
            if (prevText.startsWith('#')) { doc = prevText.mid(1).trimmed(); break; }
            if (!prevText.isEmpty()) break;
            prevBlock = prevBlock.previous();
        }

        tooltip = QString("<div style='font-size:15px;font-weight:bold;color:%1'>%2</div>"
                          "<div style='color:#94a3b8;margin-top:4px'>%3</div>")
                      .arg(kindColor, kindName, keywordDesc);
        if (!defName.isEmpty()) {
            tooltip += QString("<div style='color:#e2e8f0;margin-top:6px;font-size:14px;'>%1 <span style='color:%2'>%3</span></div>")
                           .arg(kindName, kindColor, defName.toHtmlEscaped());
        }
        if (!doc.isEmpty()) {
            tooltip += QString("<div style='color:#94a3b8;margin-top:4px;border-top:1px solid #334155;padding-top:4px'>%1</div>")
                           .arg(doc.toHtmlEscaped());
        }

        hoverLabel->setText(tooltip);
        hoverLabel->adjustSize();
        QTextBlock b = document()->findBlockByNumber(hoverLine);
        QTextCursor tc(b);
        tc.setPosition(b.position() + end);
        QRect cr = QPlainTextEdit::cursorRect(tc);
        QPoint globalPos = viewport()->mapToGlobal(cr.bottomLeft());
        hoverLabel->move(globalPos.x(), globalPos.y() + 4);
        hoverLabel->show();
        return;
    }

    QRegularExpression reDef(
        QString("(?:^|\\s)(دالة|صنف)\\s+%1(?:\\s|$|[\\(\\)])").arg(QRegularExpression::escape(word)),
        QRegularExpression::MultilineOption
    );
    QTextCursor foundDef = document()->find(reDef);
    if (!foundDef.isNull()) {
        int defLine = foundDef.blockNumber() + 1;
        QString defText = foundDef.block().text().trimmed();
        bool isFunc = defText.startsWith("دالة");
        QString kind = isFunc ? "دالة" : "صنف";
        QString kindColor = isFunc ? "#34d399" : "#fbbf24";

        QString doc;
        QTextBlock prevBlock = document()->findBlockByNumber(foundDef.blockNumber() - 1);
        while (prevBlock.isValid()) {
            QString prevText = prevBlock.text().trimmed();
            if (prevText.startsWith('#')) { doc = prevText.mid(1).trimmed(); break; }
            if (!prevText.isEmpty()) break;
            prevBlock = prevBlock.previous();
        }

        tooltip = QString("<div style='font-size:15px;font-weight:bold;color:%1'>%2 <span style='color:#e2e8f0'>%3</span></div>"
                          "<div style='color:#94a3b8;margin-top:4px'>معرّفة في السطر %4</div>")
                      .arg(kindColor, kind, word.toHtmlEscaped()).arg(defLine);
        if (!doc.isEmpty()) {
            tooltip += QString("<div style='color:#94a3b8;margin-top:4px;border-top:1px solid #334155;padding-top:4px'>%1</div>")
                           .arg(doc.toHtmlEscaped());
        }

        hoverLabel->setText(tooltip);
        hoverLabel->adjustSize();
        QTextBlock b = document()->findBlockByNumber(hoverLine);
        QTextCursor tc(b);
        tc.setPosition(b.position() + end);
        QRect cr = QPlainTextEdit::cursorRect(tc);
        QPoint globalPos = viewport()->mapToGlobal(cr.bottomLeft());
        hoverLabel->move(globalPos.x(), globalPos.y() + 4);
        hoverLabel->show();
        return;
    }

    // "هذا"
    if (word == "هذا") {
        tooltip = QString("<div style='font-size:15px;font-weight:bold;color:#f472b6'>%1</div>"
                          "<div style='color:#94a3b8;margin-top:4px'>متغير خاص — يشير إلى الكائن الحالي</div>")
                     .arg(word.toHtmlEscaped());
    }

    else if (langDef.keywordSet.contains(word)) {
        QString extra;
        if (word == "صنف") extra = "تعريف صنف جديد";
        else if (word == "دالة") extra = "تعريف دالة جديدة";
        else if (word == "اذا" ) extra = "جملة شرطية — تنفيذ كتلة إذا تحقق الشرط";
        else if (word == "بينما") extra = "حلقة تكرار شرطية — تستمر ما دام الشرط صحيحاً";
        else if (word == "لكل") extra = "حلقة تكرار — تمر على كل عنصر في تسلسل";
        else if (word == "حاول") extra = "بدء كتلة معالجة أخطاء";
        else if (word == "خلل") extra = "معالجة خطأ معين — بند الاستثناء";
        else if (word == "نهاية") extra = "يُنفَّذ دائماً بعد المحاولة سواء نجح أو أخطأ";
        else if (word == "عند") extra = "لإدارة السياق";
        else if (word == "لاجل") extra = "حلقة في التعبيرات الضمنية";
        else if (word == "طابق") extra = "مطابقة هيكلية للأنماط";
        else if (word == "خطية") extra = "دالة مجهولة الاسم — تعبير مختصر";
        else if ( word == "والا") extra = "بند بديل في الجمل الشرطية";
        else if ( word == "اواذا") extra = "جملة شرطية بديلة";
        else if (word == "توقف") extra = "الخروج من الحلقة الحالية";
        else if (word == "استمر") extra = "انتظار التكرار التالي بدون تنفيذ ما بعده";
        else if (word == "ارجع") extra = "إرجاع قيمة من الدالة";
        else if (word == "مرر") extra = "جملة فارغة — لا تفعل شيئاً";
        else if (word == "ولد") extra = "إنتاج قيمة من مُولِّد";
        else if (word == "و") extra = "عملية منطقية — الجامعة";
        else if (word == "او") extra = "عملية منطقية — المانعة";
        else if (word == "ليس") extra = "نفي منطقي";
        else if (word == "في") extra = "اختبار العضوية — هل يوجد العنصر في المجموعة؟";
        else if (word == "هل") extra = "اختبار الهوية — هل الكائنان متساويان؟";
        else if (word == "عام") extra = "لتعريف متغير في النطاق العام";
        else if (word == "نطاق") extra = "للإشارة إلى متغير في النطاق الأعلى";
        else if (word == "احذف") extra = "حذف متغير أو عنصر";
        else if (word == "متوقع") extra = "جملة تأكيد — تتحقق من صحة شرط";
        else if (word == "استورد") extra = "استيراد وحدة أو مكتبة";
        else if (word == "مزامنة") extra = "لتحديد الدالة كغير متزامنة";
        else if (word == "انتظر") extra = "انتظار نتيجة غير متزامنة";
        else if (word == "عدم") extra = "قيمة الفراغ — عدم الوجود";
        else if (word == "صح") extra = "القيمة المنطقية — صحيح";
        else if (word == "خطا") extra = "القيمة المنطقية — خطأ";
        else if (word == "ك") extra = "إعطاء اسم بديل";
        else if (word == "من") extra = "لتحديد مصدر الاستيراد";
        else extra = "كلمة محجوزة في لغة ألف";

        tooltip = QString("<div style='font-size:15px;font-weight:bold;color:#60a5fa'>%1</div>"
                          "<div style='color:#94a3b8;margin-top:4px'>%2</div>")
                     .arg(word.toHtmlEscaped(), extra);
    }

    else if (langDef.builtinSet.contains(word)) {
        QString extra;
        if (word == "اطبع") extra = "طباعة الكائنات إلى المخرجات";
        else if (word == "ادخل") extra = "قراءة سطر من المدخلات القياسية";
        else if (word == "طول") extra = "إرجاع طول الكائن — عدد العناصر";
        else if (word == "اقصى") extra = "إرجاع أكبر عنصر في تسلسل";
        else if (word == "ادنى") extra = "إرجاع أصغر عنصر في تسلسل";
        else if (word == "اجمع") extra = "إرجاع مجموع عناصر التسلسل";
        else if (word == "تحقق_اي") extra = "يُرجع صح إذا كان أي عنصر صحيحاً";
        else if (word == "هل_نوع") extra = "التحقق مما إذا كان الكائن من نوع معين";
        else if (word == "صحيح") extra = "نوع العدد الصحيح";
        else if (word == "عشري") extra = "نوع العدد العشري";
        else if (word == "منطق") extra = "نوع المنطقي — صحيح أو خطأ";
        else if (word == "نص") extra = "نوع النص";
        else if (word == "مصفوفة") extra = "نوع المصفوفة الديناميكية";
        else if (word == "مترابطة") extra = "تسلسل ثابت لا يتغير";
        else if (word == "مميزة") extra = "مجموعة قابلة للتعديل";
        else if (word == "مدى") extra = "نطاق أرقام متسلسلة";
        else if (word == "نوع") extra = "نوع الكائن";
        else if (word == "اصل") extra = "للوصول لطرق الفئة الأصل";
        else if (word == "مقرون") extra = "دمج تسلسلات في مترابطات";
        else if (word == "فهرس") extra = "فهرس — رقمตำแหนון العنصر";
        else if (word == "افتح") extra = "فتح ملف للقراءة أو الكتابة";
        else extra = "دالة أو نوع مدمج في لغة ألف";

        tooltip = QString("<div style='font-size:15px;font-weight:bold;color:#c084fc'>%1</div>"
                          "<div style='color:#94a3b8;margin-top:4px'>%2</div>")
                     .arg(word.toHtmlEscaped(), extra);
    }
    else {
        hideHover();
        return;
    }

    hoverLabel->setText(tooltip);
    hoverLabel->adjustSize();

    QTextBlock b = document()->findBlockByNumber(hoverLine);
    QTextCursor tc(b);
    tc.setPosition(b.position() + end);
    QRect cr = QPlainTextEdit::cursorRect(tc);
    QPoint globalPos = viewport()->mapToGlobal(cr.bottomLeft());
    hoverLabel->move(globalPos.x(), globalPos.y() + 4);
    hoverLabel->show();
}

void TEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {

    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::transparent);
    painter.setRenderHint(QPainter::Antialiasing);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    QPen arrowPen(QColor(37, 70, 99));
    arrowPen.setWidth(3);
    arrowPen.setJoinStyle(Qt::RoundJoin);
    arrowPen.setCapStyle(Qt::RoundCap);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            painter.setPen(QColor(200, 200, 200));
            painter.drawText(12, top, lineNumberArea->width(), fontMetrics().height(),
                                     Qt::AlignRight | Qt::AlignVCenter, number);

            for (const auto& region : foldRegions) {
                if (region.startBlockNumber == blockNumber) {
                    painter.setPen(arrowPen);
                    painter.setBrush(QColor(37, 70, 99));

                    int midY = top + fontMetrics().height() / 2;
                    int rightEdge = lineNumberArea->width() - 6;
                    int leftEdge = rightEdge - 8;

                    QPolygonF arrow;
                    if (region.folded) {
                        // Left-pointing Triangle
                        arrow << QPoint(rightEdge, midY - 4)
                        << QPoint(leftEdge, midY)
                        << QPoint(rightEdge, midY + 4);
                    } else {
                        // Down-pointing Triangle
                        arrow << QPoint(leftEdge, midY - 3)
                        << QPoint(rightEdge, midY - 3)
                        << QPoint((leftEdge + rightEdge) / 2.0, midY + 4);
                    }

                    painter.drawPolygon(arrow);
                }
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void TEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(16, 23, 48, 225);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void TEditor::updateFoldRegions() {

    // we use static array for zero memory allocation per call
    static const QStringView foldTriggers[] = {
        u"صنف", u"دالة", u"اذا", u"والا",
        u"اواذا", u"بينما", u"لكل", u"حاول", u"خلل", u"نهاية"
    };

    // Preserve previous fold states
    QHash<int, bool> previousFoldStates;
    previousFoldStates.reserve(foldRegions.size());
    for (const FoldRegion& region : foldRegions) {
        previousFoldStates.insert(region.startBlockNumber, region.folded);
    }
    foldRegions.clear();

    struct ActiveFold {
        int startBlock;
        int indent;
    };
    QStack<ActiveFold> stack;
    int lastValidBlockNumber = -1;

    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        QString text = block.text();
        QStringView trimmed = QStringView(text).trimmed();

        if (trimmed.isEmpty()) {
            block = block.next();
            continue;
        }

        int indent = 0;
        for (QChar c : text) {
            if (c == u'\t') indent += 8;
            else if (c == u' ') indent += 1;
            else break;
        }

        while (!stack.isEmpty() && indent <= stack.top().indent) {
            ActiveFold af = stack.pop();
            if (lastValidBlockNumber > af.startBlock) {
                FoldRegion region{};
                region.startBlockNumber = af.startBlock;
                region.endBlockNumber = lastValidBlockNumber;
                region.folded = previousFoldStates.value(af.startBlock, false);
                foldRegions.append(region);
            }
        }

        bool isTrigger = false;
        for (const QStringView& trigger : foldTriggers) {
            if (trimmed.startsWith(trigger)) {
                isTrigger = true;
                break;
            }
        }

        if (isTrigger) {
            stack.push({block.blockNumber(), indent});
        }

        lastValidBlockNumber = block.blockNumber();
        block = block.next();
    }

    while (!stack.isEmpty()) {
        ActiveFold af = stack.pop();
        if (lastValidBlockNumber > af.startBlock) {
            FoldRegion region;
            region.startBlockNumber = af.startBlock;
            region.endBlockNumber = lastValidBlockNumber;
            region.folded = previousFoldStates.value(af.startBlock, false);
            foldRegions.append(region);
        }
    }

    if (lineNumberArea) {
        lineNumberArea->update();
    }

    std::vector<std::pair<int, int>> hiddenIntervals;
    hiddenIntervals.reserve(foldRegions.size());
    for (const FoldRegion& r : foldRegions) {
        if (r.folded) {
            hiddenIntervals.push_back({r.startBlockNumber + 1, r.endBlockNumber});
        }
    }

    std::sort(hiddenIntervals.begin(), hiddenIntervals.end());
    std::vector<std::pair<int, int>> mergedHidden;
    mergedHidden.reserve(hiddenIntervals.size());
    for (const auto& interval : hiddenIntervals) {
        if (mergedHidden.empty() || mergedHidden.back().second < interval.first) {
            mergedHidden.push_back(interval);
        } else {
            mergedHidden.back().second = std::max(mergedHidden.back().second, interval.second);
        }
    }

    block = document()->firstBlock();
    auto intervalIt = mergedHidden.begin();
    while (block.isValid()) {
        int bNum = block.blockNumber();

        while (intervalIt != mergedHidden.end() && intervalIt->second < bNum) {
            ++intervalIt;
        }

        bool shouldBeHidden = (intervalIt != mergedHidden.end() && bNum >= intervalIt->first && bNum <= intervalIt->second);

        if (block.isVisible() == shouldBeHidden) {
            block.setVisible(!shouldBeHidden);
        }

        block = block.next();
    }

    document()->markContentsDirty(0, document()->characterCount());
    viewport()->update();
}

void TEditor::toggleFold(int blockNumber) {
    for (FoldRegion &region : foldRegions) {
        if (region.startBlockNumber == blockNumber) {
            region.folded = !region.folded;

            QTextBlock block = document()->findBlockByNumber(region.startBlockNumber + 1);
            while (block.isValid() && block.blockNumber() <= region.endBlockNumber) {
                block.setVisible(!region.folded);
                block = block.next();
            }

            if (!region.folded) {
                for (FoldRegion &subRegion : foldRegions) {
                    if (subRegion.startBlockNumber > region.startBlockNumber &&
                        subRegion.endBlockNumber <= region.endBlockNumber) {
                        QTextBlock subBlock = document()->findBlockByNumber(subRegion.startBlockNumber + 1);
                        bool allVisible = true;
                        while (subBlock.isValid() && subBlock.blockNumber() <= subRegion.endBlockNumber) {
                            if (!subBlock.isVisible()) {
                                allVisible = false;
                                break;
                            }
                            subBlock = subBlock.next();
                        }
                        subRegion.folded = !allVisible;
                    }
                }
            }

            document()->markContentsDirty(0, document()->characterCount());
            viewport()->update();
            break;
        }
    }
}

void TEditor::paintEvent(QPaintEvent *event) {
    QPlainTextEdit::paintEvent(event);

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen linePen(QColor(79, 144, 170, 125));
    linePen.setWidth(1);
    linePen.setCapStyle(Qt::FlatCap);
    painter.setPen(linePen);

    qreal tabStopDistance = this->tabStopDistance();
    qreal viewWidth = viewport()->width();

    qreal rightOffset = document()->documentMargin() - contentOffset().x();

    QTextBlock block = firstVisibleBlock();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    // lambda func to calculate indent level (8 spaces = 1 tab)
    auto getIndentLevel = [](const QString& text) -> int {
        int indent = 0;
        int spaces = 0;
        for (QChar c : text) {
            if (c == '\t') {
                indent++;
                spaces = 0;
            } else if (c == ' ') {
                spaces++;
                if (spaces == 8) {
                    indent++;
                    spaces = 0;
                }
            } else {
                break;
            }
        }
        return indent;
    };

    // Iterate through all visible blocks
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible()) {
            QString text = block.text();
            int indentLevel = 0;

            // Handle empty lines (continue scope lines across them)
            if (text.trimmed().isEmpty()) {
                int prevIndent = 0;
                int nextIndent = 0;

                // Look back for the previous non-empty line
                QTextBlock prev = block.previous();
                while (prev.isValid() && prev.text().trimmed().isEmpty()) {
                    prev = prev.previous();
                }
                if (prev.isValid()) prevIndent = getIndentLevel(prev.text());

                // Look ahead for the next non-empty line
                QTextBlock next = block.next();
                while (next.isValid() && next.text().trimmed().isEmpty()) {
                    next = next.next();
                }
                if (next.isValid()) nextIndent = getIndentLevel(next.text());

                // Use the minimum of surrounding indents to safely connect/close scopes
                indentLevel = qMin(prevIndent, nextIndent);
            } else {
                indentLevel = getIndentLevel(text);
            }

            // Draw vertical lines from Right to Left
            // Starting from i = 0 to places the line Under the parent keyword
            for (int i = 0; i < indentLevel; ++i) {
                // Calculate X from the right edge, shifting left based on the scope depth
                qreal x = viewWidth - rightOffset - (i * tabStopDistance);

                // Draw the scope line for the current block height
                painter.drawLine(QLineF(x, top, x, bottom));
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
    }
}


/* ---------------------------------- Drag and Drop ---------------------------------- */

void TEditor::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.fileName().endsWith(".alif", Qt::CaseInsensitive) or
                url.fileName().endsWith(".aliflib", Qt::CaseInsensitive) or
                url.fileName().endsWith(".txt", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }

    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void TEditor::dragMoveEvent(QDragMoveEvent* event) {
    event->acceptProposedAction();
}

void TEditor::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.fileName().endsWith(".alif", Qt::CaseInsensitive) or
                url.fileName().endsWith(".aliflib", Qt::CaseInsensitive) or
                url.fileName().endsWith(".txt", Qt::CaseInsensitive)) {

                QString filePath = url.toLocalFile();
                emit openRequest(filePath);

                event->acceptProposedAction();
                return;
            }
        }
    }

    if (event->mimeData()->hasText()) {
        QTextCursor dropCursor = cursorForPosition(event->position().toPoint());
        int dropPosition = dropCursor.position();

        if (dropPosition >= textCursor().selectionStart()
            and dropPosition <= textCursor().selectionEnd()) {
            event->ignore();
            return;
        }

        QString droppedText = event->mimeData()->text();
        QTextCursor originalCursor = textCursor();

        originalCursor.removeSelectedText();

        if (originalCursor.position() < dropPosition) {
            dropPosition -= droppedText.length();
        }

        dropCursor.setPosition(dropPosition);
        dropCursor.insertText(droppedText);

        event->acceptProposedAction();
        return;
    }

    event->ignore();
}

void TEditor::dragLeaveEvent(QDragLeaveEvent* event) {
    event->accept();
}


/* ---------------------------------- Indentation ---------------------------------- */

void TEditor::curserIndentation() {
    QTextCursor cursor = textCursor();
    QString lineText = cursor.block().text();
    int cursorPosInLine = cursor.positionInBlock();
    QString currentIndentation = getCurrentLineIndentation(cursor);

    if (cursorPosInLine > 0) {
        int checkPos = cursorPosInLine - 1;
        while (checkPos >= 0 and lineText.at(checkPos).isSpace()) {
            checkPos--;
        }

        if (checkPos >= 0 and lineText.at(checkPos) == ':') {
            currentIndentation += "\t";
        }
    }

    cursor.beginEditBlock();
    cursor.insertText("\n" + currentIndentation);
    cursor.endEditBlock();
    setTextCursor(cursor);
}

QString TEditor::getCurrentLineIndentation(const QTextCursor &cursor) const {
    QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
    }

    QString lineText = block.text();
    QString indentation;
    for (const QChar &ch : lineText) {
        if (ch == ' ' or ch == '\t') {
            indentation += ch;
        } else {
            break;
        }
    }
    return indentation;
}




void TEditor::startAutoSave() {
    if (!autoSaveTimer->isActive()) {
        autoSaveTimer->start();
    }
}

void TEditor::stopAutoSave() {
    autoSaveTimer->stop();
}

void TEditor::performAutoSave() {
    QString filePath = this->property("filePath").toString();
    if (filePath.isEmpty() || !this->document()->isModified()) return;

    QString backupPath = filePath + ".~";

    QFile file(backupPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << this->toPlainText();
        file.close();
    }
}

void TEditor::removeBackupFile() {
    QString filePath = this->property("filePath").toString();
    if (filePath.isEmpty()) return;

    QString backupPath = filePath + ".~";
    if (QFile::exists(backupPath)) {
        QFile::remove(backupPath);
    }
    stopAutoSave();
}


void TEditor::updateHighlighterTheme(std::shared_ptr<SyntaxTheme> theme) {
    this->highlighter->setTheme(theme);
}



// --- autocomplete system ---

void TEditor::setupAutoComplete() {
    // set autocomplete system
    model = new CompletionModel(this);
    strategies.push_back(std::make_unique<SnippetStrategy>());
    strategies.push_back(std::make_unique<KeywordStrategy>());
    strategies.push_back(std::make_unique<BuiltinStrategy>());
    strategies.push_back(std::make_unique<DynamicWordStrategy>());

    QCompleter *completer = new QCompleter(this);
    setCompleter(completer);
}

void TEditor::setCompleter(QCompleter *completer) {
    if (c) disconnect(c, nullptr, this, nullptr);
    c = completer;
    if (!c) return;

    c->setWidget(this);
    c->setCompletionMode(QCompleter::PopupCompletion);
    c->setCaseSensitivity(Qt::CaseInsensitive);
    c->setModel(model);

    // Custom Rich Popup ---
    TCompletionPopup *popup = new TCompletionPopup;
    c->setPopup(popup); // QCompleter takes ownership

    popup->setItemDelegate(new TModernCompletionDelegate(popup));

    // set dimensions
    popup->setMinimumWidth(350);
    popup->setMinimumHeight(200); // Taller to fit list + footer

    // To this lambda that captures the type:
    connect(c, QOverload<const QString &>::of(&QCompleter::activated),
            this, [this](const QString &completion) {
                // Get the current index from the completer popup
                QModelIndex index = c->popup()->currentIndex();
                if (index.isValid()) {
                    // Get the type from the model
                    CompletionType type = static_cast<CompletionType>(
                        index.data(Qt::UserRole + 2).toInt());
                    // Get the full completion item
                    QString completionText = index.data(Qt::EditRole).toString();
                    insertCompletion(completionText, type);
                } else {
                    // Fallback to just the string without type
                    insertCompletion(completion, CompletionType::DynamicWord);
                }
            });
}

void TEditor::focusOutEvent(QFocusEvent *e) {
    if (c && c->popup()->isVisible()) {
        c->popup()->hide();
    }
    QPlainTextEdit::focusOutEvent(e);
}

void TEditor::keyPressEvent(QKeyEvent *e) {
    // handleing Brackets and Quotes
    if (handleAutoPairing(e)) {
        e->accept();
        return;
    }

    // Handle Navigation for Live Update (Arrow Keys) ---
    if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) {
        // Let the editor move the cursor first
        QPlainTextEdit::keyPressEvent(e);
        // Then immediately trigger completion to update the list based on the new cursor position
        performCompletion();
        return;
    }

    if (c && c->popup()->isVisible()) {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return;
        default: break;
        }
    }
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
        if (!snippetTargets.isEmpty()) {
            if (processSnippetNavigation()) {
                e->accept();
                return;
            }
        }
    }

    if (e->key() == Qt::Key_Tab && !snippetTargets.isEmpty()) {
        if (processSnippetNavigation()) {
            e->accept();
            return;
        }
    }

    bool isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);

    QPlainTextEdit::keyPressEvent(e);

    if (!isShortcut && e->text().isEmpty()) return;

    performCompletion();
}

void TEditor::performCompletion() {
    QString textUnder = textUnderCursor().selectedText();
    // Allow empty text for shortcut (Ctrl+Space) to show all
    if (textUnder.length() < 1) {
        // Optional: Trigger immediately on Ctrl+Space even if empty?
        // For now, keep logic to hide if empty, unless you want "all suggestion" behavior.
        c->popup()->hide();
        return;
    }

    std::vector<CompletionItem> allSuggestions;
    QString fullDoc = toPlainText();

    for (const auto& strategy : strategies) {
        auto res = strategy->getSuggestions(textUnder, fullDoc);
        allSuggestions.insert(allSuggestions.end(), res.begin(), res.end());
    }

    model->updateData(allSuggestions);

    if (allSuggestions.empty()) {
        c->popup()->hide();
        return;
    }

    c->setCompletionPrefix(textUnder);
    QRect cr = cursorRect();

    QPoint widgetPos = this->viewport()->mapTo(this, cr.topRight());
    cr.moveTo(widgetPos);

    // Calculate popup width: Text width + Scrollbar + Padding
    int popupWidth = std::clamp(35 + 150 + c->popup()->verticalScrollBar()->width() + 65, 295, 355);

    // Shift dialog left ---
    cr.moveLeft(cr.right() - popupWidth - 360);

    // set width
    cr.setWidth(popupWidth);

    c->complete(cr);

    // select first item in the popped up list
    QAbstractItemView *popup = c->popup();
    if (popup and popup->model()->rowCount() > 0) {
        popup->setCurrentIndex(popup->model()->index(0, 0));
    }
}

QTextCursor TEditor::textUnderCursor() const {
    QTextCursor tc = textCursor();
    tc.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
    return tc;
}

void TEditor::insertCompletion(const QString &completion, CompletionType type) {
    if (c->widget() != this) return;
    // This ensures we replace the whole partial word with the completion.
    QTextCursor tc = textUnderCursor();

    switch (type) {
    case CompletionType::Builtin:
        insertBuiltinFunction(completion, tc);
        break;
    case CompletionType::Snippet:
        insertSnippet(completion, tc);
        break;
    case CompletionType::Keyword:
        insertWord(completion, tc);
        break;
    case CompletionType::DynamicWord:
    default:
        insertWord(completion, tc);
        break;
    }
}
void TEditor::insertWord(const QString& completion, QTextCursor& tc) {
    tc.insertText(completion);
    setTextCursor(tc);
}
void TEditor::insertBuiltinFunction(const QString& functionName, QTextCursor& tc) {
    // Select everything from cursor to end of current word
    QTextCursor tempCursor = textCursor();
    tempCursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);

    tc.insertText(functionName);
    tc.insertText("()");

    tc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);

    // Perform the insertion
    setTextCursor(tc);
}
void TEditor::insertSnippet(const QString& snippet, QTextCursor& tc) {
    QString textToInsert = snippet;

    // Calculate indentation
    // Get the full text of the current line to determine indentation
    QTextBlock block = tc.block();
    QString lineText = block.text();
    QString baseIndentation{};
    for (const QChar &ch : lineText) {
        if (ch.isSpace()) baseIndentation.append(ch);
        else break;
    }

    // Apply indentation to multi-line snippets
    if (textToInsert.contains('\n')) {
        QStringList lines = textToInsert.split('\n');
        // Start from index 1 because index 0 is appended to the current line
        // (which already has indentation on the left).
        // Subsequent lines need the base indentation explicitly added.
        for (int i = 1; i < lines.size(); ++i) {
            lines[i] = baseIndentation + lines[i];
        }
        textToInsert = lines.join('\n');
    }

    // Perform the insertion
    tc.insertText(textToInsert);
    setTextCursor(tc);

    // Reset snippet targets
    snippetTargets.clear();

    // Setup snippet navigation based on snippet content
    if (snippet.startsWith("دالة")) {
        QTextCursor finder = textCursor();
        finder.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, textToInsert.length());
        finder = document()->find("اسم", finder);
        if (!finder.isNull()) setTextCursor(finder);
        snippetTargets << "معاملات" << "مرر";
    }
    else if (snippet.startsWith("صنف")) {
        QTextCursor finder = textCursor();
        finder.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, textToInsert.length());
        finder = document()->find("اسم", finder);
        if (!finder.isNull()) setTextCursor(finder);
        snippetTargets << "مرر";
    }
    else if (snippet.startsWith("اذا")) {
        QTextCursor finder = textCursor();
        finder.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, textToInsert.length());
        finder = document()->find("الشرط", finder);
        if (!finder.isNull()) setTextCursor(finder);
        snippetTargets << "مرر";
    }
    else if (snippet.startsWith("لكل")) {
        QTextCursor finder = textCursor();
        finder.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, textToInsert.length());
        finder = document()->find("عنصر", finder);
        if (!finder.isNull()) setTextCursor(finder);
        snippetTargets << "العناصر" << "مرر";
    }
    else if (snippet.startsWith("بينما")) {
        QTextCursor finder = textCursor();
        finder.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, textToInsert.length());
        finder = document()->find("الشرط", finder);
        if (!finder.isNull()) setTextCursor(finder);
        snippetTargets << "مرر";
    }
    else if (snippet.startsWith("حاول")) {
        QTextCursor finder = textCursor();
        finder.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, textToInsert.length());
        finder = document()->find("مرر", finder);
        if (!finder.isNull()) setTextCursor(finder);
        snippetTargets << "مرر";
    }
    else if (snippet.startsWith("خطية")) {
        QTextCursor finder = textCursor();
        finder.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, textToInsert.length());
        finder = document()->find("معاملات", finder);
        if (!finder.isNull()) setTextCursor(finder);
        snippetTargets << "مرر";
    }

}


bool TEditor::processSnippetNavigation() {
    if (snippetTargets.isEmpty()) return false;
    QString nextTarget = snippetTargets.first();
    QTextCursor tc = textCursor();
    QTextCursor found = document()->find(nextTarget, tc);
    if (!found.isNull()) {
        setTextCursor(found);
        snippetTargets.removeFirst();
        return true;
    }
    snippetTargets.clear();
    return false;
}

bool TEditor::handleAutoPairing(QKeyEvent* e) {
    QString text = e->text();

    if (!text.isEmpty()) {
        QChar typedChar = text.at(0);

        // Handle opening brackets
        if (typedChar == '(' || typedChar == '[' || typedChar == '{') {
            QChar closingBracket;
            if (typedChar == '(') closingBracket = ')';
            else if (typedChar == '[') closingBracket = ']';
            else closingBracket = '}';

            return handleBracketCompletion(typedChar, closingBracket);
        }
        // Handle quotes
        else if (typedChar == '\'' || typedChar == '"' || typedChar == '`') {
                return handleQuoteCompletion(typedChar);
        }
        // Handle closing brackets (skip over existing ones)
        else if (typedChar == ')' || typedChar == ']' || typedChar == '}' ||
                 typedChar == '\'' || typedChar == '"' || typedChar == '`') {
            return handleBracketSkip(typedChar);
        }
    }

    return false;
}

bool TEditor::handleBracketCompletion(QChar openingBracket, QChar closingBracket) {
    QTextCursor cursor = textCursor();

    // Check if there's a selection
    if (cursor.hasSelection()) {
        // Wrap selection with brackets
        QString selectedText = cursor.selectedText();
        cursor.insertText(openingBracket + selectedText + closingBracket);

        // Move cursor after the opening bracket to select the original text
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, selectedText.length() + 1);
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, selectedText.length());
        setTextCursor(cursor);
    } else {
        // Insert both brackets and place cursor between them
        cursor.insertText(QString(openingBracket) + closingBracket);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
        setTextCursor(cursor);
    }

    return true;
}

bool TEditor::handleQuoteCompletion(QChar quoteChar) {
    QTextCursor cursor = textCursor();
    QTextDocument *doc = document();

    // Get the character at cursor position
    int pos = cursor.position();

    // Check if there's a selection
    if (cursor.hasSelection()) {
        // Wrap selection with quotes
        QString selectedText = cursor.selectedText();
        cursor.insertText(quoteChar + selectedText + quoteChar);

        // Move cursor after the opening quote to select the original text
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, selectedText.length() + 1);
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, selectedText.length());
        setTextCursor(cursor);
        return true;
    }

    // Check if next character is the same quote (should skip)
    QChar nextChar;
    if (pos < doc->characterCount() - 1) {
        nextChar = doc->characterAt(pos);
        if (nextChar == quoteChar) {
            // Just move cursor over the existing quote
            cursor.movePosition(QTextCursor::Left);
            setTextCursor(cursor);
            return true;
        }
    }

    // Check if we're inside a word (for smart quotes)
    bool insideWord = false;
    if (pos > 0) {
        QChar prevChar = doc->characterAt(pos - 1);
        insideWord = prevChar.isLetterOrNumber() || prevChar == '_';
    }

    // Insert the quote pair
    cursor.insertText(QString(quoteChar) + quoteChar);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
    setTextCursor(cursor);

    return true;
}

bool TEditor::handleBracketSkip(QChar typedChar) {
    QTextCursor cursor = textCursor();
    QTextDocument *doc = document();
    int pos = cursor.position();

    // Check if the next character matches the typed closing bracket/quote
    if (pos < doc->characterCount() - 1) {
        QChar nextChar = doc->characterAt(pos);
        if (nextChar == typedChar) {
            // Just move the cursor over the existing bracket/quote
            cursor.movePosition(QTextCursor::Left);
            setTextCursor(cursor);
            return true;
        }
    }

    return false;
}
