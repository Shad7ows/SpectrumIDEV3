#include "TSearchPanel.h"
#include <QStyle>

SearchPanel::SearchPanel(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    auto *searchLayout = new QHBoxLayout();
    auto *replaceLayout = new QHBoxLayout();

    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText("بحث...");
    searchInput->setStyleSheet("border: 1px solid #3e3e42; background: #252526; color: #cccccc; padding: 4px; border-radius: 3px;");

    btnNext = new QPushButton("التالي", this);
    btnPrev = new QPushButton("السابق", this);

    QString btnStyle = "QPushButton { background: transparent; border: 1px solid transparent; color: #cccccc; padding: 4px; } QPushButton:hover { background: #334466; border-radius: 3px; }";
    btnNext->setStyleSheet(btnStyle);
    btnPrev->setStyleSheet(btnStyle);

    btnClose = new QPushButton(this);
    btnClose->setIcon(QIcon(":/icons/resources/close.svg"));
    btnClose->setStyleSheet("QPushButton { background: transparent; border: none; color: white; } QPushButton:hover { background: #334466; border-radius: 3px; }");
    btnClose->setFixedSize(30, 30);

    checkCase = new QCheckBox("Aa", this);
    checkCase->setToolTip("مطابقة حالة الأحرف");
    checkCase->setStyleSheet("color: #cccccc;");

    searchLayout->addWidget(btnClose);
    searchLayout->addWidget(searchInput);
    searchLayout->addWidget(btnNext);
    searchLayout->addWidget(btnPrev);
    searchLayout->addWidget(checkCase);
    searchLayout->addStretch();

    replaceInput = new QLineEdit(this);
    replaceInput->setPlaceholderText("استبدال بـ...");
    replaceInput->setStyleSheet("border: 1px solid #3e3e42; background: #252526; color: #cccccc; padding: 4px; border-radius: 3px;");

    btnReplace = new QPushButton("استبدال", this);
    btnReplaceAll = new QPushButton("استبدال الكل", this);

    btnReplace->setStyleSheet(btnStyle);
    btnReplaceAll->setStyleSheet(btnStyle);

    replaceLayout->addSpacing(35);
    replaceLayout->addWidget(replaceInput);
    replaceLayout->addWidget(btnReplace);
    replaceLayout->addWidget(btnReplaceAll);
    replaceLayout->addStretch();

    mainLayout->addLayout(searchLayout);
    mainLayout->addLayout(replaceLayout);

    setFixedHeight(75);
    QString styleSheet = "background-color: #1e202e; border-top: 1px solid #3e3e42;";
    this->setStyleSheet(styleSheet);

    connect(btnNext, &QPushButton::clicked, this, &SearchPanel::findNext);
    connect(btnPrev, &QPushButton::clicked, this, &SearchPanel::findPrevious);
    connect(btnClose, &QPushButton::clicked, this, &SearchPanel::closed);

    connect(searchInput, &QLineEdit::returnPressed, this, &SearchPanel::findNext);
    connect(searchInput, &QLineEdit::textChanged, this, &SearchPanel::findText);

    connect(btnReplace, &QPushButton::clicked, this, &SearchPanel::replaceRequested);
    connect(btnReplaceAll, &QPushButton::clicked, this, &SearchPanel::replaceAllRequested);
    connect(replaceInput, &QLineEdit::returnPressed, this, &SearchPanel::replaceRequested);
}

QString SearchPanel::getText() const { return searchInput->text(); }
QString SearchPanel::getReplaceText() const { return replaceInput->text(); }
bool SearchPanel::isCaseSensitive() const { return checkCase->isChecked(); }
bool SearchPanel::isWholeWord() const { return false; }
void SearchPanel::setFocusToInput() { searchInput->setFocus(); searchInput->selectAll(); }
void SearchPanel::setFocusToReplace() { replaceInput->setFocus(); replaceInput->selectAll(); }
