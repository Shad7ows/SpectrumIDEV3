#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

class SearchPanel : public QWidget {
    Q_OBJECT

public:
    explicit SearchPanel(QWidget *parent = nullptr);
    QString getText() const;
    QString getReplaceText() const;

    bool isCaseSensitive() const;
    bool isWholeWord() const;
    void setFocusToInput();
    void setFocusToReplace();

signals:
    void findText();
    void findNext();
    void findPrevious();
    void closed();
    void replaceRequested();
    void replaceAllRequested();

private:
    QLineEdit *searchInput;
    QLineEdit *replaceInput;

    QPushButton *btnNext;
    QPushButton *btnPrev;
    QPushButton *btnReplace;
    QPushButton *btnReplaceAll;
    QPushButton *btnClose;
    QCheckBox *checkCase;
    QCheckBox *checkWord;
};
