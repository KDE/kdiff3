/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mergeresultwindow.h"

#include "defmac.h"
#include "kdiff3.h"
#include "options.h"
#include "RLPainter.h"
#include "guiutils.h"
#include "Utils.h"             // for Utils

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QInputEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QtMath>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QRegExp>
#include <QResizeEvent>
#include <QStatusBar>
#include <QTextCodec>
#include <QTextLayout>
#include <QTextStream>
#include <QTimerEvent>
#include <QUrl>
#include <QWheelEvent>

#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>
#include <KToggleAction>

bool g_bAutoSolve = true;

QScrollBar* MergeResultWindow::mVScrollBar = nullptr;
QPointer<QAction> MergeResultWindow::chooseAEverywhere;
QPointer<QAction> MergeResultWindow::chooseBEverywhere;
QPointer<QAction> MergeResultWindow::chooseCEverywhere;
QPointer<QAction> MergeResultWindow::chooseAForUnsolvedConflicts;
QPointer<QAction> MergeResultWindow::chooseBForUnsolvedConflicts;
QPointer<QAction> MergeResultWindow::chooseCForUnsolvedConflicts;
QPointer<QAction> MergeResultWindow::chooseAForUnsolvedWhiteSpaceConflicts;
QPointer<QAction> MergeResultWindow::chooseBForUnsolvedWhiteSpaceConflicts;
QPointer<QAction> MergeResultWindow::chooseCForUnsolvedWhiteSpaceConflicts;

MergeResultWindow::MergeResultWindow(
    QWidget* pParent,
    const QSharedPointer<Options> &pOptions,
    QStatusBar* pStatusBar)
    : QWidget(pParent)
{
    setObjectName("MergeResultWindow");
    setFocusPolicy(Qt::ClickFocus);

    mOverviewMode = e_OverviewMode::eOMNormal;

    m_pStatusBar = pStatusBar;
    if(m_pStatusBar != nullptr)
        chk_connect_a(m_pStatusBar, &QStatusBar::messageChanged, this, &MergeResultWindow::slotStatusMessageChanged);

    m_pOptions = pOptions;
    setUpdatesEnabled(false);

    chk_connect_a(&m_cursorTimer, &QTimer::timeout, this, &MergeResultWindow::slotCursorUpdate);
    m_cursorTimer.setSingleShot(true);
    m_cursorTimer.start(500 /*ms*/);
    m_selection.reset();

    setMinimumSize(QSize(20, 20));
    setFont(m_pOptions->m_font);
}

void MergeResultWindow::init(
    const QVector<LineData>* pLineDataA, LineRef sizeA,
    const QVector<LineData>* pLineDataB, LineRef sizeB,
    const QVector<LineData>* pLineDataC, LineRef sizeC,
    const Diff3LineList* pDiff3LineList,
    TotalDiffStatus* pTotalDiffStatus)
{
    m_firstLine = 0;
    m_horizScrollOffset = 0;
    m_nofLines = 0;
    m_bMyUpdate = false;
    m_bInsertMode = true;
    m_scrollDeltaX = 0;
    m_scrollDeltaY = 0;
    setModified(false);

    m_pldA = pLineDataA;
    m_pldB = pLineDataB;
    m_pldC = pLineDataC;
    m_sizeA = sizeA;
    m_sizeB = sizeB;
    m_sizeC = sizeC;

    m_pDiff3LineList = pDiff3LineList;
    m_pTotalDiffStatus = pTotalDiffStatus;

    m_selection.reset();
    m_cursorXPos = 0;
    m_cursorOldXPixelPos = 0;
    m_cursorYPos = 0;

    m_maxTextWidth = -1;

    merge(g_bAutoSolve, e_SrcSelector::Invalid);
    g_bAutoSolve = true;
    update();
    updateSourceMask();

    showUnsolvedConflictsStatusMessage();
}

//This must be called before KXMLGui::SetXMLFile and friends or the actions will not be shown in the menu.
//At that point in startup we don't have a MergeResultWindow object so we cannot connect the signals yet.
void MergeResultWindow::initActions(KActionCollection* ac)
{
    if(ac == nullptr){
        KMessageBox::error(nullptr, "actionCollection==0");
        exit(-1);//we cannot recover from this.
    }

    chooseAEverywhere = GuiUtils::createAction<QAction>(i18n("Choose A Everywhere"), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_1), ac, "merge_choose_a_everywhere");
    chooseBEverywhere = GuiUtils::createAction<QAction>(i18n("Choose B Everywhere"), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_2), ac, "merge_choose_b_everywhere");
    chooseCEverywhere = GuiUtils::createAction<QAction>(i18n("Choose C Everywhere"), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_3), ac, "merge_choose_c_everywhere");
    chooseAForUnsolvedConflicts = GuiUtils::createAction<QAction>(i18n("Choose A for All Unsolved Conflicts"), ac, "merge_choose_a_for_unsolved_conflicts");
    chooseBForUnsolvedConflicts = GuiUtils::createAction<QAction>(i18n("Choose B for All Unsolved Conflicts"), ac, "merge_choose_b_for_unsolved_conflicts");
    chooseCForUnsolvedConflicts = GuiUtils::createAction<QAction>(i18n("Choose C for All Unsolved Conflicts"), ac, "merge_choose_c_for_unsolved_conflicts");
    chooseAForUnsolvedWhiteSpaceConflicts = GuiUtils::createAction<QAction>(i18n("Choose A for All Unsolved Whitespace Conflicts"), ac, "merge_choose_a_for_unsolved_whitespace_conflicts");
    chooseBForUnsolvedWhiteSpaceConflicts = GuiUtils::createAction<QAction>(i18n("Choose B for All Unsolved Whitespace Conflicts"), ac, "merge_choose_b_for_unsolved_whitespace_conflicts");
    chooseCForUnsolvedWhiteSpaceConflicts = GuiUtils::createAction<QAction>(i18n("Choose C for All Unsolved Whitespace Conflicts"), ac, "merge_choose_c_for_unsolved_whitespace_conflicts");
}

void MergeResultWindow::connectActions() const
{
    chk_connect_a(chooseAEverywhere, &QAction::triggered, this, &MergeResultWindow::slotChooseAEverywhere);
    chk_connect_a(chooseBEverywhere, &QAction::triggered, this, &MergeResultWindow::slotChooseBEverywhere);
    chk_connect_a(chooseCEverywhere, &QAction::triggered, this, &MergeResultWindow::slotChooseCEverywhere);

    chk_connect_a(chooseAForUnsolvedConflicts, &QAction::triggered, this, &MergeResultWindow::slotChooseAForUnsolvedConflicts);
    chk_connect_a(chooseBForUnsolvedConflicts, &QAction::triggered, this, &MergeResultWindow::slotChooseBForUnsolvedConflicts);
    chk_connect_a(chooseCForUnsolvedConflicts, &QAction::triggered, this, &MergeResultWindow::slotChooseCForUnsolvedConflicts);

    chk_connect_a(chooseAForUnsolvedWhiteSpaceConflicts, &QAction::triggered, this, &MergeResultWindow::slotChooseAForUnsolvedWhiteSpaceConflicts);
    chk_connect_a(chooseBForUnsolvedWhiteSpaceConflicts, &QAction::triggered, this, &MergeResultWindow::slotChooseBForUnsolvedWhiteSpaceConflicts);
    chk_connect_a(chooseCForUnsolvedWhiteSpaceConflicts, &QAction::triggered, this, &MergeResultWindow::slotChooseCForUnsolvedWhiteSpaceConflicts);
}

void MergeResultWindow::setupConnections(const KDiff3App *app)
{
    chk_connect_a(app, &KDiff3App::cut, this, &MergeResultWindow::slotCut);
    chk_connect_a(app, &KDiff3App::selectAll, this, &MergeResultWindow::slotSelectAll);

    chk_connect_a(this, &MergeResultWindow::scrollVertically, mVScrollBar, &QScrollBar::setValue);
    chk_connect_a(this, &MergeResultWindow::scrollMergeResultWindow, app, &KDiff3App::scrollMergeResultWindow);
    chk_connect_a(this, &MergeResultWindow::sourceMask, app, &KDiff3App::sourceMask);
    chk_connect_a(this, &MergeResultWindow::resizeSignal, app, &KDiff3App::setHScrollBarRange);
    chk_connect_a(this, &MergeResultWindow::resizeSignal, this, &MergeResultWindow::slotResize);

    chk_connect_a(this, &MergeResultWindow::selectionEnd, app, &KDiff3App::slotSelectionEnd);
    chk_connect_a(this, &MergeResultWindow::newSelection, app, &KDiff3App::slotSelectionStart);
    chk_connect_a(this, &MergeResultWindow::modifiedChanged, app, &KDiff3App::slotOutputModified);
    chk_connect_a(this, &MergeResultWindow::updateAvailabilities, app, &KDiff3App::slotUpdateAvailabilities);
    chk_connect_a(this, &MergeResultWindow::showPopupMenu, app, &KDiff3App::showPopupMenu);
    chk_connect_a(this, &MergeResultWindow::noRelevantChangesDetected, app, &KDiff3App::slotNoRelevantChangesDetected);
    chk_connect_a(this, &MergeResultWindow::statusBarMessage, app, &KDiff3App::slotStatusMsg);
    //connect menu actions
    chk_connect_a(app, &KDiff3App::showWhiteSpaceToggled, this, static_cast<void (MergeResultWindow::*)(void)>(&MergeResultWindow::update));
    chk_connect_a(app, &KDiff3App::doRefresh, this, &MergeResultWindow::slotRefresh);

    chk_connect_a(app, &KDiff3App::autoSolve, this, &MergeResultWindow::slotAutoSolve);
    chk_connect_a(app, &KDiff3App::unsolve, this, &MergeResultWindow::slotUnsolve);
    chk_connect_a(app, &KDiff3App::mergeHistory, this, &MergeResultWindow::slotMergeHistory);
    chk_connect_a(app, &KDiff3App::regExpAutoMerge, this, &MergeResultWindow::slotRegExpAutoMerge);

    chk_connect_a(app, &KDiff3App::goCurrent, this, &MergeResultWindow::slotGoCurrent);
    chk_connect_a(app, &KDiff3App::goTop, this, &MergeResultWindow::slotGoTop);
    chk_connect_a(app, &KDiff3App::goBottom, this, &MergeResultWindow::slotGoBottom);
    chk_connect_a(app, &KDiff3App::goPrevUnsolvedConflict, this, &MergeResultWindow::slotGoPrevUnsolvedConflict);
    chk_connect_a(app, &KDiff3App::goNextUnsolvedConflict, this, &MergeResultWindow::slotGoNextUnsolvedConflict);
    chk_connect_a(app, &KDiff3App::goPrevConflict, this, &MergeResultWindow::slotGoPrevConflict);
    chk_connect_a(app, &KDiff3App::goNextConflict, this, &MergeResultWindow::slotGoNextConflict);
    chk_connect_a(app, &KDiff3App::goPrevDelta, this, &MergeResultWindow::slotGoPrevDelta);
    chk_connect_a(app, &KDiff3App::goNextDelta, this, &MergeResultWindow::slotGoNextDelta);

    chk_connect_a(app, &KDiff3App::changeOverViewMode, this, &MergeResultWindow::setOverviewMode);

    connections.push_back(KDiff3App::allowCut.connect(boost::bind(&MergeResultWindow::canCut, this)));
}

void MergeResultWindow::slotResize()
{
    mVScrollBar->setRange(0, std::max(0, getNofLines() - getNofVisibleLines()));
    mVScrollBar->setPageStep(getNofVisibleLines());
}

void MergeResultWindow::slotCut()
{
    const QString curSelection = getSelection();
    Q_ASSERT(!curSelection.isEmpty() && hasFocus());
    deleteSelection();
    update();

    QApplication::clipboard()->setText(curSelection, QClipboard::Clipboard);
}

void MergeResultWindow::slotSelectAll()
{
    if(hasFocus())
    {
        setSelection(0, 0, getNofLines(), 0);
    }
}

void MergeResultWindow::showUnsolvedConflictsStatusMessage()
{
    if(m_pStatusBar != nullptr)
    {
        int wsc;
        int nofUnsolved = getNrOfUnsolvedConflicts(&wsc);

        m_persistentStatusMessage = i18n("Number of remaining unsolved conflicts: %1 (of which %2 are whitespace)", nofUnsolved, wsc);

        Q_EMIT statusBarMessage(m_persistentStatusMessage);
    }
}

void MergeResultWindow::slotRefresh()
{
    setFont(m_pOptions->m_font);
    update();
}

void MergeResultWindow::slotUpdateAvailabilities()
{
    const QWidget* frame = qobject_cast<QWidget*>(parent());
    Q_ASSERT(frame != nullptr);
    const bool bMergeEditorVisible = frame->isVisible();
    const bool bTripleDiff = KDiff3App::isTripleDiff();

    chooseAEverywhere->setEnabled(bMergeEditorVisible);
    chooseBEverywhere->setEnabled(bMergeEditorVisible);
    chooseCEverywhere->setEnabled(bMergeEditorVisible && bTripleDiff);
    chooseAForUnsolvedConflicts->setEnabled(bMergeEditorVisible);
    chooseBForUnsolvedConflicts->setEnabled(bMergeEditorVisible);
    chooseCForUnsolvedConflicts->setEnabled(bMergeEditorVisible && bTripleDiff);
    chooseAForUnsolvedWhiteSpaceConflicts->setEnabled(bMergeEditorVisible);
    chooseBForUnsolvedWhiteSpaceConflicts->setEnabled(bMergeEditorVisible);
    chooseCForUnsolvedWhiteSpaceConflicts->setEnabled(bMergeEditorVisible && bTripleDiff);
}

void MergeResultWindow::slotStatusMessageChanged(const QString& s)
{
    if(s.isEmpty() && !m_persistentStatusMessage.isEmpty())
    {
        Q_EMIT statusBarMessage(m_persistentStatusMessage);
    }
}

void MergeResultWindow::reset()
{
    m_pDiff3LineList = nullptr;
    m_pTotalDiffStatus = nullptr;
    m_pldA = nullptr;
    m_pldB = nullptr;
    m_pldC = nullptr;
    if(!m_persistentStatusMessage.isEmpty())
    {
        m_persistentStatusMessage = QString();
    }
}

// Calculate the merge information for the given Diff3Line.
// Results will be stored in mergeDetails, bConflict, bLineRemoved and src.
void Diff3Line::mergeOneLine(
    e_MergeDetails& mergeDetails, bool& bConflict,
    bool& bLineRemoved, e_SrcSelector& src, bool bTwoInputs) const
{
    mergeDetails = e_MergeDetails::eDefault;
    bConflict = false;
    bLineRemoved = false;
    src = e_SrcSelector::None;

    if(bTwoInputs) // Only two input files
    {
        if(getLineA().isValid() && getLineB().isValid())
        {
            if(pFineAB == nullptr)
            {
                mergeDetails = e_MergeDetails::eNoChange;
                src = e_SrcSelector::A;
            }
            else
            {
                mergeDetails = e_MergeDetails::eBChanged;
                bConflict = true;
            }
        }
        else
        {
            mergeDetails = e_MergeDetails::eBDeleted;
            bConflict = true;
        }
        return;
    }

    // A is base.
    if(getLineA().isValid() && getLineB().isValid() && getLineC().isValid())
    {
        if(pFineAB == nullptr && pFineBC == nullptr && pFineCA == nullptr)
        {
            mergeDetails = e_MergeDetails::eNoChange;
            src = e_SrcSelector::A;
        }
        else if(pFineAB == nullptr && pFineBC != nullptr && pFineCA != nullptr)
        {
            mergeDetails = e_MergeDetails::eCChanged;
            src = e_SrcSelector::C;
        }
        else if(pFineAB != nullptr && pFineBC != nullptr && pFineCA == nullptr)
        {
            mergeDetails = e_MergeDetails::eBChanged;
            src = e_SrcSelector::B;
        }
        else if(pFineAB != nullptr && pFineBC == nullptr && pFineCA != nullptr)
        {
            mergeDetails = e_MergeDetails::eBCChangedAndEqual;
            src = e_SrcSelector::C;
        }
        else if(pFineAB != nullptr && pFineBC != nullptr && pFineCA != nullptr)
        {
            mergeDetails = e_MergeDetails::eBCChanged;
            bConflict = true;
        }
        else
            Q_ASSERT(true);
    }
    else if(getLineA().isValid() && getLineB().isValid() && !getLineC().isValid())
    {
        if(pFineAB != nullptr)
        {
            mergeDetails = e_MergeDetails::eBChanged_CDeleted;
            bConflict = true;
        }
        else
        {
            mergeDetails = e_MergeDetails::eCDeleted;
            bLineRemoved = true;
            src = e_SrcSelector::C;
        }
    }
    else if(getLineA().isValid() && !getLineB().isValid() && getLineC().isValid())
    {
        if(pFineCA != nullptr)
        {
            mergeDetails = e_MergeDetails::eCChanged_BDeleted;
            bConflict = true;
        }
        else
        {
            mergeDetails = e_MergeDetails::eBDeleted;
            bLineRemoved = true;
            src = e_SrcSelector::B;
        }
    }
    else if(!getLineA().isValid() && getLineB().isValid() && getLineC().isValid())
    {
        if(pFineBC != nullptr)
        {
            mergeDetails = e_MergeDetails::eBCAdded;
            bConflict = true;
        }
        else // B==C
        {
            mergeDetails = e_MergeDetails::eBCAddedAndEqual;
            src = e_SrcSelector::C;
        }
    }
    else if(!getLineA().isValid() && !getLineB().isValid() && getLineC().isValid())
    {
        mergeDetails = e_MergeDetails::eCAdded;
        src = e_SrcSelector::C;
    }
    else if(!getLineA().isValid() && getLineB().isValid() && !getLineC().isValid())
    {
        mergeDetails = e_MergeDetails::eBAdded;
        src = e_SrcSelector::B;
    }
    else if(getLineA().isValid() && !getLineB().isValid() && !getLineC().isValid())
    {
        mergeDetails = e_MergeDetails::eBCDeleted;
        bLineRemoved = true;
        src = e_SrcSelector::C;
    }
    else
        Q_ASSERT(true);
}

bool MergeResultWindow::sameKindCheck(const MergeLine& ml1, const MergeLine& ml2)
{
    if(ml1.bConflict && ml2.bConflict)
    {
        // Both lines have conflicts: If one is only a white space conflict and
        // the other one is a real conflict, then this line returns false.
        return ml1.id3l->isEqualAC() == ml2.id3l->isEqualAC() && ml1.id3l->isEqualAB() == ml2.id3l->isEqualAB();
    }
    else
        return (
            (!ml1.bConflict && !ml2.bConflict && ml1.bDelta && ml2.bDelta && ml1.srcSelect == ml2.srcSelect && (ml1.mergeDetails == ml2.mergeDetails || (ml1.mergeDetails != e_MergeDetails::eBCAddedAndEqual && ml2.mergeDetails != e_MergeDetails::eBCAddedAndEqual))) ||
            (!ml1.bDelta && !ml2.bDelta));
}

void MergeResultWindow::merge(bool bAutoSolve, e_SrcSelector defaultSelector, bool bConflictsOnly, bool bWhiteSpaceOnly)
{
    if(!bConflictsOnly)
    {
        if(m_bModified)
        {
            int result = KMessageBox::warningYesNo(this,
                                                   i18n("The output has been modified.\n"
                                                        "If you continue your changes will be lost."),
                                                   i18n("Warning"),
                                                   KStandardGuiItem::cont(),
                                                   KStandardGuiItem::cancel());
            if(result == KMessageBox::No)
                return;
        }

        m_mergeLineList.clear();

        int lineIdx = 0;
        Diff3LineList::const_iterator it;
        for(it = m_pDiff3LineList->begin(); it != m_pDiff3LineList->end(); ++it, ++lineIdx)
        {
            const Diff3Line& d = *it;

            MergeLine ml;
            bool bLineRemoved;
            d.mergeOneLine( ml.mergeDetails, ml.bConflict, bLineRemoved, ml.srcSelect, m_pldC == nullptr);

            // Automatic solving for only whitespace changes.
            if(ml.bConflict &&
               ((m_pldC == nullptr && (d.isEqualAB() || (d.isWhiteLine(e_SrcSelector::A) && d.isWhiteLine(e_SrcSelector::B)))) ||
                (m_pldC != nullptr && ((d.isEqualAB() && d.isEqualAC()) || (d.isWhiteLine(e_SrcSelector::A) && d.isWhiteLine(e_SrcSelector::B) && d.isWhiteLine(e_SrcSelector::C))))))
            {
                ml.bWhiteSpaceConflict = true;
            }

            ml.d3lLineIdx = lineIdx;
            ml.bDelta = ml.srcSelect != e_SrcSelector::A;
            ml.id3l = it;
            ml.srcRangeLength = 1;

            MergeLine* back = m_mergeLineList.empty() ? nullptr : &m_mergeLineList.back();

            bool bSame = back != nullptr && sameKindCheck(ml, *back);
            if(bSame)
            {
                ++back->srcRangeLength;
                if(back->bWhiteSpaceConflict && !ml.bWhiteSpaceConflict)
                    back->bWhiteSpaceConflict = false;
            }
            else
            {
                m_mergeLineList.push_back(ml);
            }

            if(!ml.bConflict)
            {
                MergeLine& tmpBack = m_mergeLineList.back();
                MergeEditLine mel(ml.id3l);
                mel.setSource(ml.srcSelect, bLineRemoved);
                tmpBack.mergeEditLineList.push_back(mel);
            }
            else if(back == nullptr || !back->bConflict || !bSame)
            {
                MergeLine& tmpBack = m_mergeLineList.back();
                MergeEditLine mel(ml.id3l);
                mel.setConflict();
                tmpBack.mergeEditLineList.push_back(mel);
            }
        }
    }

    bool bSolveWhiteSpaceConflicts = false;
    if(bAutoSolve) // when true, then the other params are not used and we can change them here. (see all invocations of merge())
    {
        if(m_pldC == nullptr && m_pOptions->m_whiteSpace2FileMergeDefault != (int)e_SrcSelector::None) // Only two inputs
        {
            Q_ASSERT(m_pOptions->m_whiteSpace2FileMergeDefault <= (int)e_SrcSelector::Max && m_pOptions->m_whiteSpace2FileMergeDefault >= (int)e_SrcSelector::Min);
            defaultSelector = (e_SrcSelector)m_pOptions->m_whiteSpace2FileMergeDefault;
            bWhiteSpaceOnly = true;
            bSolveWhiteSpaceConflicts = true;
        }
        else if(m_pldC != nullptr && m_pOptions->m_whiteSpace3FileMergeDefault != (int)e_SrcSelector::None)
        {
            Q_ASSERT(m_pOptions->m_whiteSpace3FileMergeDefault <= (int)e_SrcSelector::Max && m_pOptions->m_whiteSpace2FileMergeDefault >= (int)e_SrcSelector::Min);
            defaultSelector = (e_SrcSelector)m_pOptions->m_whiteSpace3FileMergeDefault;
            bWhiteSpaceOnly = true;
            bSolveWhiteSpaceConflicts = true;
        }
    }

    if(!bAutoSolve || bSolveWhiteSpaceConflicts)
    {
        // Change all auto selections
        MergeLineList::iterator mlIt;
        for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
        {
            MergeLine& ml = *mlIt;
            bool bConflict = ml.mergeEditLineList.empty() || ml.mergeEditLineList.begin()->isConflict();
            if(ml.bDelta && (!bConflictsOnly || bConflict) && (!bWhiteSpaceOnly || ml.bWhiteSpaceConflict))
            {
                ml.mergeEditLineList.clear();
                if(defaultSelector == e_SrcSelector::Invalid && ml.bDelta)
                {
                    MergeEditLine mel(ml.id3l);

                    mel.setConflict();
                    ml.bConflict = true;
                    ml.mergeEditLineList.push_back(mel);
                }
                else
                {
                    Diff3LineList::const_iterator d3llit = ml.id3l;
                    int j;

                    for(j = 0; j < ml.srcRangeLength; ++j)
                    {
                        MergeEditLine mel(d3llit);
                        mel.setSource(defaultSelector, false);

                        LineRef srcLine = defaultSelector == e_SrcSelector::A ? d3llit->getLineA() : defaultSelector == e_SrcSelector::B ? d3llit->getLineB() : defaultSelector == e_SrcSelector::C ? d3llit->getLineC() : LineRef();

                        if(srcLine.isValid())
                        {
                            ml.mergeEditLineList.push_back(mel);
                        }

                        ++d3llit;
                    }

                    if(ml.mergeEditLineList.empty()) // Make a line nevertheless
                    {
                        MergeEditLine mel(ml.id3l);
                        mel.setRemoved(defaultSelector);
                        ml.mergeEditLineList.push_back(mel);
                    }
                }
            }
        }
    }

    MergeLineList::iterator mlIt;
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        // Remove all lines that are empty, because no src lines are there.

        LineRef oldSrcLine;
        e_SrcSelector oldSrc = e_SrcSelector::Invalid;
        MergeEditLineList::iterator melIt;
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end();)
        {
            MergeEditLine& mel = *melIt;
            e_SrcSelector melsrc = mel.src();

            LineRef srcLine = mel.isRemoved() ? LineRef() : melsrc == e_SrcSelector::A ? mel.id3l()->getLineA() : melsrc == e_SrcSelector::B ? mel.id3l()->getLineB() : melsrc == e_SrcSelector::C ? mel.id3l()->getLineC() : LineRef();

            // At least one line remains because oldSrc != melsrc for first line in list
            // Other empty lines will be removed
            if(!srcLine.isValid() && !oldSrcLine.isValid() && oldSrc == melsrc)
                melIt = ml.mergeEditLineList.erase(melIt);
            else
                ++melIt;

            oldSrcLine = srcLine;
            oldSrc = melsrc;
        }
    }

    if(bAutoSolve && !bConflictsOnly)
    {
        if(m_pOptions->m_bRunHistoryAutoMergeOnMergeStart)
            slotMergeHistory();
        if(m_pOptions->m_bRunRegExpAutoMergeOnMergeStart)
            slotRegExpAutoMerge();
        if(m_pldC != nullptr && !doRelevantChangesExist())
            Q_EMIT noRelevantChangesDetected();
    }

    int nrOfSolvedConflicts = 0;
    int nrOfUnsolvedConflicts = 0;
    int nrOfWhiteSpaceConflicts = 0;

    MergeLineList::iterator i;
    for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
    {
        if(i->bConflict)
            ++nrOfUnsolvedConflicts;
        else if(i->bDelta)
            ++nrOfSolvedConflicts;

        if(i->bWhiteSpaceConflict)
            ++nrOfWhiteSpaceConflicts;
    }

    m_pTotalDiffStatus->setUnsolvedConflicts(nrOfUnsolvedConflicts);
    m_pTotalDiffStatus->setSolvedConflicts(nrOfSolvedConflicts);
    m_pTotalDiffStatus->setWhitespaceConflicts(nrOfWhiteSpaceConflicts);

    m_cursorXPos = 0;
    m_cursorOldXPixelPos = 0;
    m_cursorYPos = 0;
    m_maxTextWidth = -1;

    //m_firstLine = 0; // Must not set line/column without scrolling there
    //m_horizScrollOffset = 0;

    setModified(false);

    m_currentMergeLineIt = m_mergeLineList.begin();
    slotGoTop();

    Q_EMIT updateAvailabilities();
    update();
}

void MergeResultWindow::setFirstLine(QtNumberType firstLine)
{
    m_firstLine = std::max(0, firstLine);
    update();
}

void MergeResultWindow::setHorizScrollOffset(int horizScrollOffset)
{
    m_horizScrollOffset = std::max(0, horizScrollOffset);
    update();
}

int MergeResultWindow::getMaxTextWidth()
{
    if(m_maxTextWidth < 0)
    {
        m_maxTextWidth = 0;

        MergeLineList::iterator mlIt = m_mergeLineList.begin();
        for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
        {
            MergeLine& ml = *mlIt;
            MergeEditLineList::iterator melIt;
            for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt)
            {
                MergeEditLine& mel = *melIt;
                QString s = mel.getString(m_pldA, m_pldB, m_pldC);

                QTextLayout textLayout(s, font(), this);
                textLayout.beginLayout();
                textLayout.createLine();
                textLayout.endLayout();
                if(m_maxTextWidth < textLayout.maximumWidth())
                {
                    m_maxTextWidth =  qCeil(textLayout.maximumWidth());
                }
            }
        }
        m_maxTextWidth += 5; // cursorwidth
    }
    return m_maxTextWidth;
}

int MergeResultWindow::getNofLines() const
{
    return m_nofLines;
}

int MergeResultWindow::getVisibleTextAreaWidth()
{
    return width() - getTextXOffset();
}

int MergeResultWindow::getNofVisibleLines()
{
    QFontMetrics fm = fontMetrics();
    return (height() - 3) / fm.lineSpacing() - 2;
}

int MergeResultWindow::getTextXOffset()
{
    QFontMetrics fm = fontMetrics();
    return 3 * Utils::getHorizontalAdvance(fm, '0');
}

void MergeResultWindow::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    Q_EMIT resizeSignal();
}

e_OverviewMode MergeResultWindow::getOverviewMode()
{
    return mOverviewMode;
}

void MergeResultWindow::setOverviewMode(e_OverviewMode eOverviewMode)
{
    mOverviewMode = eOverviewMode;
}

// Check whether we should ignore current delta when moving to next/previous delta
bool MergeResultWindow::checkOverviewIgnore(MergeLineList::iterator& i)
{
    if(mOverviewMode == e_OverviewMode::eOMNormal) return false;
    if(mOverviewMode == e_OverviewMode::eOMAvsB)
        return i->mergeDetails == e_MergeDetails::eCAdded || i->mergeDetails == e_MergeDetails::eCDeleted || i->mergeDetails == e_MergeDetails::eCChanged;
    if(mOverviewMode == e_OverviewMode::eOMAvsC)
        return i->mergeDetails == e_MergeDetails::eBAdded || i->mergeDetails == e_MergeDetails::eBDeleted || i->mergeDetails == e_MergeDetails::eBChanged;
    if(mOverviewMode == e_OverviewMode::eOMBvsC)
        return i->mergeDetails == e_MergeDetails::eBCAddedAndEqual || i->mergeDetails == e_MergeDetails::eBCDeleted || i->mergeDetails == e_MergeDetails::eBCChangedAndEqual;
    return false;
}

// Go to prev/next delta/conflict or first/last delta.
void MergeResultWindow::go(e_Direction eDir, e_EndPoint eEndPoint)
{
    Q_ASSERT(eDir == eUp || eDir == eDown);
    MergeLineList::iterator i = m_currentMergeLineIt;
    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;
    if(eEndPoint == eEnd)
    {
        if(eDir == eUp)
            i = m_mergeLineList.begin(); // first mergeline
        else
            i = --m_mergeLineList.end(); // last mergeline

        while(isItAtEnd(eDir == eUp, i) && !i->bDelta)
        {
            if(eDir == eUp)
                ++i; // search downwards
            else
                --i; // search upwards
        }
    }
    else if(eEndPoint == eDelta && isItAtEnd(eDir != eUp, i))
    {
        do
        {
            if(eDir == eUp)
                --i;
            else
                ++i;
        } while(isItAtEnd(eDir != eUp, i) && (!i->bDelta || checkOverviewIgnore(i) || (bSkipWhiteConflicts && i->bWhiteSpaceConflict)));
    }
    else if(eEndPoint == eConflict && isItAtEnd(eDir != eUp, i))
    {
        do
        {
            if(eDir == eUp)
                --i;
            else
                ++i;
        } while(isItAtEnd(eDir != eUp, i) && (!i->bConflict || (bSkipWhiteConflicts && i->bWhiteSpaceConflict)));
    }
    else if(isItAtEnd(eDir != eUp, i) && eEndPoint == eUnsolvedConflict)
    {
        do
        {
            if(eDir == eUp)
                --i;
            else
                ++i;
        } while(isItAtEnd(eDir != eUp, i) && !i->mergeEditLineList.begin()->isConflict());
    }

    if(isVisible())
        setFocus();

    setFastSelector(i);
}

bool MergeResultWindow::isDeltaAboveCurrent()
{
    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;
    if(m_mergeLineList.empty()) return false;
    MergeLineList::iterator i = m_currentMergeLineIt;
    if(i == m_mergeLineList.begin()) return false;
    do
    {
        --i;
        if(i->bDelta && !checkOverviewIgnore(i) && !(bSkipWhiteConflicts && i->bWhiteSpaceConflict)) return true;
    } while(i != m_mergeLineList.begin());

    return false;
}

bool MergeResultWindow::isDeltaBelowCurrent()
{
    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;
    if(m_mergeLineList.empty()) return false;

    MergeLineList::iterator i = m_currentMergeLineIt;
    if(i != m_mergeLineList.end())
    {
        ++i;
        for(; i != m_mergeLineList.end(); ++i)
        {
            if(i->bDelta && !checkOverviewIgnore(i) && !(bSkipWhiteConflicts && i->bWhiteSpaceConflict)) return true;
        }
    }
    return false;
}

bool MergeResultWindow::isConflictAboveCurrent()
{
    if(m_mergeLineList.empty()) return false;
    MergeLineList::iterator i = m_currentMergeLineIt;
    if(i == m_mergeLineList.begin()) return false;

    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;

    do
    {
        --i;
        if(i->bConflict && !(bSkipWhiteConflicts && i->bWhiteSpaceConflict)) return true;
    } while(i != m_mergeLineList.begin());

    return false;
}

bool MergeResultWindow::isConflictBelowCurrent()
{
    MergeLineList::iterator i = m_currentMergeLineIt;
    if(m_mergeLineList.empty()) return false;

    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;

    if(i != m_mergeLineList.end())
    {
        ++i;
        for(; i != m_mergeLineList.end(); ++i)
        {
            if(i->bConflict && !(bSkipWhiteConflicts && i->bWhiteSpaceConflict)) return true;
        }
    }
    return false;
}

bool MergeResultWindow::isUnsolvedConflictAtCurrent()
{
    if(m_mergeLineList.empty()) return false;
    MergeLineList::iterator i = m_currentMergeLineIt;
    return i->mergeEditLineList.begin()->isConflict();
}

bool MergeResultWindow::isUnsolvedConflictAboveCurrent()
{
    if(m_mergeLineList.empty()) return false;
    MergeLineList::iterator i = m_currentMergeLineIt;
    if(i == m_mergeLineList.begin()) return false;

    do
    {
        --i;
        if(i->mergeEditLineList.begin()->isConflict()) return true;
    } while(i != m_mergeLineList.begin());

    return false;
}

bool MergeResultWindow::isUnsolvedConflictBelowCurrent()
{
    MergeLineList::iterator i = m_currentMergeLineIt;
    if(m_mergeLineList.empty()) return false;

    if(i != m_mergeLineList.end())
    {
        ++i;
        for(; i != m_mergeLineList.end(); ++i)
        {
            if(i->mergeEditLineList.begin()->isConflict()) return true;
        }
    }
    return false;
}

void MergeResultWindow::slotGoTop()
{
    go(eUp, eEnd);
}

void MergeResultWindow::slotGoCurrent()
{
    setFastSelector(m_currentMergeLineIt);
}

void MergeResultWindow::slotGoBottom()
{
    go(eDown, eEnd);
}

void MergeResultWindow::slotGoPrevDelta()
{
    go(eUp, eDelta);
}

void MergeResultWindow::slotGoNextDelta()
{
    go(eDown, eDelta);
}

void MergeResultWindow::slotGoPrevConflict()
{
    go(eUp, eConflict);
}

void MergeResultWindow::slotGoNextConflict()
{
    go(eDown, eConflict);
}

void MergeResultWindow::slotGoPrevUnsolvedConflict()
{
    go(eUp, eUnsolvedConflict);
}

void MergeResultWindow::slotGoNextUnsolvedConflict()
{
    go(eDown, eUnsolvedConflict);
}

/** The line is given as a index in the Diff3LineList.
    The function calculates the corresponding iterator. */
void MergeResultWindow::slotSetFastSelectorLine(LineIndex line)
{
    MergeLineList::iterator i;
    for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
    {
        if(line >= i->d3lLineIdx && line < i->d3lLineIdx + i->srcRangeLength)
        {
            //if ( i->bDelta )
            {
                setFastSelector(i);
            }
            break;
        }
    }
}

int MergeResultWindow::getNrOfUnsolvedConflicts(int* pNrOfWhiteSpaceConflicts)
{
    int nrOfUnsolvedConflicts = 0;
    if(pNrOfWhiteSpaceConflicts != nullptr)
        *pNrOfWhiteSpaceConflicts = 0;

    MergeLineList::iterator mlIt = m_mergeLineList.begin();
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        MergeEditLineList::iterator melIt = ml.mergeEditLineList.begin();
        if(melIt->isConflict())
        {
            ++nrOfUnsolvedConflicts;
            if(ml.bWhiteSpaceConflict && pNrOfWhiteSpaceConflicts != nullptr)
                ++*pNrOfWhiteSpaceConflicts;
        }
    }

    return nrOfUnsolvedConflicts;
}

void MergeResultWindow::showNrOfConflicts()
{
    if(!m_pOptions->m_bShowInfoDialogs)
        return;
    int nrOfConflicts = 0;
    MergeLineList::iterator i;
    for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
    {
        if(i->bConflict || i->bDelta)
            ++nrOfConflicts;
    }
    QString totalInfo;
    if(m_pTotalDiffStatus->isBinaryEqualAB() && m_pTotalDiffStatus->isBinaryEqualAC())
        totalInfo += i18n("All input files are binary equal.");
    else if(m_pTotalDiffStatus->isTextEqualAB() && m_pTotalDiffStatus->isTextEqualAC())
        totalInfo += i18n("All input files contain the same text.");
    else
    {
        if(m_pTotalDiffStatus->isBinaryEqualAB())
            totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("A"), i18n("B"));
        else if(m_pTotalDiffStatus->isTextEqualAB())
            totalInfo += i18n("Files %1 and %2 have equal text.\n", i18n("A"), i18n("B"));
        if(m_pTotalDiffStatus->isBinaryEqualAC())
            totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("A"), i18n("C"));
        else if(m_pTotalDiffStatus->isTextEqualAC())
            totalInfo += i18n("Files %1 and %2 have equal text.\n", i18n("A"), i18n("C"));
        if(m_pTotalDiffStatus->isBinaryEqualBC())
            totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("B"), i18n("C"));
        else if(m_pTotalDiffStatus->isTextEqualBC())
            totalInfo += i18n("Files %1 and %2 have equal text.\n", i18n("B"), i18n("C"));
    }

    int nrOfUnsolvedConflicts = getNrOfUnsolvedConflicts();

    KMessageBox::information(this,
                             i18n("Total number of conflicts: %1\n"
                                  "Nr of automatically solved conflicts: %2\n"
                                  "Nr of unsolved conflicts: %3\n"
                                  "%4",
                                  nrOfConflicts, nrOfConflicts - nrOfUnsolvedConflicts,
                                  nrOfUnsolvedConflicts, totalInfo),
                             i18n("Conflicts"));
}

void MergeResultWindow::setFastSelector(MergeLineList::iterator i)
{
    if(i == m_mergeLineList.end())
        return;
    m_currentMergeLineIt = i;
    Q_EMIT setFastSelectorRange(i->d3lLineIdx, i->srcRangeLength);

    int line1 = 0;

    MergeLineList::iterator mlIt = m_mergeLineList.begin();
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        if(mlIt == m_currentMergeLineIt)
            break;
        line1 += mlIt->mergeEditLineList.size();
    }

    int nofLines = m_currentMergeLineIt->mergeEditLineList.size();
    int newFirstLine = getBestFirstLine(line1, nofLines, m_firstLine, getNofVisibleLines());
    if(newFirstLine != m_firstLine)
    {
        Q_EMIT scrollVertically(newFirstLine - m_firstLine);
    }

    if(m_selection.isEmpty())
    {
        m_cursorXPos = 0;
        m_cursorOldXPixelPos = 0;
        m_cursorYPos = line1;
    }

    update();
    updateSourceMask();
    Q_EMIT updateAvailabilities();
}

void MergeResultWindow::choose(e_SrcSelector selector)
{
    if(m_currentMergeLineIt == m_mergeLineList.end())
        return;

    setModified();

    // First find range for which this change works.
    MergeLine& ml = *m_currentMergeLineIt;

    MergeEditLineList::iterator melIt;

    // Now check if selector is active for this range already.
    bool bActive = false;

    // Remove unneeded lines in the range.
    for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end();)
    {
        MergeEditLine& mel = *melIt;
        if(mel.src() == selector)
            bActive = true;

        if(mel.src() == selector || !mel.isEditableText() || mel.isModified())
            melIt = ml.mergeEditLineList.erase(melIt);
        else
            ++melIt;
    }

    if(!bActive) // Selected source wasn't active.
    {            // Append the lines from selected source here at rangeEnd.
        Diff3LineList::const_iterator d3llit = ml.id3l;
        int j;

        for(j = 0; j < ml.srcRangeLength; ++j)
        {
            MergeEditLine mel(d3llit);
            mel.setSource(selector, false);
            ml.mergeEditLineList.push_back(mel);

            ++d3llit;
        }
    }

    if(!ml.mergeEditLineList.empty())
    {
        // Remove all lines that are empty, because no src lines are there.
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end();)
        {
            MergeEditLine& mel = *melIt;

            LineRef srcLine = mel.src() == e_SrcSelector::A ? mel.id3l()->getLineA() : mel.src() == e_SrcSelector::B ? mel.id3l()->getLineB() : mel.src() == e_SrcSelector::C ? mel.id3l()->getLineC() : LineRef();

            if(!srcLine.isValid())
                melIt = ml.mergeEditLineList.erase(melIt);
            else
                ++melIt;
        }
    }

    if(ml.mergeEditLineList.empty())
    {
        // Insert a dummy line:
        MergeEditLine mel(ml.id3l);

        if(bActive)
            mel.setConflict(); // All src entries deleted => conflict
        else
            mel.setRemoved(selector); // No lines in corresponding src found.

        ml.mergeEditLineList.push_back(mel);
    }

    if(m_cursorYPos >= m_nofLines)
    {
        m_cursorYPos = m_nofLines - 1;
        m_cursorXPos = 0;
    }

    m_maxTextWidth = -1;
    update();
    updateSourceMask();
    Q_EMIT updateAvailabilities();
    showUnsolvedConflictsStatusMessage();
}

// bConflictsOnly: automatically choose for conflicts only (true) or for everywhere (false)
void MergeResultWindow::chooseGlobal(e_SrcSelector selector, bool bConflictsOnly, bool bWhiteSpaceOnly)
{
    resetSelection();

    merge(false, selector, bConflictsOnly, bWhiteSpaceOnly);
    setModified(true);
    update();
    showUnsolvedConflictsStatusMessage();
}

void MergeResultWindow::slotAutoSolve()
{
    resetSelection();
    merge(true, e_SrcSelector::Invalid);
    setModified(true);
    update();
    showUnsolvedConflictsStatusMessage();
    showNrOfConflicts();
}

void MergeResultWindow::slotUnsolve()
{
    resetSelection();
    merge(false, e_SrcSelector::Invalid);
    setModified(true);
    update();
    showUnsolvedConflictsStatusMessage();
}

bool findParenthesesGroups(const QString& s, QStringList& sl)
{
    sl.clear();
    int i = 0;
    std::list<int> startPosStack;
    int length = s.length();
    for(i = 0; i < length; ++i)
    {
        if(s[i] == '\\' && i + 1 < length && (s[i + 1] == '\\' || s[i + 1] == '(' || s[i + 1] == ')'))
        {
            ++i;
            continue;
        }
        if(s[i] == '(')
        {
            startPosStack.push_back(i);
        }
        else if(s[i] == ')')
        {
            if(startPosStack.empty())
                return false; // Parentheses don't match
            int startPos = startPosStack.back();
            startPosStack.pop_back();
            sl.push_back(s.mid(startPos + 1, i - startPos - 1));
        }
    }
    return startPosStack.empty(); // false if parentheses don't match
}

QString calcHistorySortKey(const QString& keyOrder, QRegExp& matchedRegExpr, const QStringList& parenthesesGroupList)
{
    const QStringList keyOrderList = keyOrder.split(',');
    QString key;

    for(const QString& keyIt : keyOrderList)
    {
        if(keyIt.isEmpty())
            continue;
        bool bOk = false;
        int groupIdx = keyIt.toInt(&bOk);
        if(!bOk || groupIdx < 0 || groupIdx > parenthesesGroupList.size())
            continue;
        QString s = matchedRegExpr.cap(groupIdx);
        if(groupIdx == 0)
        {
            key += s + ' ';
            continue;
        }

        QString groupRegExp = parenthesesGroupList[groupIdx - 1];
        if(groupRegExp.indexOf('|') < 0 || groupRegExp.indexOf('(') >= 0)
        {
            bOk = false;
            int i = s.toInt(&bOk);
            if(bOk && i >= 0 && i < 10000)
                s.asprintf("%04d", i); // This should help for correct sorting of numbers.
            key += s + ' ';
        }
        else
        {
            // Assume that the groupRegExp consists of something like "Jan|Feb|Mar|Apr"
            // s is the string that managed to match.
            // Now we want to know at which position it occurred. e.g. Jan=0, Feb=1, Mar=2, etc.
            QStringList sl = groupRegExp.split('|');
            int idx = sl.indexOf(s);
            if(idx < 0)
            {
                // Didn't match
            }
            else
            {
                QString sIdx;
                sIdx.asprintf("%02d", idx + 1); // Up to 99 words in the groupRegExp (more than 12 aren't expected)
                key += sIdx + ' ';
            }
        }
    }
    return key;
}

void MergeResultWindow::collectHistoryInformation(
    e_SrcSelector src, Diff3LineList::const_iterator &iHistoryBegin, Diff3LineList::const_iterator &iHistoryEnd,
    HistoryMap& historyMap,
    std::list<HistoryMap::iterator>& hitList // list of iterators
)
{
    std::list<HistoryMap::iterator>::iterator itHitListFront = hitList.begin();
    Diff3LineList::const_iterator id3l = iHistoryBegin;
    QString historyLead;
    {
        const LineData* pld = id3l->getLineData(src);

        historyLead = Utils::calcHistoryLead(pld->getLine());
    }
    QRegExp historyStart(m_pOptions->m_historyStartRegExp);
    if(id3l == iHistoryEnd)
        return;
    ++id3l; // Skip line with "$Log ... $"
    QRegExp newHistoryEntry(m_pOptions->m_historyEntryStartRegExp);
    QStringList parenthesesGroups;
    findParenthesesGroups(m_pOptions->m_historyEntryStartRegExp, parenthesesGroups);
    QString key;
    MergeEditLineList melList;
    bool bPrevLineIsEmpty = true;
    bool bUseRegExp = !m_pOptions->m_historyEntryStartRegExp.isEmpty();
    for(; id3l != iHistoryEnd; ++id3l)
    {
        const LineData* pld = id3l->getLineData(src);
        if(!pld) continue;

        const QString& oriLine = pld->getLine();
        if(historyLead.isEmpty()) historyLead = Utils::calcHistoryLead(oriLine);
        QString sLine = oriLine.mid(historyLead.length());
        if((!bUseRegExp && !sLine.trimmed().isEmpty() && bPrevLineIsEmpty) || (bUseRegExp && newHistoryEntry.exactMatch(sLine)))
        {
            if(!key.isEmpty() && !melList.empty())
            {
                // Only insert new HistoryMapEntry if key not found; in either case p.first is a valid iterator to element key.
                std::pair<HistoryMap::iterator, bool> p = historyMap.insert(HistoryMap::value_type(key, HistoryMapEntry()));
                HistoryMapEntry& hme = p.first->second;
                if(src == e_SrcSelector::A) hme.mellA = melList;
                if(src == e_SrcSelector::B) hme.mellB = melList;
                if(src == e_SrcSelector::C) hme.mellC = melList;
                if(p.second) // Not in list yet?
                {
                    hitList.insert(itHitListFront, p.first);
                }
            }

            if(!bUseRegExp)
                key = sLine;
            else
                key = calcHistorySortKey(m_pOptions->m_historyEntryStartSortKeyOrder, newHistoryEntry, parenthesesGroups);

            melList.clear();
            melList.push_back(MergeEditLine(id3l, src));
        }
        else if(!historyStart.exactMatch(oriLine))
        {
            melList.push_back(MergeEditLine(id3l, src));
        }

        bPrevLineIsEmpty = sLine.trimmed().isEmpty();
    }
    if(!key.isEmpty())
    {
        // Only insert new HistoryMapEntry if key not found; in either case p.first is a valid iterator to element key.
        std::pair<HistoryMap::iterator, bool> p = historyMap.insert(HistoryMap::value_type(key, HistoryMapEntry()));
        HistoryMapEntry& hme = p.first->second;
        if(src == e_SrcSelector::A) hme.mellA = melList;
        if(src == e_SrcSelector::B) hme.mellB = melList;
        if(src == e_SrcSelector::C) hme.mellC = melList;
        if(p.second) // Not in list yet?
        {
            hitList.insert(itHitListFront, p.first);
        }
    }
    // End of the history
}

MergeEditLineList& MergeResultWindow::HistoryMapEntry::choice(bool bThreeInputs)
{
    if(!bThreeInputs)
        return mellA.empty() ? mellB : mellA;
    else
    {
        if(mellA.empty())
            return mellC.empty() ? mellB : mellC; // A doesn't exist, return one that exists
        else if(!mellB.empty() && !mellC.empty())
        { // A, B and C exist
            return mellA;
        }
        else
            return mellB.empty() ? mellB : mellC; // A exists, return the one that doesn't exist
    }
}

bool MergeResultWindow::HistoryMapEntry::staysInPlace(bool bThreeInputs, Diff3LineList::const_iterator& iHistoryEnd)
{
    // The entry should stay in place if the decision made by the automerger is correct.
    Diff3LineList::const_iterator& iHistoryLast = iHistoryEnd;
    --iHistoryLast;
    if(!bThreeInputs)
    {
        if(!mellA.empty() && !mellB.empty() && mellA.begin()->id3l() == mellB.begin()->id3l() &&
           mellA.back().id3l() == iHistoryLast && mellB.back().id3l() == iHistoryLast)
        {
            iHistoryEnd = mellA.begin()->id3l();
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if(!mellA.empty() && !mellB.empty() && !mellC.empty() && mellA.begin()->id3l() == mellB.begin()->id3l() && mellA.begin()->id3l() == mellC.begin()->id3l() && mellA.back().id3l() == iHistoryLast && mellB.back().id3l() == iHistoryLast && mellC.back().id3l() == iHistoryLast)
        {
            iHistoryEnd = mellA.begin()->id3l();
            return true;
        }
        else
        {
            return false;
        }
    }
}

void MergeResultWindow::slotMergeHistory()
{
    Diff3LineList::const_iterator iD3LHistoryBegin;
    Diff3LineList::const_iterator iD3LHistoryEnd;
    int d3lHistoryBeginLineIdx = -1;
    int d3lHistoryEndLineIdx = -1;

    // Search for history start, history end in the diff3LineList
    m_pDiff3LineList->findHistoryRange(QRegExp(m_pOptions->m_historyStartRegExp), m_pldC != nullptr, iD3LHistoryBegin, iD3LHistoryEnd, d3lHistoryBeginLineIdx, d3lHistoryEndLineIdx);

    if(iD3LHistoryBegin != m_pDiff3LineList->end())
    {
        // Now collect the historyMap information
        HistoryMap historyMap;
        std::list<HistoryMap::iterator> hitList;
        if(m_pldC == nullptr)
        {
            collectHistoryInformation(e_SrcSelector::A, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList);
            collectHistoryInformation(e_SrcSelector::B, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList);
        }
        else
        {
            collectHistoryInformation(e_SrcSelector::A, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList);
            collectHistoryInformation(e_SrcSelector::B, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList);
            collectHistoryInformation(e_SrcSelector::C, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList);
        }

        Diff3LineList::const_iterator iD3LHistoryOrigEnd = iD3LHistoryEnd;

        bool bHistoryMergeSorting = m_pOptions->m_bHistoryMergeSorting && !m_pOptions->m_historyEntryStartSortKeyOrder.isEmpty() &&
                                    !m_pOptions->m_historyEntryStartRegExp.isEmpty();

        if(m_pOptions->m_maxNofHistoryEntries == -1)
        {
            // Remove parts from the historyMap and hitList that stay in place
            if(bHistoryMergeSorting)
            {
                while(!historyMap.empty())
                {
                    HistoryMap::iterator hMapIt = historyMap.begin();
                    if(hMapIt->second.staysInPlace(m_pldC != nullptr, iD3LHistoryEnd))
                        historyMap.erase(hMapIt);
                    else
                        break;
                }
            }
            else
            {
                while(!hitList.empty())
                {
                    HistoryMap::iterator hMapIt = hitList.back();
                    if(hMapIt->second.staysInPlace(m_pldC != nullptr, iD3LHistoryEnd))
                        hitList.pop_back();
                    else
                        break;
                }
            }
            while(iD3LHistoryOrigEnd != iD3LHistoryEnd)
            {
                --iD3LHistoryOrigEnd;
                --d3lHistoryEndLineIdx;
            }
        }

        MergeLineList::iterator iMLLStart = splitAtDiff3LineIdx(d3lHistoryBeginLineIdx);
        MergeLineList::iterator iMLLEnd = splitAtDiff3LineIdx(d3lHistoryEndLineIdx);
        // Now join all MergeLines in the history
        MergeLineList::iterator i = iMLLStart;
        if(i != iMLLEnd)
        {
            ++i;
            while(i != iMLLEnd)
            {
                iMLLStart->join(*i);
                i = m_mergeLineList.erase(i);
            }
        }
        iMLLStart->mergeEditLineList.clear();
        // Now insert the complete history into the first MergeLine of the history
        iMLLStart->mergeEditLineList.push_back(MergeEditLine(iD3LHistoryBegin, m_pldC == nullptr ? e_SrcSelector::B : e_SrcSelector::C));
        QString lead = Utils::calcHistoryLead(iD3LHistoryBegin->getString(e_SrcSelector::A));
        MergeEditLine mel(m_pDiff3LineList->end());
        mel.setString(lead);
        iMLLStart->mergeEditLineList.push_back(mel);

        int historyCount = 0;
        if(bHistoryMergeSorting)
        {
            // Create a sorted history
            HistoryMap::reverse_iterator hmit;
            for(hmit = historyMap.rbegin(); hmit != historyMap.rend(); ++hmit)
            {
                if(historyCount == m_pOptions->m_maxNofHistoryEntries)
                    break;
                ++historyCount;
                HistoryMapEntry& hme = hmit->second;
                MergeEditLineList& mell = hme.choice(m_pldC != nullptr);
                if(!mell.empty())
                    iMLLStart->mergeEditLineList.splice(iMLLStart->mergeEditLineList.end(), mell, mell.begin(), mell.end());
            }
        }
        else
        {
            // Create history in order of appearance
            std::list<HistoryMap::iterator>::iterator hlit;
            for(hlit = hitList.begin(); hlit != hitList.end(); ++hlit)
            {
                if(historyCount == m_pOptions->m_maxNofHistoryEntries)
                    break;
                ++historyCount;
                HistoryMapEntry& hme = (*hlit)->second;
                MergeEditLineList& mell = hme.choice(m_pldC != nullptr);
                if(!mell.empty())
                    iMLLStart->mergeEditLineList.splice(iMLLStart->mergeEditLineList.end(), mell, mell.begin(), mell.end());
            }
            // If the end of start is empty and the first line at the end is empty remove the last line of start
            if(!iMLLStart->mergeEditLineList.empty() && !iMLLEnd->mergeEditLineList.empty())
            {
                QString lastLineOfStart = iMLLStart->mergeEditLineList.back().getString(m_pldA, m_pldB, m_pldC);
                QString firstLineOfEnd = iMLLEnd->mergeEditLineList.front().getString(m_pldA, m_pldB, m_pldC);
                if(lastLineOfStart.mid(lead.length()).trimmed().isEmpty() && firstLineOfEnd.mid(lead.length()).trimmed().isEmpty())
                    iMLLStart->mergeEditLineList.pop_back();
            }
        }
        setFastSelector(iMLLStart);
        update();
    }
}

void MergeResultWindow::slotRegExpAutoMerge()
{
    if(m_pOptions->m_autoMergeRegExp.isEmpty())
        return;

    QRegExp vcsKeywords(m_pOptions->m_autoMergeRegExp);
    MergeLineList::iterator i;
    for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
    {
        if(i->bConflict)
        {
            Diff3LineList::const_iterator id3l = i->id3l;
            if(vcsKeywords.exactMatch(id3l->getString(e_SrcSelector::A)) &&
               vcsKeywords.exactMatch(id3l->getString(e_SrcSelector::B)) &&
               (m_pldC == nullptr || vcsKeywords.exactMatch(id3l->getString(e_SrcSelector::C))))
            {
                MergeEditLine& mel = *i->mergeEditLineList.begin();
                mel.setSource(m_pldC == nullptr ? e_SrcSelector::B : e_SrcSelector::C, false);
                splitAtDiff3LineIdx(i->d3lLineIdx + 1);
            }
        }
    }
    update();
}

// This doesn't detect user modifications and should only be called after automatic merge
// This will only do something for three file merge.
// Irrelevant changes are those where all contributions from B are already contained in C.
// Also irrelevant are conflicts automatically solved (automerge regexp and history automerge)
// Precondition: The VCS-keyword would also be C.
bool MergeResultWindow::doRelevantChangesExist()
{
    if(m_pldC == nullptr || m_mergeLineList.size() <= 1)
        return true;

    MergeLineList::iterator i;
    for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
    {
        if((i->bConflict && i->mergeEditLineList.begin()->src() != e_SrcSelector::C) || i->srcSelect == e_SrcSelector::B)
        {
            return true;
        }
    }

    return false;
}

// Returns the iterator to the MergeLine after the split
MergeLineList::iterator MergeResultWindow::splitAtDiff3LineIdx(int d3lLineIdx)
{
    MergeLineList::iterator i;
    for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
    {
        if(i->d3lLineIdx == d3lLineIdx)
        {
            // No split needed, this is the beginning of a MergeLine
            return i;
        }
        else if(i->d3lLineIdx > d3lLineIdx)
        {
            // The split must be in the previous MergeLine
            --i;
            MergeLine& ml = *i;
            MergeLine newML;
            ml.split(newML, d3lLineIdx);
            ++i;
            return m_mergeLineList.insert(i, newML);
        }
    }
    // The split must be in the previous MergeLine
    --i;
    MergeLine& ml = *i;
    MergeLine newML;
    ml.split(newML, d3lLineIdx);
    ++i;
    return m_mergeLineList.insert(i, newML);
}

void MergeResultWindow::slotSplitDiff(int firstD3lLineIdx, int lastD3lLineIdx)
{
    if(lastD3lLineIdx >= 0)
        splitAtDiff3LineIdx(lastD3lLineIdx + 1);
    setFastSelector(splitAtDiff3LineIdx(firstD3lLineIdx));
}

void MergeResultWindow::slotJoinDiffs(int firstD3lLineIdx, int lastD3lLineIdx)
{
    MergeLineList::iterator i;
    MergeLineList::iterator iMLLStart = m_mergeLineList.end();
    MergeLineList::iterator iMLLEnd = m_mergeLineList.end();
    for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
    {
        MergeLine& ml = *i;
        if(firstD3lLineIdx >= ml.d3lLineIdx && firstD3lLineIdx < ml.d3lLineIdx + ml.srcRangeLength)
        {
            iMLLStart = i;
        }
        if(lastD3lLineIdx >= ml.d3lLineIdx && lastD3lLineIdx < ml.d3lLineIdx + ml.srcRangeLength)
        {
            iMLLEnd = i;
            ++iMLLEnd;
            break;
        }
    }

    bool bJoined = false;
    for(i = iMLLStart; i != iMLLEnd && i != m_mergeLineList.end();)
    {
        if(i == iMLLStart)
        {
            ++i;
        }
        else
        {
            iMLLStart->join(*i);
            i = m_mergeLineList.erase(i);
            bJoined = true;
        }
    }
    if(bJoined)
    {
        iMLLStart->mergeEditLineList.clear();
        // Insert a conflict line as placeholder
        iMLLStart->mergeEditLineList.push_back(MergeEditLine(iMLLStart->id3l));
    }
    setFastSelector(iMLLStart);
}

void MergeResultWindow::myUpdate(int afterMilliSecs)
{
    if(m_delayedDrawTimer)
        killTimer(m_delayedDrawTimer);
    m_bMyUpdate = true;
    m_delayedDrawTimer = startTimer(afterMilliSecs);
}

void MergeResultWindow::timerEvent(QTimerEvent*)
{
    killTimer(m_delayedDrawTimer);
    m_delayedDrawTimer = 0;

    if(m_bMyUpdate)
    {
        update();
        m_bMyUpdate = false;
    }

    if(m_scrollDeltaX != 0 || m_scrollDeltaY != 0)
    {
        m_selection.end(m_selection.getLastLine() + m_scrollDeltaY, m_selection.getLastPos() + m_scrollDeltaX);
        Q_EMIT scrollMergeResultWindow(m_scrollDeltaX, m_scrollDeltaY);
        killTimer(m_delayedDrawTimer);
        m_delayedDrawTimer = startTimer(50);
    }
}

QVector<QTextLayout::FormatRange> MergeResultWindow::getTextLayoutForLine(int line, const QString& str, QTextLayout& textLayout)
{
    // tabs
    QTextOption textOption;
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    textOption.setTabStop(QFontMetricsF(font()).width(' ') * m_pOptions->m_tabSize);
#else
    textOption.setTabStopDistance(QFontMetricsF(font()).width(' ') * m_pOptions->m_tabSize);
#endif
    if(m_pOptions->m_bShowWhiteSpaceCharacters)
    {
        textOption.setFlags(QTextOption::ShowTabsAndSpaces);
    }
    textLayout.setTextOption(textOption);

    if(m_pOptions->m_bShowWhiteSpaceCharacters)
    {
        // This additional format is only necessary for the tab arrow
        QVector<QTextLayout::FormatRange> formats;
        QTextLayout::FormatRange formatRange;
        formatRange.start = 0;
        formatRange.length = str.length();
        formatRange.format.setFont(font());
        formats.append(formatRange);
        textLayout.setFormats(formats);
    }
    QVector<QTextLayout::FormatRange> selectionFormat;
    textLayout.beginLayout();
    if(m_selection.lineWithin(line))
    {
        int firstPosInText = m_selection.firstPosInLine(line);
        int lastPosInText = m_selection.lastPosInLine(line);
        int lengthInText = std::max(0, lastPosInText - firstPosInText);
        if(lengthInText > 0)
            m_selection.bSelectionContainsData = true;

        QTextLayout::FormatRange selection;
        selection.start = firstPosInText;
        selection.length = lengthInText;
        selection.format.setBackground(palette().highlight());
        selection.format.setForeground(palette().highlightedText().color());
        selectionFormat.push_back(selection);
    }
    QTextLine textLine = textLayout.createLine();
    textLine.setPosition(QPointF(0, fontMetrics().leading()));
    textLayout.endLayout();
    int cursorWidth = 5;
    if(m_pOptions->m_bRightToLeftLanguage)
        textLayout.setPosition(QPointF(width() - textLayout.maximumWidth() - getTextXOffset() + m_horizScrollOffset - cursorWidth, 0));
    else
        textLayout.setPosition(QPointF(getTextXOffset() - m_horizScrollOffset, 0));
    return selectionFormat;
}

void MergeResultWindow::writeLine(
    RLPainter& p, int line, const QString& str,
    e_SrcSelector srcSelect, e_MergeDetails mergeDetails, int rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict)
{
    const QFontMetrics& fm = fontMetrics();
    int fontHeight = fm.lineSpacing();
    int fontAscent = fm.ascent();

    int topLineYOffset = 0;
    int xOffset = getTextXOffset();

    int yOffset = (line - m_firstLine) * fontHeight;
    if(yOffset < 0 || yOffset > height())
        return;

    yOffset += topLineYOffset;

    QString srcName = QChar(' ');
    if(bUserModified)
        srcName = QChar('m');
    else if(srcSelect == e_SrcSelector::A && mergeDetails != e_MergeDetails::eNoChange)
        srcName = i18n("A");
    else if(srcSelect == e_SrcSelector::B)
        srcName = i18n("B");
    else if(srcSelect == e_SrcSelector::C)
        srcName = i18n("C");

    if(rangeMark & 4)
    {
        p.fillRect(xOffset, yOffset, width(), fontHeight, m_pOptions->m_currentRangeBgColor);
    }

    if((srcSelect > e_SrcSelector::None || bUserModified) && !bLineRemoved)
    {
        if(!m_pOptions->m_bRightToLeftLanguage)
            p.setClipRect(QRectF(xOffset, 0, width() - xOffset, height()));
        else
            p.setClipRect(QRectF(0, 0, width() - xOffset, height()));

        int outPos = 0;
        QString s;
        int size = str.length();
        for(int i = 0; i < size; ++i)
        {
            int spaces = 1;
            if(str[i] == '\t')
            {
                spaces = tabber(outPos, m_pOptions->m_tabSize);
                for(int j = 0; j < spaces; ++j)
                    s += ' ';
            }
            else
            {
                s += str[i];
            }
            outPos += spaces;
        }

        p.setPen(m_pOptions->m_fgColor);

        QTextLayout textLayout(str, font(), this);
        QVector<QTextLayout::FormatRange> selectionFormat = getTextLayoutForLine(line, str, textLayout);
        textLayout.draw(&p, QPointF(0, yOffset), selectionFormat);

        if(line == m_cursorYPos)
        {
            m_cursorXPixelPos =  qCeil(textLayout.lineAt(0).cursorToX(m_cursorXPos));
            if(m_pOptions->m_bRightToLeftLanguage)
                m_cursorXPixelPos +=  qCeil(textLayout.position().x() - m_horizScrollOffset);
        }

        p.setClipping(false);

        p.setPen(m_pOptions->m_fgColor);

        p.drawText(1, yOffset + fontAscent, srcName, true);
    }
    else if(bLineRemoved)
    {
        p.setPen(m_pOptions->m_colorForConflict);
        p.drawText(xOffset, yOffset + fontAscent, i18n("<No src line>"));
        p.drawText(1, yOffset + fontAscent, srcName);
        if(m_cursorYPos == line) m_cursorXPos = 0;
    }
    else if(srcSelect == e_SrcSelector::None)
    {
        p.setPen(m_pOptions->m_colorForConflict);
        if(bWhiteSpaceConflict)
            p.drawText(xOffset, yOffset + fontAscent, i18n("<Merge Conflict (Whitespace only)>"));
        else
            p.drawText(xOffset, yOffset + fontAscent, i18n("<Merge Conflict>"));
        p.drawText(1, yOffset + fontAscent, "?");
        if(m_cursorYPos == line) m_cursorXPos = 0;
    }
    else
        Q_ASSERT(true);

    xOffset -= Utils::getHorizontalAdvance(fm, '0');
    p.setPen(m_pOptions->m_fgColor);
    if(rangeMark & 1) // begin mark
    {
        p.drawLine(xOffset, yOffset + 1, xOffset, yOffset + fontHeight / 2);
        p.drawLine(xOffset, yOffset + 1, xOffset - 2, yOffset + 1);
    }
    else
    {
        p.drawLine(xOffset, yOffset, xOffset, yOffset + fontHeight / 2);
    }

    if(rangeMark & 2) // end mark
    {
        p.drawLine(xOffset, yOffset + fontHeight / 2, xOffset, yOffset + fontHeight - 1);
        p.drawLine(xOffset, yOffset + fontHeight - 1, xOffset - 2, yOffset + fontHeight - 1);
    }
    else
    {
        p.drawLine(xOffset, yOffset + fontHeight / 2, xOffset, yOffset + fontHeight);
    }

    if(rangeMark & 4)
    {
        p.fillRect(xOffset + 3, yOffset, 3, fontHeight, m_pOptions->m_fgColor);
        /*      p.setPen( blue );
      p.drawLine( xOffset+2, yOffset, xOffset+2, yOffset+fontHeight-1 );
      p.drawLine( xOffset+3, yOffset, xOffset+3, yOffset+fontHeight-1 );*/
    }
}

void MergeResultWindow::setPaintingAllowed(bool bPaintingAllowed)
{
    setUpdatesEnabled(bPaintingAllowed);
    if(!bPaintingAllowed)
    {
        m_currentMergeLineIt = m_mergeLineList.end();
        reset();
    }
    else
        update();
}

void MergeResultWindow::paintEvent(QPaintEvent*)
{
    if(m_pDiff3LineList == nullptr)
        return;

    bool bOldSelectionContainsData = m_selection.selectionContainsData();
    const QFontMetrics& fm = fontMetrics();
    int fontWidth = Utils::getHorizontalAdvance(fm, '0');

    if(!m_bCursorUpdate) // Don't redraw everything for blinking cursor?
    {
        m_selection.bSelectionContainsData = false;
        const auto dpr = devicePixelRatioF();
        if(size() * dpr != m_pixmap.size()) {
            m_pixmap = QPixmap(size() * dpr);
            m_pixmap.setDevicePixelRatio(dpr);
        }

        RLPainter p(&m_pixmap, m_pOptions->m_bRightToLeftLanguage, width(), fontWidth);
        p.setFont(font());
        p.QPainter::fillRect(rect(), m_pOptions->m_bgColor);

        //int visibleLines = height() / fontHeight;

        int lastVisibleLine = m_firstLine + getNofVisibleLines() + 5;
        LineRef line = 0;
        MergeLineList::iterator mlIt = m_mergeLineList.begin();
        for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
        {
            MergeLine& ml = *mlIt;
            if(line > lastVisibleLine || line + ml.mergeEditLineList.size() < m_firstLine)
            {
                line += ml.mergeEditLineList.size();
            }
            else
            {
                MergeEditLineList::iterator melIt;
                for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt)
                {
                    if(line >= m_firstLine && line <= lastVisibleLine)
                    {
                        MergeEditLine& mel = *melIt;
                        MergeEditLineList::iterator melIt1 = melIt;
                        ++melIt1;

                        int rangeMark = 0;
                        if(melIt == ml.mergeEditLineList.begin()) rangeMark |= 1; // Begin range mark
                        if(melIt1 == ml.mergeEditLineList.end()) rangeMark |= 2;  // End range mark

                        if(mlIt == m_currentMergeLineIt) rangeMark |= 4; // Mark of the current line

                        QString s;
                        s = mel.getString(m_pldA, m_pldB, m_pldC);

                        writeLine(p, line, s, mel.src(), ml.mergeDetails, rangeMark,
                                  mel.isModified(), mel.isRemoved(), ml.bWhiteSpaceConflict);
                    }
                    ++line;
                }
            }
        }

        if(line != m_nofLines)
        {
            m_nofLines = line;

            Q_EMIT resizeSignal();
        }

        p.end();
    }

    QPainter painter(this);

    if(!m_bCursorUpdate)
        painter.drawPixmap(0, 0, m_pixmap);
    else
    {
        painter.drawPixmap(0, 0, m_pixmap); // Draw everything. (Internally cursor rect is clipped anyway.)
        m_bCursorUpdate = false;
    }

    if(m_bCursorOn && hasFocus() && m_cursorYPos >= m_firstLine)
    {
        painter.setPen(m_pOptions->m_fgColor);

        QString str = getString(m_cursorYPos);
        QTextLayout textLayout(str, font(), this);
        getTextLayoutForLine(m_cursorYPos, str, textLayout);
        textLayout.drawCursor(&painter, QPointF(0, (m_cursorYPos - m_firstLine) * fontMetrics().lineSpacing()), m_cursorXPos);
    }

    painter.end();

    if(!bOldSelectionContainsData && m_selection.selectionContainsData())
        Q_EMIT newSelection();
}

void MergeResultWindow::updateSourceMask()
{
    int srcMask = 0;
    int enabledMask = 0;
    if(!hasFocus() || m_pDiff3LineList == nullptr || !updatesEnabled() || m_currentMergeLineIt == m_mergeLineList.end())
    {
        srcMask = 0;
        enabledMask = 0;
    }
    else
    {
        enabledMask = m_pldC == nullptr ? 3 : 7;
        MergeLine& ml = *m_currentMergeLineIt;

        srcMask = 0;
        bool bModified = false;
        MergeEditLineList::iterator melIt;
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt)
        {
            MergeEditLine& mel = *melIt;
            if(mel.src() == e_SrcSelector::A) srcMask |= 1;
            if(mel.src() == e_SrcSelector::B) srcMask |= 2;
            if(mel.src() == e_SrcSelector::C) srcMask |= 4;
            if(mel.isModified() || !mel.isEditableText()) bModified = true;
        }

        if(ml.mergeDetails == e_MergeDetails::eNoChange)
        {
            srcMask = 0;
            enabledMask = bModified ? 1 : 0;
        }
    }

    Q_EMIT sourceMask(srcMask, enabledMask);
}

void MergeResultWindow::focusInEvent(QFocusEvent* e)
{
    updateSourceMask();
    QWidget::focusInEvent(e);
}

LineRef MergeResultWindow::convertToLine(int y)
{
    const QFontMetrics& fm = fontMetrics();
    int fontHeight = fm.lineSpacing();
    int topLineYOffset = 0;

    int yOffset = topLineYOffset - m_firstLine * fontHeight;

    LineRef line = std::min((y - yOffset) / fontHeight, m_nofLines - 1);
    return line;
}

void MergeResultWindow::mousePressEvent(QMouseEvent* e)
{
    m_bCursorOn = true;

    int xOffset = getTextXOffset();

    LineRef line = convertToLine(e->y());
    QString s = getString(line);
    QTextLayout textLayout(s, font(), this);
    getTextLayoutForLine(line, s, textLayout);
    QtNumberType pos = textLayout.lineAt(0).xToCursor(e->x() - textLayout.position().x());

    bool bLMB = e->button() == Qt::LeftButton;
    bool bMMB = e->button() == Qt::MidButton;
    bool bRMB = e->button() == Qt::RightButton;

    if((bLMB && (e->x() < xOffset)) || bRMB) // Fast range selection
    {
        m_cursorXPos = 0;
        m_cursorOldXPixelPos = 0;
        m_cursorYPos = std::max((LineRef::LineType)line, 0);
        int l = 0;
        MergeLineList::iterator i = m_mergeLineList.begin();
        for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
        {
            if(l == line)
                break;

            l += i->mergeEditLineList.size();
            if(l > line)
                break;
        }
        m_selection.reset(); // Disable current selection

        m_bCursorOn = true;
        setFastSelector(i);

        if(bRMB)
        {
            Q_EMIT showPopupMenu(QCursor::pos());
        }
    }
    else if(bLMB) // Normal cursor placement
    {
        pos = std::max(pos, 0);
        line = std::max((LineRef::LineType)line, 0);
        if(e->QInputEvent::modifiers() & Qt::ShiftModifier)
        {
            if(!m_selection.isValidFirstLine())
                m_selection.start(line, pos);
            m_selection.end(line, pos);
        }
        else
        {
            // Selection
            m_selection.reset();
            m_selection.start(line, pos);
            m_selection.end(line, pos);
        }
        m_cursorXPos = pos;
        m_cursorXPixelPos =  qCeil(textLayout.lineAt(0).cursorToX(pos));
        if(m_pOptions->m_bRightToLeftLanguage)
            m_cursorXPixelPos +=  qCeil(textLayout.position().x() - m_horizScrollOffset);
        m_cursorOldXPixelPos = m_cursorXPixelPos;
        m_cursorYPos = line;

        update();
        //showStatusLine( line, m_winIdx, m_pFilename, m_pDiff3LineList, m_pStatusBar );
    }
    else if(bMMB) // Paste clipboard
    {
        pos = std::max(pos, 0);
        line = std::max((LineRef::LineType)line, 0);

        m_selection.reset();
        m_cursorXPos = pos;
        m_cursorOldXPixelPos = m_cursorXPixelPos;
        m_cursorYPos = line;

        pasteClipboard(true);
    }
}

void MergeResultWindow::mouseDoubleClickEvent(QMouseEvent* e)
{
    if(e->button() == Qt::LeftButton)
    {
        LineRef line = convertToLine(e->y());
        QString s = getString(line);
        QTextLayout textLayout(s, font(), this);
        getTextLayoutForLine(line, s, textLayout);
        int pos = textLayout.lineAt(0).xToCursor(e->x() - textLayout.position().x());
        m_cursorXPos = pos;
        m_cursorOldXPixelPos = m_cursorXPixelPos;
        m_cursorYPos = line;

        if(!s.isEmpty())
        {
            int pos1, pos2;

            Utils::calcTokenPos(s, pos, pos1, pos2);

            resetSelection();
            m_selection.start(line, pos1);
            m_selection.end(line, pos2);

            update();
            // Q_EMIT selectionEnd() happens in the mouseReleaseEvent.
        }
    }
}

void MergeResultWindow::mouseReleaseEvent(QMouseEvent* e)
{
    if(e->button() == Qt::LeftButton)
    {
        if(m_delayedDrawTimer)
        {
            killTimer(m_delayedDrawTimer);
            m_delayedDrawTimer = 0;
        }

        if(m_selection.isValidFirstLine())
        {
            Q_EMIT selectionEnd();
        }

        Q_EMIT updateAvailabilities();
    }
}

void MergeResultWindow::mouseMoveEvent(QMouseEvent* e)
{
    LineRef line = convertToLine(e->y());
    QString s = getString(line);
    QTextLayout textLayout(s, font(), this);
    getTextLayoutForLine(line, s, textLayout);
    int pos = textLayout.lineAt(0).xToCursor(e->x() - textLayout.position().x());
    m_cursorXPos = pos;
    m_cursorOldXPixelPos = m_cursorXPixelPos;
    m_cursorYPos = line;
    if(m_selection.isValidFirstLine())
    {
        m_selection.end(line, pos);
        myUpdate(0);

        //showStatusLine( line, m_winIdx, m_pFilename, m_pDiff3LineList, m_pStatusBar );

        // Scroll because mouse moved out of the window
        const QFontMetrics& fm = fontMetrics();
        int fontWidth = Utils::getHorizontalAdvance(fm, '0');
        int topLineYOffset = 0;
        int deltaX = 0;
        int deltaY = 0;
        if(!m_pOptions->m_bRightToLeftLanguage)
        {
            if(e->x() < getTextXOffset()) deltaX = -1;
            if(e->x() > width()) deltaX = +1;
        }
        else
        {
            if(e->x() > width() - 1 - getTextXOffset()) deltaX = -1;
            if(e->x() < fontWidth) deltaX = +1;
        }
        if(e->y() < topLineYOffset) deltaY = -1;
        if(e->y() > height()) deltaY = +1;
        m_scrollDeltaX = deltaX;
        m_scrollDeltaY = deltaY;
        if(deltaX != 0 || deltaY != 0)
        {
            Q_EMIT scrollMergeResultWindow(deltaX, deltaY);
        }
    }
}

void MergeResultWindow::slotCursorUpdate()
{
    m_cursorTimer.stop();
    m_bCursorOn = !m_bCursorOn;

    if(isVisible())
    {
        m_bCursorUpdate = true;

        const QFontMetrics& fm = fontMetrics();
        int topLineYOffset = 0;
        int yOffset = (m_cursorYPos - m_firstLine) * fm.lineSpacing() + topLineYOffset;

        repaint(0, yOffset, width(), fm.lineSpacing() + 2);

        m_bCursorUpdate = false;
    }

    m_cursorTimer.start(500);
}


void MergeResultWindow::wheelEvent(QWheelEvent* pWheelEvent)
{
    QPoint delta = pWheelEvent->angleDelta();
    //Block diagonal scrolling easily generated unintentionally with track pads.
    if(delta.y() != 0 && abs(delta.y()) > abs(delta.x()) && mVScrollBar != nullptr)
    {
        pWheelEvent->accept();
        QCoreApplication::postEvent(mVScrollBar, new QWheelEvent(*pWheelEvent));
    }
}

bool MergeResultWindow::event(QEvent* e)
{
    if(e->type() == QEvent::KeyPress)
    {
        QKeyEvent* ke = static_cast<QKeyEvent*>(e);
        if(ke->key() == Qt::Key_Tab)
        {
            // special tab handling here to avoid moving focus
            keyPressEvent(ke);
            return true;
        }
    }
    return QWidget::event(e);
}
void MergeResultWindow::keyPressEvent(QKeyEvent* e)
{
    int y = m_cursorYPos;
    MergeLineList::iterator mlIt;
    MergeEditLineList::iterator melIt;
    calcIteratorFromLineNr(y, mlIt, melIt);

    QString str = melIt->getString(m_pldA, m_pldB, m_pldC);
    int x = m_cursorXPos;

    QTextLayout textLayoutOrig(str, font(), this);
    getTextLayoutForLine(y, str, textLayoutOrig);

    bool bCtrl = (e->QInputEvent::modifiers() & Qt::ControlModifier) != 0;
    bool bShift = (e->QInputEvent::modifiers() & Qt::ShiftModifier) != 0;
#ifdef Q_OS_WIN
    bool bAlt = (e->QInputEvent::modifiers() & Qt::AltModifier) != 0;
    if(bCtrl && bAlt)
    {
        bCtrl = false;
        bAlt = false;
    } // AltGr-Key pressed.
#endif

    bool bYMoveKey = false;
    // Special keys
    switch(e->key())
    {
        case Qt::Key_Escape:
        //case Qt::Key_Tab:
        case Qt::Key_Backtab:
            break;
        case Qt::Key_Delete: {
            if(deleteSelection2(str, x, y, mlIt, melIt) || !melIt->isEditableText()) break;

            if(x >= str.length())
            {
                if(y < m_nofLines - 1)
                {
                    setModified();
                    MergeLineList::iterator mlIt1;
                    MergeEditLineList::iterator melIt1;
                    calcIteratorFromLineNr(y + 1, mlIt1, melIt1);
                    if(melIt1->isEditableText())
                    {
                        QString s2 = melIt1->getString(m_pldA, m_pldB, m_pldC);
                        melIt->setString(str + s2);

                        // Remove the line
                        if(mlIt1->mergeEditLineList.size() > 1)
                            mlIt1->mergeEditLineList.erase(melIt1);
                        else
                            melIt1->setRemoved();
                    }
                }
            }
            else
            {
                QString s = str.left(x);
                s += str.midRef(x + 1);
                melIt->setString(s);
                setModified();
            }
            break;
        }
        case Qt::Key_Backspace: {
            if(deleteSelection2(str, x, y, mlIt, melIt)) break;
            if(!melIt->isEditableText()) break;
            if(x == 0)
            {
                if(y > 0)
                {
                    setModified();
                    MergeLineList::iterator mlIt1;
                    MergeEditLineList::iterator melIt1;
                    calcIteratorFromLineNr(y - 1, mlIt1, melIt1);
                    if(melIt1->isEditableText())
                    {
                        QString s1 = melIt1->getString(m_pldA, m_pldB, m_pldC);
                        melIt1->setString(s1 + str);

                        // Remove the previous line
                        if(mlIt->mergeEditLineList.size() > 1)
                            mlIt->mergeEditLineList.erase(melIt);
                        else
                            melIt->setRemoved();

                        --y;
                        x = str.length();
                    }
                }
            }
            else
            {
                QString s = str.left(x - 1);
                s += str.midRef(x);
                --x;
                melIt->setString(s);
                setModified();
            }
            break;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            if(!melIt->isEditableText()) break;
            deleteSelection2(str, x, y, mlIt, melIt);
            setModified();
            QString indentation;
            if(m_pOptions->m_bAutoIndentation)
            { // calc last indentation
                MergeLineList::iterator mlIt1 = mlIt;
                MergeEditLineList::iterator melIt1 = melIt;
                for(;;)
                {
                    const QString s = melIt1->getString(m_pldA, m_pldB, m_pldC);
                    if(!s.isEmpty())
                    {
                        int i;
                        for(i = 0; i < s.length(); ++i)
                        {
                            if(s[i] != ' ' && s[i] != '\t') break;
                        }
                        if(i < s.length())
                        {
                            indentation = s.left(i);
                            break;
                        }
                    }
                    // Go back one line
                    if(melIt1 != mlIt1->mergeEditLineList.begin())
                        --melIt1;
                    else
                    {
                        if(mlIt1 == m_mergeLineList.begin()) break;
                        --mlIt1;
                        melIt1 = mlIt1->mergeEditLineList.end();
                        --melIt1;
                    }
                }
            }
            MergeEditLine mel(mlIt->id3l); // Associate every mel with an id3l, even if not really valid.
            mel.setString(indentation + str.mid(x));

            if(x < str.length()) // Cut off the old line.
            {
                // Since ps possibly points into melIt->str, first copy it into a temporary.
                QString temp = str.left(x);
                melIt->setString(temp);
            }

            ++melIt;
            mlIt->mergeEditLineList.insert(melIt, mel);
            x = indentation.length();
            ++y;
            break;
        }
        case Qt::Key_Insert:
            m_bInsertMode = !m_bInsertMode;
            break;
        case Qt::Key_Pause:
        case Qt::Key_Print:
        case Qt::Key_SysReq:
            break;
        case Qt::Key_Home:
            x = 0;
            if(bCtrl)
            {
                y = 0;
            }
            break; // cursor movement
        case Qt::Key_End:
            x = TYPE_MAX(int);
            if(bCtrl)
            {
                y = TYPE_MAX(int);
            }
            break;

        case Qt::Key_Left:
        case Qt::Key_Right:
            if((e->key() == Qt::Key_Left) != m_pOptions->m_bRightToLeftLanguage)
            {
                if(!bCtrl)
                {
                    int newX = textLayoutOrig.previousCursorPosition(x);
                    if(newX == x && y > 0)
                    {
                        --y;
                        x = TYPE_MAX(int);
                    }
                    else
                    {
                        x = newX;
                    }
                }
                else
                {
                    while(x > 0 && (str[x - 1] == ' ' || str[x - 1] == '\t'))
                    {
                        int newX = textLayoutOrig.previousCursorPosition(x);
                        if(newX == x) break;
                        x = newX;
                    }
                    while(x > 0 && (str[x - 1] != ' ' && str[x - 1] != '\t'))
                    {
                        int newX = textLayoutOrig.previousCursorPosition(x);
                        if(newX == x) break;
                        x = newX;
                    }
                }
            }
            else
            {
                if(!bCtrl)
                {
                    int newX = textLayoutOrig.nextCursorPosition(x);
                    if(newX == x && y < m_nofLines - 1)
                    {
                        ++y;
                        x = 0;
                    }
                    else
                    {
                        x = newX;
                    }
                }
                else
                {
                    while(x < str.length() && (str[x] == ' ' || str[x] == '\t'))
                    {
                        int newX = textLayoutOrig.nextCursorPosition(x);
                        if(newX == x) break;
                        x = newX;
                    }
                    while(x < str.length() && (str[x] != ' ' && str[x] != '\t'))
                    {
                        int newX = textLayoutOrig.nextCursorPosition(x);
                        if(newX == x) break;
                        x = newX;
                    }
                }
            }
            break;

        case Qt::Key_Up:
            if(!bCtrl)
            {
                --y;
                bYMoveKey = true;
            }
            break;
        case Qt::Key_Down:
            if(!bCtrl)
            {
                ++y;
                bYMoveKey = true;
            }
            break;
        case Qt::Key_PageUp:
            if(!bCtrl)
            {
                y -= getNofVisibleLines();
                bYMoveKey = true;
            }
            break;
        case Qt::Key_PageDown:
            if(!bCtrl)
            {
                y += getNofVisibleLines();
                bYMoveKey = true;
            }
            break;
        default: {
            QString t = e->text();
            if(t.isEmpty() || bCtrl)
            {
                e->ignore();
                return;
            }
            else
            {
                if(bCtrl)
                {
                    e->ignore();
                    return;
                }
                else
                {
                    if(!melIt->isEditableText()) break;
                    deleteSelection2(str, x, y, mlIt, melIt);

                    setModified();
                    // Characters to insert
                    QString s = str;
                    if(t[0] == '\t' && m_pOptions->m_bReplaceTabs)
                    {
                        int spaces = (m_cursorXPos / m_pOptions->m_tabSize + 1) * m_pOptions->m_tabSize - m_cursorXPos;
                        t.fill(' ', spaces);
                    }
                    if(m_bInsertMode)
                        s.insert(x, t);
                    else
                        s.replace(x, t.length(), t);

                    melIt->setString(s);
                    x += t.length();
                    bShift = false;
                }
            }
        }
    }

    y = qBound(0, y, m_nofLines - 1);

    calcIteratorFromLineNr(y, mlIt, melIt);
    str = melIt->getString(m_pldA, m_pldB, m_pldC);

    x = qBound(0, x, (int)str.length());

    int newFirstLine = m_firstLine;
    int newHorizScrollOffset = m_horizScrollOffset;

    if(y < m_firstLine)
        newFirstLine = y;
    else if(y > m_firstLine + getNofVisibleLines())
        newFirstLine = y - getNofVisibleLines();

    QTextLayout textLayout(str, font(), this);
    getTextLayoutForLine(m_cursorYPos, str, textLayout);

    // try to preserve cursor x pixel position when moving to another line
    if(bYMoveKey)
    {
        if(m_pOptions->m_bRightToLeftLanguage)
            x = textLayout.lineAt(0).xToCursor(m_cursorOldXPixelPos - (textLayout.position().x() - m_horizScrollOffset));
        else
            x = textLayout.lineAt(0).xToCursor(m_cursorOldXPixelPos);
    }

    m_cursorXPixelPos =  qCeil(textLayout.lineAt(0).cursorToX(x));
    int hF = 1; // horizontal factor
    if(m_pOptions->m_bRightToLeftLanguage)
    {
        m_cursorXPixelPos +=  qCeil(textLayout.position().x() - m_horizScrollOffset);
        hF = -1;
    }
    int cursorWidth = 5;
    if(m_cursorXPixelPos < hF * m_horizScrollOffset)
        newHorizScrollOffset = hF * m_cursorXPixelPos;
    else if(m_cursorXPixelPos > hF * m_horizScrollOffset + getVisibleTextAreaWidth() - cursorWidth)
        newHorizScrollOffset = hF * (m_cursorXPixelPos - (getVisibleTextAreaWidth() - cursorWidth));

    int newCursorX = x;
    if(bShift)
    {
        if(!m_selection.isValidFirstLine())
            m_selection.start(m_cursorYPos, m_cursorXPos);

        m_selection.end(y, newCursorX);
    }
    else
        m_selection.reset();

    m_cursorYPos = y;
    m_cursorXPos = newCursorX;

    // TODO if width of current line exceeds the current maximum width then force recalculating the scrollbars
    if(textLayout.maximumWidth() > getMaxTextWidth())
    {
        m_maxTextWidth =  qCeil(textLayout.maximumWidth());
        Q_EMIT resizeSignal();
    }
    if(!bYMoveKey)
        m_cursorOldXPixelPos = m_cursorXPixelPos;

    m_bCursorOn = true;
    m_cursorTimer.start(500);

    Q_EMIT updateAvailabilities();
    update();
    if(newFirstLine != m_firstLine || newHorizScrollOffset != m_horizScrollOffset)
    {
        Q_EMIT scrollMergeResultWindow(newHorizScrollOffset - m_horizScrollOffset, newFirstLine - m_firstLine);
        return;
    }
}

void MergeResultWindow::calcIteratorFromLineNr(
    int line,
    MergeLineList::iterator& mlIt,
    MergeEditLineList::iterator& melIt)
{
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        if(line > ml.mergeEditLineList.size())
        {
            line -= ml.mergeEditLineList.size();
        }
        else
        {
            for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt)
            {
                --line;
                if(line < 0) return;
            }
        }
    }
}

QString MergeResultWindow::getSelection()
{
    QString selectionString;

    int line = 0;
    MergeLineList::iterator mlIt = m_mergeLineList.begin();
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        MergeEditLineList::iterator melIt;
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt)
        {
            MergeEditLine& mel = *melIt;

            if(m_selection.lineWithin(line))
            {
                int outPos = 0;
                if(mel.isEditableText())
                {
                    const QString str = mel.getString(m_pldA, m_pldB, m_pldC);

                    // Consider tabs

                    for(int i = 0; i < str.length(); ++i)
                    {
                        int spaces = 1;
                        if(str[i] == '\t')
                        {
                            spaces = tabber(outPos, m_pOptions->m_tabSize);
                        }

                        if(m_selection.within(line, outPos))
                        {
                            selectionString += str[i];
                        }

                        outPos += spaces;
                    }
                }
                else if(mel.isConflict())
                {
                    selectionString += i18n("<Merge Conflict>");
                }

                if(m_selection.within(line, outPos))
                {
#ifdef Q_OS_WIN
                    selectionString += '\r';
#endif
                    selectionString += '\n';
                }
            }

            ++line;
        }
    }

    return selectionString;
}

bool MergeResultWindow::deleteSelection2(QString& s, int& x, int& y,
                                         MergeLineList::iterator& mlIt, MergeEditLineList::iterator& melIt)
{
    if(m_selection.selectionContainsData())
    {
        Q_ASSERT(m_selection.isValidFirstLine());
        deleteSelection();
        y = m_cursorYPos;
        calcIteratorFromLineNr(y, mlIt, melIt);
        s = melIt->getString(m_pldA, m_pldB, m_pldC);
        x = m_cursorXPos;
        return true;
    }

    return false;
}

void MergeResultWindow::deleteSelection()
{
    if(!m_selection.selectionContainsData())
    {
        return;
    }
    Q_ASSERT(m_selection.isValidFirstLine());

    setModified();

    LineRef line = 0;
    MergeLineList::iterator mlItFirst;
    MergeEditLineList::iterator melItFirst;
    QString firstLineString;

    LineRef firstLine;
    LineRef lastLine;

    MergeLineList::iterator mlIt;
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        MergeEditLineList::iterator melIt;
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt)
        {
            MergeEditLine& mel = *melIt;

            if(mel.isEditableText() && m_selection.lineWithin(line))
            {
                if(!firstLine.isValid())
                    firstLine = line;
                lastLine = line;
            }

            ++line;
        }
    }

    if(!firstLine.isValid())
    {
        return; // Nothing to delete.
    }

    line = 0;
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        MergeEditLineList::iterator melIt, melIt1;
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end();)
        {
            MergeEditLine& mel = *melIt;
            melIt1 = melIt;
            ++melIt1;

            if(mel.isEditableText() && m_selection.lineWithin(line))
            {
                QString lineString = mel.getString(m_pldA, m_pldB, m_pldC);

                int firstPosInLine = m_selection.firstPosInLine(line);
                int lastPosInLine = m_selection.lastPosInLine(line);

                if(line == firstLine)
                {
                    mlItFirst = mlIt;
                    melItFirst = melIt;
                    int pos = firstPosInLine;
                    firstLineString = lineString.left(pos);
                }

                if(line == lastLine)
                {
                    // This is the last line in the selection
                    int pos = lastPosInLine;
                    firstLineString += lineString.midRef(pos); // rest of line
                    melItFirst->setString(firstLineString);
                }

                if(line != firstLine || (m_selection.endPos() - m_selection.beginPos()) == lineString.length())
                {
                    // Remove the line
                    if(mlIt->mergeEditLineList.size() > 1)
                        mlIt->mergeEditLineList.erase(melIt);
                    else
                        melIt->setRemoved();
                }
            }

            ++line;
            melIt = melIt1;
        }
    }

    m_cursorYPos = m_selection.beginLine();
    m_cursorXPos = m_selection.beginPos();
    m_cursorOldXPixelPos = m_cursorXPixelPos;

    m_selection.reset();
}

void MergeResultWindow::pasteClipboard(bool bFromSelection)
{
    //checking of m_selection if needed is done by deleteSelection no need for check here.
    deleteSelection();

    setModified();

    int y = m_cursorYPos;
    MergeLineList::iterator mlIt;
    MergeEditLineList::iterator melIt, melItAfter;
    calcIteratorFromLineNr(y, mlIt, melIt);
    melItAfter = melIt;
    ++melItAfter;
    QString str = melIt->getString(m_pldA, m_pldB, m_pldC);
    int x = m_cursorXPos;

    if(!QApplication::clipboard()->supportsSelection())
        bFromSelection = false;

    QString clipBoard = QApplication::clipboard()->text(bFromSelection ? QClipboard::Selection : QClipboard::Clipboard);

    QString currentLine = str.left(x);
    QString endOfLine = str.mid(x);
    int i;
    int len = clipBoard.length();
    for(i = 0; i < len; ++i)
    {
        QChar c = clipBoard[i];
        if(c == '\n' || (c == '\r' && clipBoard[i+1] != '\n'))
        {
            melIt->setString(currentLine);
            MergeEditLine mel(mlIt->id3l); // Associate every mel with an id3l, even if not really valid.
            melIt = mlIt->mergeEditLineList.insert(melItAfter, mel);
            currentLine = "";
            x = 0;
            ++y;
        }
        else
        {
            if(c == '\r') continue;

            currentLine += c;
            ++x;
        }
    }

    currentLine += endOfLine;
    melIt->setString(currentLine);

    m_cursorYPos = y;
    m_cursorXPos = x;
    m_cursorOldXPixelPos = m_cursorXPixelPos;

    update();
}

void MergeResultWindow::resetSelection()
{
    m_selection.reset();
    update();
}

void MergeResultWindow::setModified(bool bModified)
{
    if(bModified != m_bModified)
    {
        m_bModified = bModified;
        Q_EMIT modifiedChanged(m_bModified);
    }
}

/// Saves and returns true when successful.
bool MergeResultWindow::saveDocument(const QString& fileName, QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle)
{
    // Are still conflicts somewhere?
    if(getNrOfUnsolvedConflicts() > 0)
    {
        KMessageBox::error(this,
                           i18n("Not all conflicts are solved yet.\n"
                                "File not saved."),
                           i18n("Conflicts Left"));
        return false;
    }

    if(eLineEndStyle == eLineEndStyleConflict || eLineEndStyle == eLineEndStyleUndefined)
    {
        KMessageBox::error(this,
                           i18n("There is a line end style conflict. Please choose the line end style manually.\n"
                                "File not saved."),
                           i18n("Conflicts Left"));
        return false;
    }

    update();

    FileAccess file(fileName, true /*bWantToWrite*/);
    if(m_pOptions->m_bDmCreateBakFiles && file.exists())
    {
        bool bSuccess = file.createBackup(".orig");
        if(!bSuccess)
        {
            KMessageBox::error(this, file.getStatusText() + i18n("\n\nCreating backup failed. File not saved."), i18n("File Save Error"));
            return false;
        }
    }

    QByteArray dataArray;
    QTextStream textOutStream(&dataArray, QIODevice::WriteOnly);
    if(pEncoding->name() == "UTF-8")
        textOutStream.setGenerateByteOrderMark(false); // Shouldn't be necessary. Bug in Qt or docs
    else
        textOutStream.setGenerateByteOrderMark(true); // Only for UTF-16
    textOutStream.setCodec(pEncoding);

    int line = 0;
    MergeLineList::iterator mlIt = m_mergeLineList.begin();
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        MergeEditLineList::iterator melIt;
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt)
        {
            MergeEditLine& mel = *melIt;

            if(mel.isEditableText())
            {
                QString str = mel.getString(m_pldA, m_pldB, m_pldC);

                if(line > 0) // Prepend line feed, but not for first line
                {
                    if(eLineEndStyle == eLineEndStyleDos)
                    {
                        str.prepend("\r\n");
                    }
                    else
                    {
                        str.prepend("\n");
                    }
                }

                textOutStream << str;
                ++line;
            }
        }
    }
    textOutStream.flush();
    bool bSuccess = file.writeFile(dataArray.data(), dataArray.size());
    if(!bSuccess)
    {
        KMessageBox::error(this, i18n("Error while writing."), i18n("File Save Error"));
        return false;
    }

    setModified(false);
    update();

    return true;
}

QString MergeResultWindow::getString(int lineIdx)
{
    MergeLineList::iterator mlIt;
    MergeEditLineList::iterator melIt;
    if(m_mergeLineList.empty())
    {
        return QString();
    }
    calcIteratorFromLineNr(lineIdx, mlIt, melIt);
    QString s = melIt->getString(m_pldA, m_pldB, m_pldC);
    return s;
}

bool MergeResultWindow::findString(const QString& s, LineRef& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive)
{
    int it = d3vLine;
    int endIt = bDirDown ? getNofLines() : -1;
    int step = bDirDown ? 1 : -1;
    int startPos = posInLine;

    for(; it != endIt; it += step)
    {
        QString line = getString(it);
        if(!line.isEmpty())
        {
            int pos = line.indexOf(s, startPos, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
            if(pos != -1)
            {
                d3vLine = it;
                posInLine = pos;
                return true;
            }

            startPos = 0;
        }
    }
    return false;
}

void MergeResultWindow::setSelection(int firstLine, int startPos, int lastLine, int endPos)
{
    if(lastLine >= getNofLines())
    {
        lastLine = getNofLines() - 1;
        QString s = getString(lastLine);
        endPos = s.length();
    }
    m_selection.reset();
    m_selection.start(firstLine, startPos);
    m_selection.end(lastLine, endPos);
    update();
}

WindowTitleWidget::WindowTitleWidget(const QSharedPointer<Options> &pOptions)
{
    m_pOptions = pOptions;
    setAutoFillBackground(true);

    QHBoxLayout* pHLayout = new QHBoxLayout(this);
    pHLayout->setMargin(2);
    pHLayout->setSpacing(2);

    m_pLabel = new QLabel(i18n("Output:"));
    pHLayout->addWidget(m_pLabel);

    m_pFileNameLineEdit = new FileNameLineEdit();
    pHLayout->addWidget(m_pFileNameLineEdit, 6);
    m_pFileNameLineEdit->installEventFilter(this);//for focus tracking
    m_pFileNameLineEdit->setAcceptDrops(true);
    m_pFileNameLineEdit->setReadOnly(true);

    //m_pBrowseButton = new QPushButton("...");
    //pHLayout->addWidget( m_pBrowseButton, 0 );
    //chk_connect_a( m_pBrowseButton, &QPushButton::clicked), this, &MergeResultWindow::slotBrowseButtonClicked);

    m_pModifiedLabel = new QLabel(i18n("[Modified]"));
    pHLayout->addWidget(m_pModifiedLabel);
    m_pModifiedLabel->setMinimumSize(m_pModifiedLabel->sizeHint());
    m_pModifiedLabel->setText("");

    pHLayout->addStretch(1);

    m_pEncodingLabel = new QLabel(i18n("Encoding for saving:"));
    pHLayout->addWidget(m_pEncodingLabel);

    m_pEncodingSelector = new QComboBox();
    m_pEncodingSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    pHLayout->addWidget(m_pEncodingSelector, 2);
    setEncodings(nullptr, nullptr, nullptr);

    m_pLineEndStyleLabel = new QLabel(i18n("Line end style:"));
    pHLayout->addWidget(m_pLineEndStyleLabel);
    m_pLineEndStyleSelector = new QComboBox();
    m_pLineEndStyleSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    pHLayout->addWidget(m_pLineEndStyleSelector);
    setLineEndStyles(eLineEndStyleUndefined, eLineEndStyleUndefined, eLineEndStyleUndefined);
}

void WindowTitleWidget::setFileName(const QString& fileName)
{
    m_pFileNameLineEdit->setText(QDir::toNativeSeparators(fileName));
}

QString WindowTitleWidget::getFileName()
{
    return m_pFileNameLineEdit->text();
}

//static QString getLineEndStyleName( e_LineEndStyle eLineEndStyle )
//{
//   if ( eLineEndStyle == eLineEndStyleDos )
//      return "DOS";
//   else if ( eLineEndStyle == eLineEndStyleUnix )
//      return "Unix";
//   return QString();
//}

void WindowTitleWidget::setLineEndStyles(e_LineEndStyle eLineEndStyleA, e_LineEndStyle eLineEndStyleB, e_LineEndStyle eLineEndStyleC)
{
    m_pLineEndStyleSelector->clear();
    QString dosUsers;
    if(eLineEndStyleA == eLineEndStyleDos)
        dosUsers += i18n("A");
    if(eLineEndStyleB == eLineEndStyleDos)
        dosUsers += QLatin1String(dosUsers.isEmpty() ? "" : ", ") + i18n("B");
    if(eLineEndStyleC == eLineEndStyleDos)
        dosUsers += QLatin1String(dosUsers.isEmpty() ? "" : ", ") + i18n("C");
    QString unxUsers;
    if(eLineEndStyleA == eLineEndStyleUnix)
        unxUsers += i18n("A");
    if(eLineEndStyleB == eLineEndStyleUnix)
        unxUsers += QLatin1String(unxUsers.isEmpty() ? "" : ", ") + i18n("B");
    if(eLineEndStyleC == eLineEndStyleUnix)
        unxUsers += QLatin1String(unxUsers.isEmpty() ? "" : ", ") + i18n("C");

    m_pLineEndStyleSelector->addItem(i18n("Unix") + (unxUsers.isEmpty() ? QString("") : QLatin1String(" (") + unxUsers + QLatin1String(")")));
    m_pLineEndStyleSelector->addItem(i18n("DOS") + (dosUsers.isEmpty() ? QString("") : QLatin1String(" (") + dosUsers + QLatin1String(")")));

    e_LineEndStyle autoChoice = (e_LineEndStyle)m_pOptions->m_lineEndStyle;

    if(m_pOptions->m_lineEndStyle == eLineEndStyleAutoDetect)
    {
        if(eLineEndStyleA != eLineEndStyleUndefined && eLineEndStyleB != eLineEndStyleUndefined && eLineEndStyleC != eLineEndStyleUndefined)
        {
            if(eLineEndStyleA == eLineEndStyleB)
                autoChoice = eLineEndStyleC;
            else if(eLineEndStyleA == eLineEndStyleC)
                autoChoice = eLineEndStyleB;
            else
                autoChoice = eLineEndStyleConflict; //conflict (not likely while only two values exist)
        }
        else
        {
            e_LineEndStyle c1, c2;
            if(eLineEndStyleA == eLineEndStyleUndefined)
            {
                c1 = eLineEndStyleB;
                c2 = eLineEndStyleC;
            }
            else if(eLineEndStyleB == eLineEndStyleUndefined)
            {
                c1 = eLineEndStyleA;
                c2 = eLineEndStyleC;
            }
            else /*if( eLineEndStyleC == eLineEndStyleUndefined )*/
            {
                c1 = eLineEndStyleA;
                c2 = eLineEndStyleB;
            }
            if(c1 == c2 && c1 != eLineEndStyleUndefined)
                autoChoice = c1;
            else
                autoChoice = eLineEndStyleConflict;
        }
    }

    if(autoChoice == eLineEndStyleUnix)
        m_pLineEndStyleSelector->setCurrentIndex(0);
    else if(autoChoice == eLineEndStyleDos)
        m_pLineEndStyleSelector->setCurrentIndex(1);
    else if(autoChoice == eLineEndStyleConflict)
    {
        m_pLineEndStyleSelector->addItem(i18n("Conflict"));
        m_pLineEndStyleSelector->setCurrentIndex(2);
    }
}

e_LineEndStyle WindowTitleWidget::getLineEndStyle()
{

    int current = m_pLineEndStyleSelector->currentIndex();
    if(current == 0)
        return eLineEndStyleUnix;
    else if(current == 1)
        return eLineEndStyleDos;
    else
        return eLineEndStyleConflict;
}

void WindowTitleWidget::setEncodings(QTextCodec* pCodecForA, QTextCodec* pCodecForB, QTextCodec* pCodecForC)
{
    m_pEncodingSelector->clear();

    // First sort codec names:
    std::map<QString, QTextCodec*> names;
    QList<int> mibs = QTextCodec::availableMibs();
    for(int i: mibs)
    {
        QTextCodec* c = QTextCodec::codecForMib(i);
        if(c != nullptr)
            names[QLatin1String(c->name())] = c;
    }

    if(pCodecForA != nullptr)
        m_pEncodingSelector->addItem(i18n("Codec from A: %1", QLatin1String(pCodecForA->name())), QVariant::fromValue((void*)pCodecForA));
    if(pCodecForB != nullptr)
        m_pEncodingSelector->addItem(i18n("Codec from B: %1", QLatin1String(pCodecForB->name())), QVariant::fromValue((void*)pCodecForB));
    if(pCodecForC != nullptr)
        m_pEncodingSelector->addItem(i18n("Codec from C: %1", QLatin1String(pCodecForC->name())), QVariant::fromValue((void*)pCodecForC));

    std::map<QString, QTextCodec*>::iterator it;
    for(it = names.begin(); it != names.end(); ++it)
    {
        m_pEncodingSelector->addItem(it->first, QVariant::fromValue((void*)it->second));
    }
    m_pEncodingSelector->setMinimumSize(m_pEncodingSelector->sizeHint());

    if(pCodecForC != nullptr && pCodecForB != nullptr && pCodecForA != nullptr)
    {
        if(pCodecForA == pCodecForC)
            m_pEncodingSelector->setCurrentIndex(1); // B
        else
            m_pEncodingSelector->setCurrentIndex(2); // C
    }
    else if(pCodecForA != nullptr && pCodecForB != nullptr)
        m_pEncodingSelector->setCurrentIndex(1); // B
    else
        m_pEncodingSelector->setCurrentIndex(0);
}

QTextCodec* WindowTitleWidget::getEncoding()
{
    return (QTextCodec*)m_pEncodingSelector->itemData(m_pEncodingSelector->currentIndex()).value<void*>();
}

void WindowTitleWidget::setEncoding(QTextCodec* pEncoding)
{
    int idx = m_pEncodingSelector->findText(QLatin1String(pEncoding->name()));
    if(idx >= 0)
        m_pEncodingSelector->setCurrentIndex(idx);
}

//void WindowTitleWidget::slotBrowseButtonClicked()
//{
//   QString current = m_pFileNameLineEdit->text();
//
//   QUrl newURL = KFileDialog::getSaveUrl( current, 0, this, i18n("Select file (not saving yet)"));
//   if ( !newURL.isEmpty() )
//   {
//      m_pFileNameLineEdit->setText( newURL.url() );
//   }
//}

void WindowTitleWidget::slotSetModified(bool bModified)
{
    m_pModifiedLabel->setText(bModified ? i18n("[Modified]") : "");
}

bool WindowTitleWidget::eventFilter(QObject* o, QEvent* e)
{
    Q_UNUSED(o);
    if(e->type() == QEvent::FocusIn || e->type() == QEvent::FocusOut)
    {
        QPalette p = m_pLabel->palette();

        QColor c1 = m_pOptions->m_fgColor;
        QColor c2 = Qt::lightGray;
        if(e->type() == QEvent::FocusOut)
            c2 = m_pOptions->m_bgColor;

        p.setColor(QPalette::Window, c2);
        setPalette(p);

        p.setColor(QPalette::WindowText, c1);
        m_pLabel->setPalette(p);
        m_pEncodingLabel->setPalette(p);
        m_pEncodingSelector->setPalette(p);
    }
    return false;
}

//#include "mergeresultwindow.moc"
