/*
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "smalldialogs.h"
#include "diff.h"
#include "options.h"
#include "kdiff3.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDropEvent>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QToolTip>
#include <QUrl>

#include <KLocalizedString>

// OpenDialog **************************************************************

OpenDialog::OpenDialog(
    KDiff3App* pParent, const QString& n1, const QString& n2, const QString& n3,
    bool bMerge, const QString& outputName,  const QSharedPointer<Options> &pOptions)
    : QDialog(pParent)
{
    setObjectName("OpenDialog");
    setModal(true);
    m_pOptions = pOptions;

    QVBoxLayout* v = new QVBoxLayout(this);
    v->setMargin(5);
    QGridLayout* h = new QGridLayout();
    v->addLayout(h);
    h->setSpacing(5);
    h->setColumnStretch(1, 10);

    QLabel* label = new QLabel(i18n("A (Base):"), this);

    m_pLineA = new QComboBox();
    m_pLineA->setEditable(true);
    m_pLineA->insertItems(0, m_pOptions->m_recentAFiles);
    m_pLineA->setEditText(QUrl(n1).toDisplayString());
    m_pLineA->setMinimumWidth(200);
    QPushButton* button = new QPushButton(QIcon::fromTheme("document-new"), i18n("File..."), this);
    chk_connect_a(button, &QPushButton::clicked, this, &OpenDialog::selectFileA);
    QPushButton* button2 = new QPushButton(QIcon::fromTheme("document-open-folder"), i18n("Folder..."), this);
    chk_connect_a(button2, &QPushButton::clicked, this, &OpenDialog::selectDirA);
    chk_connect_a(m_pLineA, &QComboBox::editTextChanged, this, &OpenDialog::inputFilenameChanged);

    h->addWidget(label, 0, 0);
    h->addWidget(m_pLineA, 0, 1);
    h->addWidget(button, 0, 2);
    h->addWidget(button2, 0, 3);

    label = new QLabel("B:", this);
    m_pLineB = new QComboBox();
    m_pLineB->setEditable(true);
    m_pLineB->insertItems(0, m_pOptions->m_recentBFiles);
    m_pLineB->setEditText(QUrl(n2).toDisplayString());
    m_pLineB->setMinimumWidth(200);
    button = new QPushButton(QIcon::fromTheme("document-new"), i18n("File..."), this);
    chk_connect_a(button, &QPushButton::clicked, this, &OpenDialog::selectFileB);
    button2 = new QPushButton(QIcon::fromTheme("document-open-folder"), i18n("Folder..."), this);
    chk_connect_a(button2, &QPushButton::clicked, this, &OpenDialog::selectDirB);
    chk_connect_a(m_pLineB, &QComboBox::editTextChanged, this, &OpenDialog::inputFilenameChanged);

    h->addWidget(label, 1, 0);
    h->addWidget(m_pLineB, 1, 1);
    h->addWidget(button, 1, 2);
    h->addWidget(button2, 1, 3);

    label = new QLabel(i18n("C (Optional):"), this);
    m_pLineC = new QComboBox();
    m_pLineC->setEditable(true);
    m_pLineC->insertItems(0, m_pOptions->m_recentCFiles);
    m_pLineC->setEditText(QUrl(n3).toDisplayString());
    m_pLineC->setMinimumWidth(200);
    button = new QPushButton(QIcon::fromTheme("document-new"), i18n("File..."), this);
    chk_connect_a(button, &QPushButton::clicked, this, &OpenDialog::selectFileC);
    button2 = new QPushButton(QIcon::fromTheme("document-open-folder"), i18n("Folder..."), this);
    chk_connect_a(button2, &QPushButton::clicked, this, &OpenDialog::selectDirC);
    chk_connect_a(m_pLineC, &QComboBox::editTextChanged, this, &OpenDialog::inputFilenameChanged);

    h->addWidget(label, 2, 0);
    h->addWidget(m_pLineC, 2, 1);
    h->addWidget(button, 2, 2);
    h->addWidget(button2, 2, 3);

    m_pMerge = new QCheckBox(i18n("Merge"), this);
    h->addWidget(m_pMerge, 3, 0);

    QHBoxLayout* hl = new QHBoxLayout();
    h->addLayout(hl, 3, 1);
    hl->addStretch(2);
    button = new QPushButton(i18n("Swap/Copy Names..."), this);
    //button->setToggleButton(false);
    hl->addWidget(button);

    QMenu* m = new QMenu(this);
    m->addAction(i18n("Swap %1<->%2", i18n("A"), i18n("B")));
    m->addAction(i18n("Swap %1<->%2", i18n("B"), i18n("C")));
    m->addAction(i18n("Swap %1<->%2", i18n("C"), i18n("A")));
    m->addAction(i18n("Copy %1->Output", i18n("A")));
    m->addAction(i18n("Copy %1->Output", i18n("B")));
    m->addAction(i18n("Copy %1->Output", i18n("C")));
    m->addAction(i18n("Swap %1<->Output", i18n("A")));
    m->addAction(i18n("Swap %1<->Output", i18n("B")));
    m->addAction(i18n("Swap %1<->Output", i18n("C")));
    chk_connect_a(m, &QMenu::triggered, this, &OpenDialog::slotSwapCopyNames);
    button->setMenu(m);

    hl->addStretch(2);

    label = new QLabel(i18n("Output (optional):"), this);
    m_pLineOut = new QComboBox();
    m_pLineOut->setEditable(true);
    m_pLineOut->insertItems(0, m_pOptions->m_recentOutputFiles);
    m_pLineOut->setEditText(QUrl(outputName).toDisplayString());
    m_pLineOut->setMinimumWidth(200);
    button = new QPushButton(QIcon::fromTheme("document-new"), i18n("File..."), this);
    chk_connect_a(button, &QPushButton::clicked, this, &OpenDialog::selectOutputName);
    button2 = new QPushButton(QIcon::fromTheme("document-open-folder"), i18n("Folder..."), this);
    chk_connect_a(button2, &QPushButton::clicked, this, &OpenDialog::selectOutputDir);
    chk_connect_a(m_pMerge, &QCheckBox::stateChanged, this, &OpenDialog::internalSlot);
    chk_connect_a(this, &OpenDialog::internalSignal, m_pLineOut, &QComboBox::setEnabled);
    chk_connect_a(this, &OpenDialog::internalSignal, button, &QPushButton::setEnabled);
    chk_connect_a(this, &OpenDialog::internalSignal, button2, &QPushButton::setEnabled);

    m_pMerge->setChecked(!bMerge);
    m_pMerge->setChecked(bMerge);
    //   m_pLineOutput->setEnabled( bMerge );

    //   button->setEnabled( bMerge );

    h->addWidget(label, 4, 0);
    h->addWidget(m_pLineOut, 4, 1);
    h->addWidget(button, 4, 2);
    h->addWidget(button2, 4, 3);

    h->addItem(new QSpacerItem(200, 0), 0, 1);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    v->addWidget(box);
    button = box->addButton(i18n("Configure..."), QDialogButtonBox::ActionRole);
    button->setIcon(QIcon::fromTheme("configure"));
    chk_connect_a(button, &QPushButton::clicked, pParent, &KDiff3App::slotConfigure);
    chk_connect_a(box, &QDialogButtonBox::accepted, this, &OpenDialog::accept);
    chk_connect_a(box, &QDialogButtonBox::rejected, this, &OpenDialog::reject);

    QSize sh = sizeHint();
    setFixedHeight(sh.height());
    m_bInputFileNameChanged = false;

#ifdef Q_OS_WIN
    m_pLineA->lineEdit()->installEventFilter(this);
    m_pLineB->lineEdit()->installEventFilter(this);
    m_pLineC->lineEdit()->installEventFilter(this);
    m_pLineOut->lineEdit()->installEventFilter(this);
#endif
}

// Eventfilter: Only needed under Windows.
// Without this, files dropped in the line edit have URL-encoding.
// This eventfilter decodes the filenames as needed by KDiff3.
bool OpenDialog::eventFilter(QObject* o, QEvent* e)
{
    if(e->type() == QEvent::DragEnter)
    {
        QDragEnterEvent* d = static_cast<QDragEnterEvent*>(e);
        d->setAccepted(d->mimeData()->hasUrls());
        return true;
    }
    if(e->type() == QEvent::Drop)
    {
        QDropEvent* d = static_cast<QDropEvent*>(e);

        if(!d->mimeData()->hasUrls())
            return false;

        QList<QUrl> lst = d->mimeData()->urls();

        if(lst.count() > 0)
        {
            static_cast<QLineEdit*>(o)->setText(QDir::toNativeSeparators(lst[0].toLocalFile()));
            static_cast<QLineEdit*>(o)->setFocus();
        }

        return true;
    }
    return false;
}

void OpenDialog::selectURL(QComboBox* pLine, bool bDir, int i, bool bSave)
{
    QString current = pLine->currentText();
    QUrl currentUrl;

    if(current.isEmpty() && i > 3) {
        current = m_pLineC->currentText();
    }
    if(current.isEmpty()) {
        current = m_pLineB->currentText();
    }
    if(current.isEmpty()) {
        current = m_pLineA->currentText();
    }

    currentUrl = QUrl::fromUserInput(current, QString(), QUrl::AssumeLocalFile);
    QUrl newURL = bDir ? QFileDialog::getExistingDirectoryUrl(this, i18n("Open Folder"), currentUrl)
                       : bSave ? QFileDialog::getSaveFileUrl(this, i18n("Select Output File"), currentUrl, i18n("all/allfiles (*)"))
                               : QFileDialog::getOpenFileUrl(this, i18n("Open File"), currentUrl, i18n("all/allfiles (*)"));
    if(!newURL.isEmpty()) {
        /*
        Since we are selecting a directory open in the parent directory
        not the one selected.
             */
        //QFileDialog::setStartDir( KIO::upUrl( newURL ) );
        pLine->setEditText(newURL.url());
    }
    // newURL won't be modified if nothing was selected.
}

void OpenDialog::selectFileA() { selectURL(m_pLineA, false, 1, false); }
void OpenDialog::selectFileB() { selectURL(m_pLineB, false, 2, false); }
void OpenDialog::selectFileC() { selectURL(m_pLineC, false, 3, false); }
void OpenDialog::selectOutputName() { selectURL(m_pLineOut, false, 4, true); }
void OpenDialog::selectDirA() { selectURL(m_pLineA, true, 1, false); }
void OpenDialog::selectDirB() { selectURL(m_pLineB, true, 2, false); }
void OpenDialog::selectDirC() { selectURL(m_pLineC, true, 3, false); }
void OpenDialog::selectOutputDir() { selectURL(m_pLineOut, true, 4, true); }

void OpenDialog::internalSlot(int i)
{
    Q_EMIT internalSignal(i != 0);
}

// Clear the output-filename when any input-filename changed,
// because users forgot to change the output and accidentally overwrote it with
// wrong data during a merge.
void OpenDialog::inputFilenameChanged()
{
    if(!m_bInputFileNameChanged)
    {
        m_bInputFileNameChanged = true;
        m_pLineOut->clearEditText();
    }
}

void OpenDialog::fixCurrentText(QComboBox* pCB)
{
    QString s = pCB->currentText();

    int pos = s.indexOf('\n');
    if(pos >= 0)
        s = s.left(pos);
    pos = s.indexOf('\r');
    if(pos >= 0)
        s = s.left(pos);

    pCB->setEditText(s);
}

void OpenDialog::accept()
{
    int maxNofRecentFiles = 10;
    fixCurrentText(m_pLineA);

    QString s = m_pLineA->currentText();
    s = QUrl::fromUserInput(s, QString(), QUrl::AssumeLocalFile).toLocalFile();
    QStringList* sl = &m_pOptions->m_recentAFiles;
    // If an item exist, remove it from the list and reinsert it at the beginning.
    sl->removeAll(s);
    if(!s.isEmpty()) sl->prepend(s);
    if(sl->count() > maxNofRecentFiles) sl->erase(sl->begin() + maxNofRecentFiles, sl->end());

    fixCurrentText(m_pLineB);
    s = m_pLineB->currentText();
    s = QUrl::fromUserInput(s, QString(), QUrl::AssumeLocalFile).toLocalFile();
    sl = &m_pOptions->m_recentBFiles;
    sl->removeAll(s);
    if(!s.isEmpty()) sl->prepend(s);
    if(sl->count() > maxNofRecentFiles) sl->erase(sl->begin() + maxNofRecentFiles, sl->end());

    fixCurrentText(m_pLineC);
    s = m_pLineC->currentText();
    s = QUrl::fromUserInput(s, QString(), QUrl::AssumeLocalFile).toLocalFile();
    sl = &m_pOptions->m_recentCFiles;
    sl->removeAll(s);
    if(!s.isEmpty()) sl->prepend(s);
    if(sl->count() > maxNofRecentFiles) sl->erase(sl->begin() + maxNofRecentFiles, sl->end());

    fixCurrentText(m_pLineOut);
    s = m_pLineOut->currentText();
    s = QUrl::fromUserInput(s, QString(), QUrl::AssumeLocalFile).toLocalFile();
    sl = &m_pOptions->m_recentOutputFiles;
    sl->removeAll(s);
    if(!s.isEmpty()) sl->prepend(s);
    if(sl->count() > maxNofRecentFiles) sl->erase(sl->begin() + maxNofRecentFiles, sl->end());

    QDialog::accept();
}

void OpenDialog::slotSwapCopyNames(QAction* pAction) const // id selected in the popup menu
{
    int id = pAction->parentWidget()->actions().indexOf(pAction);
    QComboBox* cb1 = nullptr;
    QComboBox* cb2 = nullptr;
    switch(id)
    {
    case 0:
        cb1 = m_pLineA;
        cb2 = m_pLineB;
        break;
    case 1:
        cb1 = m_pLineB;
        cb2 = m_pLineC;
        break;
    case 2:
        cb1 = m_pLineC;
        cb2 = m_pLineA;
        break;
    case 3:
        cb1 = m_pLineA;
        cb2 = m_pLineOut;
        break;
    case 4:
        cb1 = m_pLineB;
        cb2 = m_pLineOut;
        break;
    case 5:
        cb1 = m_pLineC;
        cb2 = m_pLineOut;
        break;
    case 6:
        cb1 = m_pLineA;
        cb2 = m_pLineOut;
        break;
    case 7:
        cb1 = m_pLineB;
        cb2 = m_pLineOut;
        break;
    case 8:
        cb1 = m_pLineC;
        cb2 = m_pLineOut;
        break;
    }
    if(cb1 && cb2)
    {
        QString t1 = cb1->currentText();
        QString t2 = cb2->currentText();
        cb2->setEditText(t1);
        if(id <= 2 || id >= 6)
        {
            cb1->setEditText(t2);
        }
    }
}

// FindDialog *********************************************

FindDialog::FindDialog(QWidget* pParent)
    : QDialog(pParent)
{
    QGridLayout* layout = new QGridLayout(this);
    layout->setMargin(5);
    layout->setSpacing(5);

    int line = 0;
    layout->addWidget(new QLabel(i18n("Search text:"), this), line, 0, 1, 2);
    ++line;

    m_pSearchString = new QLineEdit(this);
    layout->addWidget(m_pSearchString, line, 0, 1, 2);
    ++line;

    m_pCaseSensitive = new QCheckBox(i18n("Case sensitive"), this);
    layout->addWidget(m_pCaseSensitive, line, 1);

    m_pSearchInA = new QCheckBox(i18n("Search A"), this);
    layout->addWidget(m_pSearchInA, line, 0);
    m_pSearchInA->setChecked(true);
    ++line;

    m_pSearchInB = new QCheckBox(i18n("Search B"), this);
    layout->addWidget(m_pSearchInB, line, 0);
    m_pSearchInB->setChecked(true);
    ++line;

    m_pSearchInC = new QCheckBox(i18n("Search C"), this);
    layout->addWidget(m_pSearchInC, line, 0);
    m_pSearchInC->setChecked(true);
    ++line;

    m_pSearchInOutput = new QCheckBox(i18n("Search output"), this);
    layout->addWidget(m_pSearchInOutput, line, 0);
    m_pSearchInOutput->setChecked(true);
    ++line;

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(box, line, 0, 1, 2);
    box->addButton(i18n("&Search"), QDialogButtonBox::AcceptRole);
    chk_connect_a(box, &QDialogButtonBox::accepted, this, &FindDialog::accept);
    chk_connect_a(box, &QDialogButtonBox::rejected, this, &FindDialog::reject);

    hide();
}

void FindDialog::setVisible(bool bVisible)
{
    QDialog::setVisible(bVisible);
    m_pSearchString->selectAll();
    m_pSearchString->setFocus();
}

RegExpTester::RegExpTester(QWidget* pParent, const QString& autoMergeRegExpToolTip,
                           const QString& historyStartRegExpToolTip, const QString& historyEntryStartRegExpToolTip, const QString& historySortKeyOrderToolTip)
    : QDialog(pParent)
{
    int line = 0;
    setWindowTitle(i18n("Regular Expression Tester"));
    QGridLayout* pGrid = new QGridLayout(this);
    pGrid->setSpacing(5);
    pGrid->setMargin(5);

    QLabel* l = new QLabel(i18n("Auto merge regular expression:"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(autoMergeRegExpToolTip);
    m_pAutoMergeRegExpEdit = new QLineEdit(this);
    pGrid->addWidget(m_pAutoMergeRegExpEdit, line, 1);
    chk_connect_a(m_pAutoMergeRegExpEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Example auto merge line:"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(i18n("To test auto merge, copy a line as used in your files."));
    m_pAutoMergeExampleEdit = new QLineEdit(this);
    pGrid->addWidget(m_pAutoMergeExampleEdit, line, 1);
    chk_connect_a(m_pAutoMergeExampleEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Match result:"), this);
    pGrid->addWidget(l, line, 0);
    m_pAutoMergeMatchResult = new QLineEdit(this);
    m_pAutoMergeMatchResult->setReadOnly(true);
    pGrid->addWidget(m_pAutoMergeMatchResult, line, 1);
    ++line;

    pGrid->addItem(new QSpacerItem(100, 20), line, 0);
    pGrid->setRowStretch(line, 5);
    ++line;

    l = new QLabel(i18n("History start regular expression:"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(historyStartRegExpToolTip);
    m_pHistoryStartRegExpEdit = new QLineEdit(this);
    pGrid->addWidget(m_pHistoryStartRegExpEdit, line, 1);
    chk_connect_a(m_pHistoryStartRegExpEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Example history start line (with leading comment):"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(i18n("Copy a history start line as used in your files,\n"
                       "including the leading comment."));
    m_pHistoryStartExampleEdit = new QLineEdit(this);
    pGrid->addWidget(m_pHistoryStartExampleEdit, line, 1);
    chk_connect_a(m_pHistoryStartExampleEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Match result:"), this);
    pGrid->addWidget(l, line, 0);
    m_pHistoryStartMatchResult = new QLineEdit(this);
    m_pHistoryStartMatchResult->setReadOnly(true);
    pGrid->addWidget(m_pHistoryStartMatchResult, line, 1);
    ++line;

    pGrid->addItem(new QSpacerItem(100, 20), line, 0);
    pGrid->setRowStretch(line, 5);
    ++line;

    l = new QLabel(i18n("History entry start regular expression:"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(historyEntryStartRegExpToolTip);
    m_pHistoryEntryStartRegExpEdit = new QLineEdit(this);
    pGrid->addWidget(m_pHistoryEntryStartRegExpEdit, line, 1);
    chk_connect_a(m_pHistoryEntryStartRegExpEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("History sort key order:"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(historySortKeyOrderToolTip);
    m_pHistorySortKeyOrderEdit = new QLineEdit(this);
    pGrid->addWidget(m_pHistorySortKeyOrderEdit, line, 1);
    chk_connect_a(m_pHistorySortKeyOrderEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Example history entry start line (without leading comment):"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(i18n("Copy a history entry start line as used in your files,\n"
                       "but omit the leading comment."));
    m_pHistoryEntryStartExampleEdit = new QLineEdit(this);
    pGrid->addWidget(m_pHistoryEntryStartExampleEdit, line, 1);
    chk_connect_a(m_pHistoryEntryStartExampleEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Match result:"), this);
    pGrid->addWidget(l, line, 0);
    m_pHistoryEntryStartMatchResult = new QLineEdit(this);
    m_pHistoryEntryStartMatchResult->setReadOnly(true);
    pGrid->addWidget(m_pHistoryEntryStartMatchResult, line, 1);
    ++line;

    l = new QLabel(i18n("Sort key result:"), this);
    pGrid->addWidget(l, line, 0);
    m_pHistorySortKeyResult = new QLineEdit(this);
    m_pHistorySortKeyResult->setReadOnly(true);
    pGrid->addWidget(m_pHistorySortKeyResult, line, 1);
    ++line;

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    pGrid->addWidget(box, line, 0, 1, 2);
    chk_connect_a(box, &QDialogButtonBox::accepted, this, &RegExpTester::accept);
    chk_connect_a(box, &QDialogButtonBox::rejected, this, &RegExpTester::reject);

    resize(800, sizeHint().height());
}

void RegExpTester::init(const QString& autoMergeRegExp, const QString& historyStartRegExp, const QString& historyEntryStartRegExp, const QString& historySortKeyOrder)
{
    m_pAutoMergeRegExpEdit->setText(autoMergeRegExp);
    m_pHistoryStartRegExpEdit->setText(historyStartRegExp);
    m_pHistoryEntryStartRegExpEdit->setText(historyEntryStartRegExp);
    m_pHistorySortKeyOrderEdit->setText(historySortKeyOrder);
}

QString RegExpTester::autoMergeRegExp()
{
    return m_pAutoMergeRegExpEdit->text();
}

QString RegExpTester::historyStartRegExp()
{
    return m_pHistoryStartRegExpEdit->text();
}

QString RegExpTester::historyEntryStartRegExp()
{
    return m_pHistoryEntryStartRegExpEdit->text();
}

QString RegExpTester::historySortKeyOrder()
{
    return m_pHistorySortKeyOrderEdit->text();
}

void RegExpTester::slotRecalc()
{
    QRegExp autoMergeRegExp(m_pAutoMergeRegExpEdit->text());
    if(autoMergeRegExp.exactMatch(m_pAutoMergeExampleEdit->text()))
    {
        m_pAutoMergeMatchResult->setText(i18n("Match success."));
    }
    else
    {
        m_pAutoMergeMatchResult->setText(i18n("Match failed."));
    }

    QRegExp historyStartRegExp(m_pHistoryStartRegExpEdit->text());
    if(historyStartRegExp.exactMatch(m_pHistoryStartExampleEdit->text()))
    {
        m_pHistoryStartMatchResult->setText(i18n("Match success."));
    }
    else
    {
        m_pHistoryStartMatchResult->setText(i18n("Match failed."));
    }

    QStringList parenthesesGroups;
    bool bSuccess = findParenthesesGroups(m_pHistoryEntryStartRegExpEdit->text(), parenthesesGroups);
    if(!bSuccess)
    {
        m_pHistoryEntryStartMatchResult->setText(i18n("Opening and closing parentheses do not match in regular expression."));
        m_pHistorySortKeyResult->setText("");
        return;
    }
    QRegExp historyEntryStartRegExp(m_pHistoryEntryStartRegExpEdit->text());
    QString s = m_pHistoryEntryStartExampleEdit->text();

    if(historyEntryStartRegExp.exactMatch(s))
    {
        m_pHistoryEntryStartMatchResult->setText(i18n("Match success."));
        QString key = calcHistorySortKey(m_pHistorySortKeyOrderEdit->text(), historyEntryStartRegExp, parenthesesGroups);
        m_pHistorySortKeyResult->setText(key);
    }
    else
    {
        m_pHistoryEntryStartMatchResult->setText(i18n("Match failed."));
        m_pHistorySortKeyResult->setText("");
    }
}

//#include "smalldialogs.moc"
