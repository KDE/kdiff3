/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "smalldialogs.h"

#include "diff.h"
#include "FileNameLineEdit.h"
#include "options.h"
#include "kdiff3.h"
#include "TypeUtils.h"
#include "ui_opendialog.h"

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
    dialogUi.setupUi(this);
    setModal(true);
    m_pOptions = pOptions;
    //Abort if verticalLayout is not the immediate child of the dialog. This interferes with re-sizing.
    assert(dialogUi.virticalLayout->parent() == this);

    dialogUi.lineA->insertItems(0, m_pOptions->m_recentAFiles);
    dialogUi.lineA->setEditText(n1);

    QPushButton* button = dialogUi.fileSelectA;
    chk_connect_a(button, &QPushButton::clicked, this, &OpenDialog::selectFileA);
    QPushButton* button2 = dialogUi.folderSelectA;
    chk_connect_a(button2, &QPushButton::clicked, this, &OpenDialog::selectDirA);
    chk_connect_a(dialogUi.lineA, &QComboBox::editTextChanged, this, &OpenDialog::inputFilenameChanged);

    dialogUi.lineB->setEditable(true);
    dialogUi.lineB->insertItems(0, m_pOptions->m_recentBFiles);
    dialogUi.lineB->setEditText(n2);

    dialogUi.lineB->setMinimumWidth(200);
    button = dialogUi.fileSelectB;
    chk_connect_a(button, &QPushButton::clicked, this, &OpenDialog::selectFileB);
    button2 = dialogUi.folderSelectB;
    chk_connect_a(button2, &QPushButton::clicked, this, &OpenDialog::selectDirB);
    chk_connect_a(dialogUi.lineB, &QComboBox::editTextChanged, this, &OpenDialog::inputFilenameChanged);

    dialogUi.lineC->setEditable(true);
    dialogUi.lineC->insertItems(0, m_pOptions->m_recentCFiles);
    dialogUi.lineC->setEditText(n3);
    dialogUi.lineC->setMinimumWidth(200);
    button = dialogUi.fileSelectC;
    chk_connect_a(button, &QPushButton::clicked, this, &OpenDialog::selectFileC);
    button2 = dialogUi.folderSelectC;
    chk_connect_a(button2, &QPushButton::clicked, this, &OpenDialog::selectDirC);
    chk_connect_a(dialogUi.lineC, &QComboBox::editTextChanged, this, &OpenDialog::inputFilenameChanged);

    button = dialogUi.swapCopy;

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

    dialogUi.lineOut->insertItems(0, m_pOptions->m_recentOutputFiles);
    dialogUi.lineOut->setEditText(outputName);

    button = dialogUi.selectOutputFile;
    chk_connect_a(button, &QPushButton::clicked, this, &OpenDialog::selectOutputName);
    button2 = dialogUi.selectOutputFolder;
    chk_connect_a(button2, &QPushButton::clicked, this, &OpenDialog::selectOutputDir);
    chk_connect_a(dialogUi.mergeCheckBox, &QCheckBox::stateChanged, this, &OpenDialog::internalSlot);
    chk_connect_a(this, &OpenDialog::internalSignal, dialogUi.lineOut, &QComboBox::setEnabled);
    chk_connect_a(this, &OpenDialog::internalSignal, button, &QPushButton::setEnabled);
    chk_connect_a(this, &OpenDialog::internalSignal, button2, &QPushButton::setEnabled);

    dialogUi.mergeCheckBox->setChecked(bMerge);

    QDialogButtonBox *box = dialogUi.buttonBox;
    button = box->addButton(i18n("Configure..."), QDialogButtonBox::ActionRole);
    button->setIcon(QIcon::fromTheme("configure"));
    chk_connect_a(button, &QPushButton::clicked, pParent, &KDiff3App::slotConfigure);
    chk_connect_a(box, &QDialogButtonBox::accepted, this, &OpenDialog::accept);
    chk_connect_a(box, &QDialogButtonBox::rejected, this, &OpenDialog::reject);

    QSize sh = sizeHint();
    if(sh.height() > 10)
        setFixedHeight(sh.height());
    else
    {
        //This is likely a bug. It is also a recoverable condition. Assert for it in debug builds.
        assert(sh.isValid() && sh.height() > 10);
        setFixedHeight(262);
    }
    m_bInputFileNameChanged = false;

    /*
        QComboBox's default handling of Drag and Drop fails to clear existing text on drop.
        On some systems it may fail to do anything at all.

        This is not what we want. So manually replace the each QLineEdit object with a FileNameLineEdit.
        This makes behavior consitant with the main window.

        On windows this step also needed to bypasses Qt's quirky behavior when converting from QUrl
        to QString. Specifically % encoding is by handled differently on windows. This is explicitly documented
        as platform specific unspecified behavior. Not what we need.
    */
    dialogUi.lineA->setLineEdit(new FileNameLineEdit(dialogUi.lineA));
    dialogUi.lineB->setLineEdit(new FileNameLineEdit(dialogUi.lineB));
    dialogUi.lineC->setLineEdit(new FileNameLineEdit(dialogUi.lineC));
    dialogUi.lineOut->setLineEdit(new FileNameLineEdit(dialogUi.lineOut));
}

void OpenDialog::selectURL(QComboBox* pLine, bool bDir, qint32 i, bool bSave)
{
    QString current = pLine->currentText();
    QUrl currentUrl;

    if(current.isEmpty() && i > 3)
    {
        current = dialogUi.lineC->currentText();
    }
    if(current.isEmpty())
    {
        current = dialogUi.lineB->currentText();
    }
    if(current.isEmpty())
    {
        current = dialogUi.lineA->currentText();
    }

    currentUrl = QUrl::fromUserInput(current, QString(), QUrl::AssumeLocalFile);
    QUrl newURL = bDir ? QFileDialog::getExistingDirectoryUrl(this, i18n("Open Folder"), currentUrl)
                       : bSave ? QFileDialog::getSaveFileUrl(this, i18n("Select Output File"), currentUrl, i18n("all/allfiles (*)"))
                               : QFileDialog::getOpenFileUrl(this, i18n("Open File"), currentUrl, i18n("all/allfiles (*)"));
    if(!newURL.isEmpty())
    {
        /*
            Since we are selecting a directory open in the parent directory
            not the one selected.
        */
        //QFileDialog::setStartDir( KIO::upUrl( newURL ) );
        pLine->setEditText(newURL.url());
    }
    // newURL won't be modified if nothing was selected.
}

void OpenDialog::selectFileA() { selectURL(dialogUi.lineA, false, 1, false); }
void OpenDialog::selectFileB() { selectURL(dialogUi.lineB, false, 2, false); }
void OpenDialog::selectFileC() { selectURL(dialogUi.lineC, false, 3, false); }
void OpenDialog::selectOutputName() { selectURL(dialogUi.lineOut, false, 4, true); }
void OpenDialog::selectDirA() { selectURL(dialogUi.lineA, true, 1, false); }
void OpenDialog::selectDirB() { selectURL(dialogUi.lineB, true, 2, false); }
void OpenDialog::selectDirC() { selectURL(dialogUi.lineC, true, 3, false); }
void OpenDialog::selectOutputDir() { selectURL(dialogUi.lineOut, true, 4, true); }

void OpenDialog::internalSlot(qint32 i)
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
        dialogUi.lineOut->clearEditText();
    }
}

void OpenDialog::fixCurrentText(QComboBox* pCB)
{
    QString s = pCB->currentText();
    qint32 pos = s.indexOf('\n');

    if(pos >= 0)
        s = s.left(pos);
    pos = s.indexOf('\r');
    if(pos >= 0)
        s = s.left(pos);

    pCB->setEditText(s);
}

void OpenDialog::accept()
{
    qint32 maxNofRecentFiles = 10;
    fixCurrentText(dialogUi.lineA);

    QString s = dialogUi.lineA->currentText();
    s = FileAccess::prettyAbsPath(QUrl::fromUserInput(s, QString(), QUrl::AssumeLocalFile));
    QStringList* sl = &m_pOptions->m_recentAFiles;
    // If an item exist, remove it from the list and reinsert it at the beginning.
    sl->removeAll(s);
    if(!s.isEmpty()) sl->prepend(s);
    if(sl->count() > maxNofRecentFiles) sl->erase(sl->begin() + maxNofRecentFiles, sl->end());

    fixCurrentText(dialogUi.lineB);
    s = dialogUi.lineB->currentText();
    s = FileAccess::prettyAbsPath(QUrl::fromUserInput(s, QString(), QUrl::AssumeLocalFile));
    sl = &m_pOptions->m_recentBFiles;
    sl->removeAll(s);
    if(!s.isEmpty()) sl->prepend(s);
    if(sl->count() > maxNofRecentFiles) sl->erase(sl->begin() + maxNofRecentFiles, sl->end());

    fixCurrentText(dialogUi.lineC);
    s = dialogUi.lineC->currentText();
    s = FileAccess::prettyAbsPath(QUrl::fromUserInput(s, QString(), QUrl::AssumeLocalFile));
    sl = &m_pOptions->m_recentCFiles;
    sl->removeAll(s);
    if(!s.isEmpty()) sl->prepend(s);
    if(sl->count() > maxNofRecentFiles) sl->erase(sl->begin() + maxNofRecentFiles, sl->end());

    fixCurrentText(dialogUi.lineOut);
    s = dialogUi.lineOut->currentText();
    s = FileAccess::prettyAbsPath(QUrl::fromUserInput(s, QString(), QUrl::AssumeLocalFile));
    sl = &m_pOptions->m_recentOutputFiles;
    sl->removeAll(s);
    if(!s.isEmpty()) sl->prepend(s);
    if(sl->count() > maxNofRecentFiles) sl->erase(sl->begin() + maxNofRecentFiles, sl->end());

    QDialog::accept();
}

void OpenDialog::slotSwapCopyNames(QAction* pAction) const // id selected in the popup menu
{
    qint32 id = pAction->parentWidget()->actions().indexOf(pAction);
    QComboBox* cb1 = nullptr;
    QComboBox* cb2 = nullptr;

    switch(id)
    {
        case 0:
            cb1 = dialogUi.lineA;
            cb2 = dialogUi.lineB;
            break;
        case 1:
            cb1 = dialogUi.lineB;
            cb2 = dialogUi.lineC;
            break;
        case 2:
            cb1 = dialogUi.lineC;
            cb2 = dialogUi.lineA;
            break;
        case 3:
            cb1 = dialogUi.lineA;
            cb2 = dialogUi.lineOut;
            break;
        case 4:
            cb1 = dialogUi.lineB;
            cb2 = dialogUi.lineOut;
            break;
        case 5:
            cb1 = dialogUi.lineC;
            cb2 = dialogUi.lineOut;
            break;
        case 6:
            cb1 = dialogUi.lineA;
            cb2 = dialogUi.lineOut;
            break;
        case 7:
            cb1 = dialogUi.lineB;
            cb2 = dialogUi.lineOut;
            break;
        case 8:
            cb1 = dialogUi.lineC;
            cb2 = dialogUi.lineOut;
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
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    qint32 line = 0;
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
    chk_connect(box, &QDialogButtonBox::accepted, this, &FindDialog::accept);
    chk_connect(box, &QDialogButtonBox::rejected, this, &FindDialog::reject);

    hide();
}

void FindDialog::restartFind()
{
    currentLine = 0;
    currentPos = 0;
    currentWindow = eWindowIndex::A;
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
    qint32 line = 0;
    setWindowTitle(i18n("Regular Expression Tester"));
    QGridLayout* pGrid = new QGridLayout(this);
    pGrid->setSpacing(5);
    pGrid->setContentsMargins(5, 5, 5, 5);

    QLabel* l = new QLabel(i18n("Auto merge regular expression:"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(autoMergeRegExpToolTip);
    m_pAutoMergeRegExpEdit = new QLineEdit(this);
    pGrid->addWidget(m_pAutoMergeRegExpEdit, line, 1);
    chk_connect(m_pAutoMergeRegExpEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Example auto merge line:"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(i18n("To test auto merge, copy a line as used in your files."));
    m_pAutoMergeExampleEdit = new QLineEdit(this);
    pGrid->addWidget(m_pAutoMergeExampleEdit, line, 1);
    chk_connect(m_pAutoMergeExampleEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
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
    chk_connect(m_pHistoryStartRegExpEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Example history start line (with leading comment):"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(i18n("Copy a history start line as used in your files,\n"
                       "including the leading comment."));
    m_pHistoryStartExampleEdit = new QLineEdit(this);
    pGrid->addWidget(m_pHistoryStartExampleEdit, line, 1);
    chk_connect(m_pHistoryStartExampleEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
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
    chk_connect(m_pHistoryEntryStartRegExpEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("History sort key order:"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(historySortKeyOrderToolTip);
    m_pHistorySortKeyOrderEdit = new QLineEdit(this);
    pGrid->addWidget(m_pHistorySortKeyOrderEdit, line, 1);
    chk_connect(m_pHistorySortKeyOrderEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
    ++line;

    l = new QLabel(i18n("Example history entry start line (without leading comment):"), this);
    pGrid->addWidget(l, line, 0);
    l->setToolTip(i18n("Copy a history entry start line as used in your files,\n"
                       "but omit the leading comment."));
    m_pHistoryEntryStartExampleEdit = new QLineEdit(this);
    pGrid->addWidget(m_pHistoryEntryStartExampleEdit, line, 1);
    chk_connect(m_pHistoryEntryStartExampleEdit, &QLineEdit::textChanged, this, &RegExpTester::slotRecalc);
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
    chk_connect(box, &QDialogButtonBox::accepted, this, &RegExpTester::accept);
    chk_connect(box, &QDialogButtonBox::rejected, this, &RegExpTester::reject);

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
    QRegularExpression autoMergeRegExp(m_pAutoMergeRegExpEdit->text());
    QRegularExpressionMatch match = autoMergeRegExp.match(m_pAutoMergeExampleEdit->text());

    if(match.hasMatch())
    {
        m_pAutoMergeMatchResult->setText(i18n("Match success."));
    }
    else
    {
        m_pAutoMergeMatchResult->setText(i18n("Match failed."));
    }

    QRegularExpression historyStartRegExp(m_pHistoryStartRegExpEdit->text());
    match = historyStartRegExp.match(m_pHistoryStartExampleEdit->text());

    if(match.hasMatch())
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
    QRegularExpression historyEntryStartRegExp(m_pHistoryEntryStartRegExpEdit->text());
    QString s = m_pHistoryEntryStartExampleEdit->text();
    match = historyEntryStartRegExp.match(s);

    if(match.hasMatch())
    {
        m_pHistoryEntryStartMatchResult->setText(i18n("Match success."));
        QString key = calcHistorySortKey(m_pHistorySortKeyOrderEdit->text(), match, parenthesesGroups);
        m_pHistorySortKeyResult->setText(key);
    }
    else
    {
        m_pHistoryEntryStartMatchResult->setText(i18n("Match failed."));
        m_pHistorySortKeyResult->setText("");
    }
}

//#include "smalldialogs.moc"
