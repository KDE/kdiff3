// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "mergeresultwindow.h"

#include "defmac.h"
#include "kdiff3.h"
#include "options.h"
#include "RLPainter.h"
#include "guiutils.h"
#include "TypeUtils.h"
#include "Utils.h"             // for Utils

#include <memory>

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
#include <QRegularExpression>
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
    const QSharedPointer<Options>& pOptions,
    QStatusBar* pStatusBar)
    : QWidget(pParent)
{
    setObjectName("MergeResultWindow");
    setFocusPolicy(Qt::ClickFocus);

    mOverviewMode = e_OverviewMode::eOMNormal;

    m_pStatusBar = pStatusBar;
    if(m_pStatusBar != nullptr)
        chk_connect(m_pStatusBar, &QStatusBar::messageChanged, this, &MergeResultWindow::slotStatusMessageChanged);

    m_pOptions = pOptions;
    setUpdatesEnabled(false);

    chk_connect(&m_cursorTimer, &QTimer::timeout, this, &MergeResultWindow::slotCursorUpdate);
    m_cursorTimer.setSingleShot(true);
    m_cursorTimer.start(500 /*ms*/);
    m_selection.reset();

    setMinimumSize(QSize(20, 20));
    setFont(m_pOptions->defaultFont());
}

void MergeResultWindow::init(
    const std::shared_ptr<LineDataVector> &pLineDataA, LineRef sizeA,
    const std::shared_ptr<LineDataVector> &pLineDataB, LineRef sizeB,
    const std::shared_ptr<LineDataVector> &pLineDataC, LineRef sizeC,
    const Diff3LineList* pDiff3LineList,
    TotalDiffStatus* pTotalDiffStatus,
    bool bAutoSolve)
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

    merge(bAutoSolve, e_SrcSelector::Invalid);
    update();
    updateSourceMask();

    showUnsolvedConflictsStatusMessage();
}

//This must be called before KXMLGui::SetXMLFile and friends or the actions will not be shown in the menu.
//At that point in startup we don't have a MergeResultWindow object so we cannot connect the signals yet.
void MergeResultWindow::initActions(KActionCollection* ac)
{
    if(ac == nullptr)
    {
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
    static bool setupComplete = false;
    if(setupComplete) return;

    setupComplete = true;
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

void MergeResultWindow::setupConnections(const KDiff3App* app)
{
    chk_connect(app, &KDiff3App::cut, this, &MergeResultWindow::slotCut);
    chk_connect(app, &KDiff3App::copy, this, &MergeResultWindow::slotCopy);
    chk_connect(app, &KDiff3App::selectAll, this, &MergeResultWindow::slotSelectAll);

    chk_connect(this, &MergeResultWindow::scrollMergeResultWindow, app, &KDiff3App::scrollMergeResultWindow);
    chk_connect(this, &MergeResultWindow::sourceMask, app, &KDiff3App::sourceMask);
    chk_connect(this, &MergeResultWindow::resizeSignal, app, &KDiff3App::setHScrollBarRange);
    chk_connect(this, &MergeResultWindow::resizeSignal, this, &MergeResultWindow::slotResize);

    chk_connect(this, &MergeResultWindow::selectionEnd, app, &KDiff3App::slotSelectionEnd);
    chk_connect(this, &MergeResultWindow::newSelection, app, &KDiff3App::slotSelectionStart);
    chk_connect(this, &MergeResultWindow::modifiedChanged, app, &KDiff3App::slotOutputModified);
    //TODO: Why two signals?
    chk_connect(this, &MergeResultWindow::updateAvailabilities, app, &KDiff3App::slotUpdateAvailabilities);
    chk_connect(this, &MergeResultWindow::updateAvailabilities, this, &MergeResultWindow::slotUpdateAvailabilities);
    chk_connect(app, &KDiff3App::updateAvailabilities, this, &MergeResultWindow::slotUpdateAvailabilities);
    chk_connect(this, &MergeResultWindow::showPopupMenu, app, &KDiff3App::showPopupMenu);
    chk_connect(this, &MergeResultWindow::noRelevantChangesDetected, app, &KDiff3App::slotNoRelevantChangesDetected);
    chk_connect(this, &MergeResultWindow::statusBarMessage, app, &KDiff3App::slotStatusMsg);
    //connect menu actions
    chk_connect(app, &KDiff3App::showWhiteSpaceToggled, this, static_cast<void (MergeResultWindow::*)(void)>(&MergeResultWindow::update));
    chk_connect(app, &KDiff3App::doRefresh, this, &MergeResultWindow::slotRefresh);

    chk_connect(app, &KDiff3App::autoSolve, this, &MergeResultWindow::slotAutoSolve);
    chk_connect(app, &KDiff3App::unsolve, this, &MergeResultWindow::slotUnsolve);
    chk_connect(app, &KDiff3App::mergeHistory, this, &MergeResultWindow::slotMergeHistory);
    chk_connect(app, &KDiff3App::regExpAutoMerge, this, &MergeResultWindow::slotRegExpAutoMerge);

    chk_connect(app, &KDiff3App::goCurrent, this, &MergeResultWindow::slotGoCurrent);
    chk_connect(app, &KDiff3App::goTop, this, &MergeResultWindow::slotGoTop);
    chk_connect(app, &KDiff3App::goBottom, this, &MergeResultWindow::slotGoBottom);
    chk_connect(app, &KDiff3App::goPrevUnsolvedConflict, this, &MergeResultWindow::slotGoPrevUnsolvedConflict);
    chk_connect(app, &KDiff3App::goNextUnsolvedConflict, this, &MergeResultWindow::slotGoNextUnsolvedConflict);
    chk_connect(app, &KDiff3App::goPrevConflict, this, &MergeResultWindow::slotGoPrevConflict);
    chk_connect(app, &KDiff3App::goNextConflict, this, &MergeResultWindow::slotGoNextConflict);
    chk_connect(app, &KDiff3App::goPrevDelta, this, &MergeResultWindow::slotGoPrevDelta);
    chk_connect(app, &KDiff3App::goNextDelta, this, &MergeResultWindow::slotGoNextDelta);

    chk_connect(app, &KDiff3App::changeOverViewMode, this, &MergeResultWindow::setOverviewMode);

    connections.push_back(KDiff3App::allowCut.connect(boost::bind(&MergeResultWindow::canCut, this)));
    connections.push_back(KDiff3App::allowCopy.connect(boost::bind(&MergeResultWindow::canCopy, this)));
    connections.push_back(KDiff3App::getSelection.connect(boost::bind(&MergeResultWindow::getSelection, this)));
}

void MergeResultWindow::slotResize()
{
    mVScrollBar->setRange(0, std::max(0, getNofLines() - getNofVisibleLines()));
    mVScrollBar->setPageStep(getNofVisibleLines());
}

void MergeResultWindow::slotCut()
{
    const QString curSelection = getSelection();
    assert(!curSelection.isEmpty() && hasFocus());
    deleteSelection();
    update();

    QApplication::clipboard()->setText(curSelection, QClipboard::Clipboard);
}

void MergeResultWindow::slotCopy()
{
    if(!hasFocus())
        return;

    const QString curSelection = getSelection();

    if(!curSelection.isEmpty())
    {
        QApplication::clipboard()->setText(curSelection, QClipboard::Clipboard);
    }
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
        qint32 wsc;
        qint32 nofUnsolved = getNumberOfUnsolvedConflicts(&wsc);

        m_persistentStatusMessage = i18n("Number of remaining unsolved conflicts: %1 (of which %2 are whitespace)", nofUnsolved, wsc);

        Q_EMIT statusBarMessage(m_persistentStatusMessage);
    }
}

void MergeResultWindow::slotRefresh()
{
    setFont(m_pOptions->defaultFont());
    update();
}

void MergeResultWindow::slotUpdateAvailabilities()
{
    const QWidget* frame = qobject_cast<QWidget*>(parent());
    assert(frame != nullptr);
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
    m_mergeLineList.list().clear();

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

void MergeResultWindow::merge(bool bAutoSolve, e_SrcSelector defaultSelector, bool bConflictsOnly, bool bWhiteSpaceOnly)
{
    const bool lIsThreeWay = m_pldC != nullptr;

    if(!bConflictsOnly)
    {
        if(m_bModified)
        {
            qint32 result = KMessageBox::warningYesNo(this,
                                                   i18n("The output has been modified.\n"
                                                        "If you continue your changes will be lost."),
                                                   i18nc("Error dialog caption", "Warning"),
                                                   KStandardGuiItem::cont(),
                                                   KStandardGuiItem::cancel());
            if(result == KMessageBox::No)
                return;
        }

        m_mergeLineList.list().clear();

        m_mergeLineList.buildFromDiff3(*m_pDiff3LineList, lIsThreeWay);
    }

    bool bSolveWhiteSpaceConflicts = false;
    if(bAutoSolve) // when true, then the other params are not used and we can change them here. (see all invocations of merge())
    {
        if(!lIsThreeWay && m_pOptions->m_whiteSpace2FileMergeDefault != (qint32)e_SrcSelector::None) // Only two inputs
        {
            assert(m_pOptions->m_whiteSpace2FileMergeDefault <= (qint32)e_SrcSelector::Max && m_pOptions->m_whiteSpace2FileMergeDefault >= (qint32)e_SrcSelector::Min);
            defaultSelector = (e_SrcSelector)m_pOptions->m_whiteSpace2FileMergeDefault;
            bWhiteSpaceOnly = true;
            bSolveWhiteSpaceConflicts = true;
        }
        else if(lIsThreeWay && m_pOptions->m_whiteSpace3FileMergeDefault != (qint32)e_SrcSelector::None)
        {
            assert(m_pOptions->m_whiteSpace3FileMergeDefault <= (qint32)e_SrcSelector::Max && m_pOptions->m_whiteSpace2FileMergeDefault >= (qint32)e_SrcSelector::Min);
            defaultSelector = (e_SrcSelector)m_pOptions->m_whiteSpace3FileMergeDefault;
            bWhiteSpaceOnly = true;
            bSolveWhiteSpaceConflicts = true;
        }
    }

    if(!bAutoSolve || bSolveWhiteSpaceConflicts)
    {
        m_mergeLineList.updateDefaults(defaultSelector, bConflictsOnly, bWhiteSpaceOnly);
    }

    MergeLineListImp::iterator mlIt;
    for(mlIt = m_mergeLineList.list().begin(); mlIt != m_mergeLineList.list().end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        // Remove all lines that are empty, because no src lines are there.
        ml.removeEmptySource();
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

    qint32 nrOfSolvedConflicts = 0;
    qint32 nrOfUnsolvedConflicts = 0;
    qint32 nrOfWhiteSpaceConflicts = 0;

    MergeLineListImp::iterator i;
    for(i = m_mergeLineList.list().begin(); i != m_mergeLineList.list().end(); ++i)
    {
        if(i->isConflict())
            ++nrOfUnsolvedConflicts;
        else if(i->isDelta())
            ++nrOfSolvedConflicts;

        if(i->isWhiteSpaceConflict())
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

    m_currentMergeLineIt = m_mergeLineList.list().begin();
    slotGoTop();

    Q_EMIT updateAvailabilities();
    update();
}

void MergeResultWindow::setFirstLine(qint32 firstLine)
{
    m_firstLine = std::max(0, firstLine);
    update();
}

void MergeResultWindow::setHorizScrollOffset(qint32 horizScrollOffset)
{
    m_horizScrollOffset = std::max(0, horizScrollOffset);
    update();
}

qint32 MergeResultWindow::getMaxTextWidth()
{
    if(m_maxTextWidth < 0)
    {
        m_maxTextWidth = 0;

        for(const MergeLine& ml: m_mergeLineList.list())
        {
            for(const MergeEditLine& mel: ml.list())
            {
                QString s = mel.getString(m_pldA, m_pldB, m_pldC);

                QTextLayout textLayout(s, font(), this);
                textLayout.beginLayout();
                textLayout.createLine();
                textLayout.endLayout();
                if(m_maxTextWidth < textLayout.maximumWidth())
                {
                    m_maxTextWidth = qCeil(textLayout.maximumWidth());
                }
            }
        }
        m_maxTextWidth += 5; // cursorwidth
    }
    return m_maxTextWidth;
}

qint32 MergeResultWindow::getNofLines() const
{
    return m_nofLines;
}

qint32 MergeResultWindow::getVisibleTextAreaWidth() const
{
    return width() - getTextXOffset();
}

qint32 MergeResultWindow::getNofVisibleLines() const
{
    QFontMetrics fm = fontMetrics();
    return (height() - 3) / fm.lineSpacing() - 2;
}

qint32 MergeResultWindow::getTextXOffset() const
{
    QFontMetrics fm = fontMetrics();
    return 3 * Utils::getHorizontalAdvance(fm, '0');
}

void MergeResultWindow::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    Q_EMIT resizeSignal();
}

e_OverviewMode MergeResultWindow::getOverviewMode() const
{
    return mOverviewMode;
}

void MergeResultWindow::setOverviewMode(e_OverviewMode eOverviewMode)
{
    mOverviewMode = eOverviewMode;
}

// Check whether we should ignore current delta when moving to next/previous delta
bool MergeResultWindow::checkOverviewIgnore(MergeLineListImp::iterator& i) const
{
    if(mOverviewMode == e_OverviewMode::eOMNormal) return false;
    if(mOverviewMode == e_OverviewMode::eOMAvsB)
        return i->details() == e_MergeDetails::eCAdded || i->details() == e_MergeDetails::eCDeleted || i->details() == e_MergeDetails::eCChanged;
    if(mOverviewMode == e_OverviewMode::eOMAvsC)
        return i->details() == e_MergeDetails::eBAdded || i->details() == e_MergeDetails::eBDeleted || i->details() == e_MergeDetails::eBChanged;
    if(mOverviewMode == e_OverviewMode::eOMBvsC)
        return i->details() == e_MergeDetails::eBCAddedAndEqual || i->details() == e_MergeDetails::eBCDeleted || i->details() == e_MergeDetails::eBCChangedAndEqual;
    return false;
}

// Go to prev/next delta/conflict or first/last delta.
void MergeResultWindow::go(e_Direction eDir, e_EndPoint eEndPoint)
{
    assert(eDir == eUp || eDir == eDown);
    MergeLineListImp::iterator i = m_currentMergeLineIt;
    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;
    if(eEndPoint == eEnd)
    {
        if(eDir == eUp)
            i = m_mergeLineList.list().begin(); // first mergeline
        else
            i = --m_mergeLineList.list().end(); // last mergeline

        while(isItAtEnd(eDir == eUp, i) && !i->isDelta())
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
        } while(isItAtEnd(eDir != eUp, i) && (!i->isDelta() || checkOverviewIgnore(i) || (bSkipWhiteConflicts && i->isWhiteSpaceConflict())));
    }
    else if(eEndPoint == eConflict && isItAtEnd(eDir != eUp, i))
    {
        do
        {
            if(eDir == eUp)
                --i;
            else
                ++i;
        } while(isItAtEnd(eDir != eUp, i) && (!i->isConflict() || (bSkipWhiteConflicts && i->isWhiteSpaceConflict())));
    }
    else if(isItAtEnd(eDir != eUp, i) && eEndPoint == eUnsolvedConflict)
    {
        do
        {
            if(eDir == eUp)
                --i;
            else
                ++i;
        } while(isItAtEnd(eDir != eUp, i) && !i->list().begin()->isConflict());
    }

    if(isVisible())
        setFocus();

    setFastSelector(i);
}

bool MergeResultWindow::isDeltaAboveCurrent() const
{
    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;
    if(m_mergeLineList.list().empty()) return false;
    MergeLineListImp::iterator i = m_currentMergeLineIt;
    if(i == m_mergeLineList.list().begin()) return false;
    do
    {
        --i;
        if(i->isDelta() && !checkOverviewIgnore(i) && !(bSkipWhiteConflicts && i->isWhiteSpaceConflict())) return true;
    } while(i != m_mergeLineList.list().begin());

    return false;
}

bool MergeResultWindow::isDeltaBelowCurrent() const
{
    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;
    if(m_mergeLineList.list().empty()) return false;

    MergeLineListImp::iterator i = m_currentMergeLineIt;
    if(i != m_mergeLineList.list().end())
    {
        ++i;
        for(; i != m_mergeLineList.list().end(); ++i)
        {
            if(i->isDelta() && !checkOverviewIgnore(i) && !(bSkipWhiteConflicts && i->isWhiteSpaceConflict())) return true;
        }
    }
    return false;
}

bool MergeResultWindow::isConflictAboveCurrent() const
{
    if(m_mergeLineList.list().empty()) return false;
    MergeLineListImp::const_iterator i = m_currentMergeLineIt;
    if(i == m_mergeLineList.list().cbegin()) return false;

    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;

    do
    {
        --i;
        if(i->isConflict() && !(bSkipWhiteConflicts && i->isWhiteSpaceConflict())) return true;
    } while(i != m_mergeLineList.list().cbegin());

    return false;
}

bool MergeResultWindow::isConflictBelowCurrent() const
{
    MergeLineListImp::const_iterator i = m_currentMergeLineIt;
    if(m_mergeLineList.list().empty()) return false;

    bool bSkipWhiteConflicts = !m_pOptions->m_bShowWhiteSpace;

    if(i != m_mergeLineList.list().cend())
    {
        ++i;
        for(; i != m_mergeLineList.list().cend(); ++i)
        {
            if(i->isConflict() && !(bSkipWhiteConflicts && i->isWhiteSpaceConflict())) return true;
        }
    }
    return false;
}

bool MergeResultWindow::isUnsolvedConflictAtCurrent() const
{
    if(m_mergeLineList.list().empty()) return false;

    return m_currentMergeLineIt->list().cbegin()->isConflict();
}

bool MergeResultWindow::isUnsolvedConflictAboveCurrent() const
{
    if(m_mergeLineList.list().empty()) return false;
    MergeLineListImp::const_iterator i = m_currentMergeLineIt;
    if(i == m_mergeLineList.list().cbegin()) return false;

    do
    {
        --i;
        if(i->list().cbegin()->isConflict()) return true;
    } while(i != m_mergeLineList.list().cbegin());

    return false;
}

bool MergeResultWindow::isUnsolvedConflictBelowCurrent() const
{
    MergeLineListImp::const_iterator i = m_currentMergeLineIt;
    if(m_mergeLineList.list().empty()) return false;

    if(i != m_mergeLineList.list().cend())
    {
        ++i;
        for(; i != m_mergeLineList.list().cend(); ++i)
        {
            if(i->list().cbegin()->isConflict()) return true;
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
    MergeLineListImp::iterator i;
    for(i = m_mergeLineList.list().begin(); i != m_mergeLineList.list().end(); ++i)
    {
        if(line >= i->getIndex() && line < i->getIndex() + i->sourceRangeLength())
        {
            //if ( i->isDelta() )
            {
                setFastSelector(i);
            }
            break;
        }
    }
}

qint32 MergeResultWindow::getNumberOfUnsolvedConflicts(qint32* pNrOfWhiteSpaceConflicts) const
{
    qint32 nrOfUnsolvedConflicts = 0;
    if(pNrOfWhiteSpaceConflicts != nullptr)
        *pNrOfWhiteSpaceConflicts = 0;

    for(const MergeLine& ml: m_mergeLineList.list())
    {
        MergeEditLineList::const_iterator melIt = ml.list().cbegin();
        if(melIt->isConflict())
        {
            ++nrOfUnsolvedConflicts;
            if(ml.isWhiteSpaceConflict() && pNrOfWhiteSpaceConflicts != nullptr)
                ++*pNrOfWhiteSpaceConflicts;
        }
    }

    return nrOfUnsolvedConflicts;
}

void MergeResultWindow::showNumberOfConflicts(bool showIfNone)
{
    if(!m_pOptions->m_bShowInfoDialogs)
        return;
    qint32 nrOfConflicts = 0;
    qint32 nrOfUnsolvedConflicts = getNumberOfUnsolvedConflicts();

    for(const MergeLine& entry: m_mergeLineList.list())
    {
        if(entry.isConflict() || entry.isDelta())
            ++nrOfConflicts;
    }

    if(!showIfNone && nrOfUnsolvedConflicts == 0)
        return;

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

    KMessageBox::information(this,
                             i18n("Total number of conflicts: %1\n"
                                  "Number of automatically solved conflicts: %2\n"
                                  "Number of unsolved conflicts: %3\n"
                                  "%4",
                                  nrOfConflicts, nrOfConflicts - nrOfUnsolvedConflicts,
                                  nrOfUnsolvedConflicts, totalInfo),
                             i18n("Conflicts"));
}

void MergeResultWindow::setFastSelector(MergeLineListImp::iterator i)
{
    if(i == m_mergeLineList.list().end())
        return;
    m_currentMergeLineIt = i;
    Q_EMIT setFastSelectorRange(i->getIndex(), i->sourceRangeLength());

    qint32 line1 = 0;

    MergeLineListImp::const_iterator mlIt = m_mergeLineList.list().cbegin();
    for(; mlIt != m_mergeLineList.list().cend(); ++mlIt)
    {
        if(mlIt == m_currentMergeLineIt)
            break;
        line1 += mlIt->lineCount();
    }

    qint32 nofLines = m_currentMergeLineIt->lineCount();
    qint32 newFirstLine = getBestFirstLine(line1, nofLines, m_firstLine, getNofVisibleLines());
    if(newFirstLine != m_firstLine)
    {
        scrollVertically(newFirstLine - m_firstLine);
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
    if(m_currentMergeLineIt == m_mergeLineList.list().end())
        return;

    setModified();

    // First find range for which this change works.
    MergeLine& ml = *m_currentMergeLineIt;

    MergeEditLineList::iterator melIt;

    // Now check if selector is active for this range already.
    bool bActive = false;

    // Remove unneeded lines in the range.
    for(melIt = ml.list().begin(); melIt != ml.list().end();)
    {
        MergeEditLine& mel = *melIt;
        if(mel.src() == selector)
            bActive = true;

        if(mel.src() == selector || !mel.isEditableText() || mel.isModified())
            melIt = ml.list().erase(melIt);
        else
            ++melIt;
    }

    if(!bActive) // Selected source wasn't active.
    {            // Append the lines from selected source here at rangeEnd.
        Diff3LineList::const_iterator d3llit = ml.id3l();
        qint32 j;

        for(j = 0; j < ml.sourceRangeLength(); ++j)
        {
            MergeEditLine mel(d3llit);
            mel.setSource(selector, false);
            ml.list().push_back(mel);

            ++d3llit;
        }
    }

    if(!ml.list().empty())
    {
        // Remove all lines that are empty, because no src lines are there.
        for(melIt = ml.list().begin(); melIt != ml.list().end();)
        {
            MergeEditLine& mel = *melIt;

            LineRef srcLine = mel.src() == e_SrcSelector::A ? mel.id3l()->getLineA() : mel.src() == e_SrcSelector::B ? mel.id3l()->getLineB() : mel.src() == e_SrcSelector::C ? mel.id3l()->getLineC() : LineRef();

            if(!srcLine.isValid())
                melIt = ml.list().erase(melIt);
            else
                ++melIt;
        }
    }

    if(ml.list().empty())
    {
        // Insert a dummy line:
        MergeEditLine mel(ml.id3l());

        if(bActive)
            mel.setConflict(); // All src entries deleted => conflict
        else
            mel.setRemoved(selector); // No lines in corresponding src found.

        ml.list().push_back(mel);
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
    showNumberOfConflicts();
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
    qint32 i = 0;
    std::list<qint32> startPosStack;
    qint32 length = s.length();
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
            qint32 startPos = startPosStack.back();
            startPosStack.pop_back();
            sl.push_back(s.mid(startPos + 1, i - startPos - 1));
        }
    }
    return startPosStack.empty(); // false if parentheses don't match
}

QString calcHistorySortKey(const QString& keyOrder, QRegularExpressionMatch& regExprMatch, const QStringList& parenthesesGroupList)
{
    const QStringList keyOrderList = keyOrder.split(',');
    QString key;

    for(const QString& keyIt : keyOrderList)
    {
        if(keyIt.isEmpty())
            continue;
        bool bOk = false;
        qint32 groupIdx = keyIt.toInt(&bOk);
        if(!bOk || groupIdx < 0 || groupIdx > parenthesesGroupList.size())
            continue;
        QString s = regExprMatch.captured(groupIdx);
        if(groupIdx == 0)
        {
            key += s + ' ';
            continue;
        }

        QString groupRegExp = parenthesesGroupList[groupIdx - 1];
        if(groupRegExp.indexOf('|') < 0 || groupRegExp.indexOf('(') >= 0)
        {
            bOk = false;
            qint32 i = s.toInt(&bOk);
            if(bOk && i >= 0 && i < 10000)
            {
                s += QString(4 - s.size(), '0'); // This should help for correct sorting of numbers.
            }
            key += s + ' ';
        }
        else
        {
            // Assume that the groupRegExp consists of something like "Jan|Feb|Mar|Apr"
            // s is the string that managed to match.
            // Now we want to know at which position it occurred. e.g. Jan=0, Feb=1, Mar=2, etc.
            QStringList sl = groupRegExp.split('|');
            qint32 idx = sl.indexOf(s);
            if(idx >= 0)
            {
                QString sIdx;
                sIdx.setNum(idx);
                assert(sIdx.size() <= 2);
                sIdx += QString(2 - sIdx.size(), '0'); // Up to 99 words in the groupRegExp (more than 12 aren't expected)
                key += sIdx + ' ';
            }
        }
    }
    return key;
}

void MergeResultWindow::collectHistoryInformation(
    e_SrcSelector src, const HistoryRange& historyRange,
    HistoryMap& historyMap,
    std::list<HistoryMap::iterator>& hitList // list of iterators
)
{
    std::list<HistoryMap::iterator>::iterator itHitListFront = hitList.begin();
    Diff3LineList::const_iterator id3l = historyRange.start;
    QString historyLead;

    historyLead = Utils::calcHistoryLead(id3l->getLineData(src).getLine());

    QRegularExpression historyStart(m_pOptions->m_historyStartRegExp);
    if(id3l == historyRange.end)
        return;
    //TODO: Where is this assumption coming from?
    ++id3l; // Skip line with "$Log ... $"
    QRegularExpression newHistoryEntry(m_pOptions->m_historyEntryStartRegExp);
    QRegularExpressionMatch match;
    QStringList parenthesesGroups;
    findParenthesesGroups(m_pOptions->m_historyEntryStartRegExp, parenthesesGroups);
    QString key;
    MergeEditLineList melList;
    bool bPrevLineIsEmpty = true;
    bool bUseRegExp = !m_pOptions->m_historyEntryStartRegExp.isEmpty();

    for(; id3l != historyRange.end; ++id3l)
    {
        const LineData& pld = id3l->getLineData(src);
        const QString& oriLine = pld.getLine();
        if(historyLead.isEmpty()) historyLead = Utils::calcHistoryLead(oriLine);
        QString sLine = oriLine.mid(historyLead.length());
        match = newHistoryEntry.match(sLine);
        if((!bUseRegExp && !sLine.trimmed().isEmpty() && bPrevLineIsEmpty) || (bUseRegExp && match.hasMatch()))
        {
            if(!key.isEmpty() && !melList.empty())
            {
                // Only insert new HistoryMapEntry if key not found; in either case p.first is a valid iterator to element key.
                std::pair<HistoryMap::iterator, bool> p = historyMap.insert(HistoryMap::value_type(key, HistoryMapEntry()));
                HistoryMapEntry& hme = p.first->second;
                if(src == e_SrcSelector::A) hme.mellA = melList;
                else if(src == e_SrcSelector::B) hme.mellB = melList;
                else hme.mellC = melList;
                if(p.second) // Not in list yet?
                {
                    hitList.insert(itHitListFront, p.first);
                }
            }

            if(!bUseRegExp)
                key = sLine;
            else
                key = calcHistorySortKey(m_pOptions->m_historyEntryStartSortKeyOrder, match, parenthesesGroups);

            melList.clear();
            melList.push_back(MergeEditLine(id3l, src));
        }
        else if(!historyStart.match(oriLine).hasMatch())
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
    HistoryRange        historyRange;

    // Search for history start, history end in the diff3LineList
    m_pDiff3LineList->findHistoryRange(QRegularExpression(m_pOptions->m_historyStartRegExp), m_pldC != nullptr, historyRange);
    if(historyRange.start != m_pDiff3LineList->end())
    {
        // Now collect the historyMap information
        HistoryMap historyMap;
        std::list<HistoryMap::iterator> hitList;
        if(m_pldC == nullptr)
        {
            collectHistoryInformation(e_SrcSelector::A, historyRange, historyMap, hitList);
            collectHistoryInformation(e_SrcSelector::B, historyRange, historyMap, hitList);
        }
        else
        {
            collectHistoryInformation(e_SrcSelector::A, historyRange, historyMap, hitList);
            collectHistoryInformation(e_SrcSelector::B, historyRange, historyMap, hitList);
            collectHistoryInformation(e_SrcSelector::C, historyRange, historyMap, hitList);
        }

        Diff3LineList::const_iterator iD3LHistoryOrigEnd = historyRange.end;

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
                    if(hMapIt->second.staysInPlace(m_pldC != nullptr, historyRange.end))
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
                    if(hMapIt->second.staysInPlace(m_pldC != nullptr, historyRange.end))
                        hitList.pop_back();
                    else
                        break;
                }
            }
            while(iD3LHistoryOrigEnd != historyRange.end)
            {
                --iD3LHistoryOrigEnd;
                --historyRange.endIdx;
            }
        }

        MergeLineListImp::iterator iMLLStart = m_mergeLineList.splitAtDiff3LineIdx(historyRange.startIdx);
        MergeLineListImp::iterator iMLLEnd = m_mergeLineList.splitAtDiff3LineIdx(historyRange.endIdx);
        // Now join all MergeLines in the history
        MergeLineListImp::iterator i = iMLLStart;
        if(i != iMLLEnd)
        {
            ++i;
            while(i != iMLLEnd)
            {
                iMLLStart->join(*i);
                i = m_mergeLineList.list().erase(i);
            }
        }
        iMLLStart->list().clear();
        // Now insert the complete history into the first MergeLine of the history
        iMLLStart->list().push_back(MergeEditLine(historyRange.start, m_pldC == nullptr ? e_SrcSelector::B : e_SrcSelector::C));
        QString lead = Utils::calcHistoryLead(historyRange.start->getString(e_SrcSelector::A));
        MergeEditLine mel(m_pDiff3LineList->end());
        mel.setString(lead);
        iMLLStart->list().push_back(mel);

        qint32 historyCount = 0;
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
                    iMLLStart->list().splice(iMLLStart->list().end(), mell, mell.begin(), mell.end());
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
                    iMLLStart->list().splice(iMLLStart->list().end(), mell, mell.begin(), mell.end());
            }
            // If the end of start is empty and the first line at the end is empty remove the last line of start
            if(!iMLLStart->list().empty() && !iMLLEnd->list().empty())
            {
                QString lastLineOfStart = iMLLStart->list().back().getString(m_pldA, m_pldB, m_pldC);
                QString firstLineOfEnd = iMLLEnd->list().front().getString(m_pldA, m_pldB, m_pldC);
                if(lastLineOfStart.mid(lead.length()).trimmed().isEmpty() && firstLineOfEnd.mid(lead.length()).trimmed().isEmpty())
                    iMLLStart->list().pop_back();
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

    QRegularExpression vcsKeywords(m_pOptions->m_autoMergeRegExp);
    MergeLineListImp::iterator i;
    for(i = m_mergeLineList.list().begin(); i != m_mergeLineList.list().end(); ++i)
    {
        if(i->isConflict())
        {
            Diff3LineList::const_iterator id3l = i->id3l();
            if(vcsKeywords.match(id3l->getString(e_SrcSelector::A)).hasMatch() &&
               vcsKeywords.match(id3l->getString(e_SrcSelector::B)).hasMatch() &&
               (m_pldC == nullptr || vcsKeywords.match(id3l->getString(e_SrcSelector::C)).hasMatch()))
            {
                MergeEditLine& mel = *i->list().begin();
                mel.setSource(m_pldC == nullptr ? e_SrcSelector::B : e_SrcSelector::C, false);
                m_mergeLineList.splitAtDiff3LineIdx(i->getIndex() + 1);
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
    if(m_pldC == nullptr || m_mergeLineList.list().size() <= 1)
        return true;

    MergeLineListImp::iterator i;
    for(i = m_mergeLineList.list().begin(); i != m_mergeLineList.list().end(); ++i)
    {
        if((i->isConflict() && i->list().begin()->src() != e_SrcSelector::C) || i->source() == e_SrcSelector::B)
        {
            return true;
        }
    }

    return false;
}

void MergeResultWindow::slotSplitDiff(qint32 firstD3lLineIdx, qint32 lastD3lLineIdx)
{
    if(lastD3lLineIdx >= 0)
        m_mergeLineList.splitAtDiff3LineIdx(lastD3lLineIdx + 1);
    setFastSelector(m_mergeLineList.splitAtDiff3LineIdx(firstD3lLineIdx));
}

void MergeResultWindow::slotJoinDiffs(qint32 firstD3lLineIdx, qint32 lastD3lLineIdx)
{
    MergeLineListImp::iterator i;
    MergeLineListImp::iterator iMLLStart = m_mergeLineList.list().end();
    MergeLineListImp::iterator iMLLEnd = m_mergeLineList.list().end();
    for(i = m_mergeLineList.list().begin(); i != m_mergeLineList.list().end(); ++i)
    {
        MergeLine& ml = *i;
        if(firstD3lLineIdx >= ml.getIndex() && firstD3lLineIdx < ml.getIndex() + ml.sourceRangeLength())
        {
            iMLLStart = i;
        }
        if(lastD3lLineIdx >= ml.getIndex() && lastD3lLineIdx < ml.getIndex() + ml.sourceRangeLength())
        {
            iMLLEnd = i;
            ++iMLLEnd;
            break;
        }
    }

    bool bJoined = false;
    for(i = iMLLStart; i != iMLLEnd && i != m_mergeLineList.list().end();)
    {
        if(i == iMLLStart)
        {
            ++i;
        }
        else
        {
            iMLLStart->join(*i);
            i = m_mergeLineList.list().erase(i);
            bJoined = true;
        }
    }
    if(bJoined)
    {
        iMLLStart->list().clear();
        // Insert a conflict line as placeholder
        iMLLStart->list().push_back(MergeEditLine(iMLLStart->id3l()));
    }
    setFastSelector(iMLLStart);
}

void MergeResultWindow::myUpdate(qint32 afterMilliSecs)
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
        QtSizeType newPos = m_selection.getLastPos() + m_scrollDeltaX;
        try
        {
            LineRef newLine = m_selection.getLastLine() + m_scrollDeltaY;
            m_selection.end(newLine, newPos > 0 ? newPos : 0);
        }
        catch(const std::system_error&)
        {
            m_selection.end(LineRef::invalid, newPos > 0 ? newPos : 0);
        }

        Q_EMIT scrollMergeResultWindow(m_scrollDeltaX, m_scrollDeltaY);
        killTimer(m_delayedDrawTimer);
        m_delayedDrawTimer = startTimer(50);
    }
}

QVector<QTextLayout::FormatRange> MergeResultWindow::getTextLayoutForLine(LineRef line, const QString& str, QTextLayout& textLayout)
{
    // tabs
    QTextOption textOption;

    textOption.setTabStopDistance(QFontMetricsF(font()).horizontalAdvance(' ') * m_pOptions->m_tabSize);

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
        qint32 firstPosInText = m_selection.firstPosInLine(line);
        qint32 lastPosInText = m_selection.lastPosInLine(line);
        qint32 lengthInText = std::max(0, lastPosInText - firstPosInText);
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
    qint32 cursorWidth = 5;
    if(m_pOptions->m_bRightToLeftLanguage)
        textLayout.setPosition(QPointF(width() - textLayout.maximumWidth() - getTextXOffset() + m_horizScrollOffset - cursorWidth, 0));
    else
        textLayout.setPosition(QPointF(getTextXOffset() - m_horizScrollOffset, 0));
    return selectionFormat;
}

void MergeResultWindow::writeLine(
    RLPainter& p, qint32 line, const QString& str,
    e_SrcSelector srcSelect, e_MergeDetails mergeDetails, qint32 rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict)
{
    const QFontMetrics& fm = fontMetrics();
    qint32 fontHeight = fm.lineSpacing();
    qint32 fontAscent = fm.ascent();

    qint32 topLineYOffset = 0;
    qint32 xOffset = getTextXOffset();

    qint32 yOffset = (line - m_firstLine) * fontHeight;
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

        qint32 outPos = 0;
        QString s;
        qint32 size = str.length();
        for(qint32 i = 0; i < size; ++i)
        {
            qint32 spaces = 1;
            if(str[i] == '\t')
            {
                spaces = tabber(outPos, m_pOptions->m_tabSize);
                for(qint32 j = 0; j < spaces; ++j)
                    s += ' ';
            }
            else
            {
                s += str[i];
            }
            outPos += spaces;
        }

        p.setPen(m_pOptions->forgroundColor());

        QTextLayout textLayout(str, font(), this);
        QVector<QTextLayout::FormatRange> selectionFormat = getTextLayoutForLine(line, str, textLayout);
        textLayout.draw(&p, QPointF(0, yOffset), selectionFormat);

        if(line == m_cursorYPos)
        {
            m_cursorXPixelPos = qCeil(textLayout.lineAt(0).cursorToX(m_cursorXPos));
            if(m_pOptions->m_bRightToLeftLanguage)
                m_cursorXPixelPos += qCeil(textLayout.position().x() - m_horizScrollOffset);
        }

        p.setClipping(false);

        p.setPen(m_pOptions->forgroundColor());

        p.drawText(1, yOffset + fontAscent, srcName, true);
    }
    else if(bLineRemoved)
    {
        p.setPen(m_pOptions->conflictColor());
        p.drawText(xOffset, yOffset + fontAscent, i18n("<No src line>"));
        p.drawText(1, yOffset + fontAscent, srcName);
        if(m_cursorYPos == line) m_cursorXPos = 0;
    }
    else if(srcSelect == e_SrcSelector::None)
    {
        p.setPen(m_pOptions->conflictColor());
        if(bWhiteSpaceConflict)
            p.drawText(xOffset, yOffset + fontAscent, i18n("<Merge Conflict (Whitespace only)>"));
        else
            p.drawText(xOffset, yOffset + fontAscent, i18n("<Merge Conflict>"));
        p.drawText(1, yOffset + fontAscent, "?");
        if(m_cursorYPos == line) m_cursorXPos = 0;
    }
    else
        assert(false);

    xOffset -= Utils::getHorizontalAdvance(fm, '0');
    p.setPen(m_pOptions->forgroundColor());
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
        p.fillRect(xOffset + 3, yOffset, 3, fontHeight, m_pOptions->forgroundColor());
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
        m_currentMergeLineIt = m_mergeLineList.list().end();
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
    qint32 fontWidth = Utils::getHorizontalAdvance(fm, '0');

    if(!m_bCursorUpdate) // Don't redraw everything for blinking cursor?
    {
        m_selection.bSelectionContainsData = false;
        const auto dpr = devicePixelRatioF();
        if(size() * dpr != m_pixmap.size())
        {
            m_pixmap = QPixmap(size() * dpr);
            m_pixmap.setDevicePixelRatio(dpr);
        }

        RLPainter p(&m_pixmap, m_pOptions->m_bRightToLeftLanguage, width(), fontWidth);
        p.setFont(font());
        p.QPainter::fillRect(rect(), m_pOptions->backgroundColor());

        //qint32 visibleLines = height() / fontHeight;

        qint32 lastVisibleLine = m_firstLine + getNofVisibleLines() + 5;
        LineRef line = 0;
        MergeLineListImp::const_iterator mlIt = m_mergeLineList.list().cbegin();
        for(; mlIt != m_mergeLineList.list().cend(); ++mlIt)
        {
            const MergeLine& ml = *mlIt;
            if(line > lastVisibleLine || line + ml.lineCount() < m_firstLine)
            {
                line += ml.lineCount();
            }
            else
            {
                MergeEditLineList::const_iterator melIt;
                for(melIt = ml.list().cbegin(); melIt != ml.list().cend(); ++melIt)
                {
                    if(line >= m_firstLine && line <= lastVisibleLine)
                    {
                        const MergeEditLine& mel = *melIt;
                        MergeEditLineList::const_iterator melIt1 = melIt;
                        ++melIt1;

                        qint32 rangeMark = 0;
                        if(melIt == ml.list().cbegin()) rangeMark |= 1; // Begin range mark
                        if(melIt1 == ml.list().cend()) rangeMark |= 2;  // End range mark

                        if(mlIt == m_currentMergeLineIt) rangeMark |= 4; // Mark of the current line

                        QString s;
                        s = mel.getString(m_pldA, m_pldB, m_pldC);

                        writeLine(p, line, s, mel.src(), ml.details(), rangeMark,
                                  mel.isModified(), mel.isRemoved(), ml.isWhiteSpaceConflict());
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
        painter.setPen(m_pOptions->forgroundColor());

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
    qint32 srcMask = 0;
    qint32 enabledMask = 0;
    if(!hasFocus() || m_pDiff3LineList == nullptr || !updatesEnabled() || m_currentMergeLineIt == m_mergeLineList.list().end())
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
        for(melIt = ml.list().begin(); melIt != ml.list().end(); ++melIt)
        {
            MergeEditLine& mel = *melIt;
            if(mel.src() == e_SrcSelector::A) srcMask |= 1;
            if(mel.src() == e_SrcSelector::B) srcMask |= 2;
            if(mel.src() == e_SrcSelector::C) srcMask |= 4;
            if(mel.isModified() || !mel.isEditableText()) bModified = true;
        }

        if(ml.details() == e_MergeDetails::eNoChange)
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

LineRef MergeResultWindow::convertToLine(qint32 y)
{
    const QFontMetrics& fm = fontMetrics();
    const qint32 fontHeight = fm.lineSpacing();
    constexpr qint32 topLineYOffset = 0;

    qint32 yOffset = topLineYOffset - m_firstLine * fontHeight;
    if(yOffset > y)
        return LineRef::invalid;

    const LineRef line = std::min((y - yOffset) / fontHeight, m_nofLines - 1);
    return line;
}

void MergeResultWindow::mousePressEvent(QMouseEvent* e)
{
    m_bCursorOn = true;

    const qint32 xOffset = getTextXOffset();

    const LineRef line = std::max<LineRef::LineType>(convertToLine(e->y()), 0);
    const QString s = getString(line);
    QTextLayout textLayout(s, font(), this);
    getTextLayoutForLine(line, s, textLayout);
    qint32 pos = textLayout.lineAt(0).xToCursor(e->x() - textLayout.position().x());

    const bool lLeftMouseButton = e->button() == Qt::LeftButton;
    const bool lMiddleMouseButton = e->button() == Qt::MiddleButton;
    const bool lRightMouseButton = e->button() == Qt::RightButton;

    if((lLeftMouseButton && (e->x() < xOffset)) || lRightMouseButton) // Fast range selection
    {
        m_cursorXPos = 0;
        m_cursorOldXPixelPos = 0;
        m_cursorYPos = line;
        LineCount l = 0;
        MergeLineListImp::iterator i = m_mergeLineList.list().begin();
        for(i = m_mergeLineList.list().begin(); i != m_mergeLineList.list().end(); ++i)
        {
            if(l == line)
                break;

            l += i->lineCount();
            if(l > line)
                break;
        }
        m_selection.reset(); // Disable current selection

        m_bCursorOn = true;
        setFastSelector(i);

        if(lRightMouseButton)
        {
            Q_EMIT showPopupMenu(QCursor::pos());
        }
    }
    else if(lLeftMouseButton) // Normal cursor placement
    {
        pos = std::max(pos, 0);
        if(e->modifiers() & Qt::ShiftModifier)
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
        m_cursorXPixelPos = qCeil(textLayout.lineAt(0).cursorToX(pos));
        if(m_pOptions->m_bRightToLeftLanguage)
            m_cursorXPixelPos += qCeil(textLayout.position().x() - m_horizScrollOffset);
        m_cursorOldXPixelPos = m_cursorXPixelPos;
        m_cursorYPos = line;

        update();
    }
    else if(lMiddleMouseButton) // Paste clipboard
    {
        pos = std::max(pos, 0);

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
        const LineRef line = convertToLine(e->y());
        const QString s = getString(line);
        QTextLayout textLayout(s, font(), this);
        getTextLayoutForLine(line, s, textLayout);
        qint32 pos = textLayout.lineAt(0).xToCursor(e->x() - textLayout.position().x());
        m_cursorXPos = pos;
        m_cursorOldXPixelPos = m_cursorXPixelPos;
        m_cursorYPos = line;

        if(!s.isEmpty())
        {
            QtSizeType pos1, pos2;

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
    const LineRef line = convertToLine(e->y());
    const QString s = getString(line);
    QTextLayout textLayout(s, font(), this);
    getTextLayoutForLine(line, s, textLayout);
    const qint32 pos = textLayout.lineAt(0).xToCursor(e->x() - textLayout.position().x());
    m_cursorXPos = pos;
    m_cursorOldXPixelPos = m_cursorXPixelPos;
    m_cursorYPos = line;
    if(m_selection.isValidFirstLine())
    {
        m_selection.end(line, pos);
        myUpdate(0);

        // Scroll because mouse moved out of the window
        const QFontMetrics& fm = fontMetrics();
        const qint32 fontWidth = Utils::getHorizontalAdvance(fm, '0');
        constexpr qint32 topLineYOffset = 0;
        qint32 deltaX = 0;
        qint32 deltaY = 0;
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
        constexpr qint32 topLineYOffset = 0;
        qint32 yOffset = (m_cursorYPos - m_firstLine) * fm.lineSpacing() + topLineYOffset;

        repaint(0, yOffset, width(), fm.lineSpacing() + 2);

        m_bCursorUpdate = false;
    }

    m_cursorTimer.start(500);
}

void MergeResultWindow::wheelEvent(QWheelEvent* pWheelEvent)
{
    const QPoint delta = pWheelEvent->angleDelta();
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
    qint32 y = m_cursorYPos;
    MergeLineListImp::iterator mlIt;
    MergeEditLineList::iterator melIt;
    if(!calcIteratorFromLineNr(y, mlIt, melIt))
    {
        // no data loaded or y out of bounds
        e->ignore();
        return;
    }

    QString str = melIt->getString(m_pldA, m_pldB, m_pldC);
    qint32 x = m_cursorXPos;

    QTextLayout textLayoutOrig(str, font(), this);
    getTextLayoutForLine(y, str, textLayoutOrig);

    bool bCtrl = (e->modifiers() & Qt::ControlModifier) != 0;
    bool bShift = (e->modifiers() & Qt::ShiftModifier) != 0;
#ifdef Q_OS_WIN
    bool bAlt = (e->modifiers() & Qt::AltModifier) != 0;
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
        case Qt::Key_Backtab:
            break;
        case Qt::Key_Delete: {
            if(deleteSelection2(str, x, y, mlIt, melIt) || !melIt->isEditableText()) break;

            if(x >= str.length())
            {
                if(y < m_nofLines - 1)
                {
                    setModified();
                    MergeLineListImp::iterator mlIt1;
                    MergeEditLineList::iterator melIt1;
                    if(calcIteratorFromLineNr(y + 1, mlIt1, melIt1) && melIt1->isEditableText())
                    {
                        QString s2 = melIt1->getString(m_pldA, m_pldB, m_pldC);
                        melIt->setString(str + s2);

                        // Remove the line
                        if(mlIt1->lineCount() > 1)
                            mlIt1->list().erase(melIt1);
                        else
                            melIt1->setRemoved();
                    }
                }
            }
            else
            {
                QString s = str.left(x);
                s += QStringView(str).mid(x + 1);
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
                    MergeLineListImp::iterator mlIt1;
                    MergeEditLineList::iterator melIt1;
                    if(calcIteratorFromLineNr(y - 1, mlIt1, melIt1) && melIt1->isEditableText())
                    {
                        QString s1 = melIt1->getString(m_pldA, m_pldB, m_pldC);
                        melIt1->setString(s1 + str);

                        // Remove the previous line
                        if(mlIt->lineCount() > 1)
                            mlIt->list().erase(melIt);
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
                s += QStringView(str).mid(x);
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
                MergeLineListImp::iterator mlIt1 = mlIt;
                MergeEditLineList::iterator melIt1 = melIt;
                for(;;)
                {
                    const QString s = melIt1->getString(m_pldA, m_pldB, m_pldC);
                    if(!s.isEmpty())
                    {
                        qint32 i;
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
                    if(melIt1 != mlIt1->list().begin())
                        --melIt1;
                    else
                    {
                        if(mlIt1 == m_mergeLineList.list().begin()) break;
                        --mlIt1;
                        melIt1 = mlIt1->list().end();
                        --melIt1;
                    }
                }
            }
            MergeEditLine mel(mlIt->id3l()); // Associate every mel with an id3l, even if not really valid.
            mel.setString(indentation + str.mid(x));

            if(x < str.length()) // Cut off the old line.
            {
                // Since ps possibly points into melIt->str, first copy it into a temporary.
                QString temp = str.left(x);
                melIt->setString(temp);
            }

            ++melIt;
            mlIt->list().insert(melIt, mel);
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
            x = TYPE_MAX(qint32);
            if(bCtrl)
            {
                y = TYPE_MAX(qint32);
            }
            break;

        case Qt::Key_Left:
        case Qt::Key_Right:
            if((e->key() == Qt::Key_Left) != m_pOptions->m_bRightToLeftLanguage)
            {
                if(!bCtrl)
                {
                    qint32 newX = textLayoutOrig.previousCursorPosition(x);
                    if(newX == x && y > 0)
                    {
                        --y;
                        x = TYPE_MAX(qint32);
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
                        qint32 newX = textLayoutOrig.previousCursorPosition(x);
                        if(newX == x) break;
                        x = newX;
                    }
                    while(x > 0 && (str[x - 1] != ' ' && str[x - 1] != '\t'))
                    {
                        qint32 newX = textLayoutOrig.previousCursorPosition(x);
                        if(newX == x) break;
                        x = newX;
                    }
                }
            }
            else
            {
                if(!bCtrl)
                {
                    qint32 newX = textLayoutOrig.nextCursorPosition(x);
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
                        qint32 newX = textLayoutOrig.nextCursorPosition(x);
                        if(newX == x) break;
                        x = newX;
                    }
                    while(x < str.length() && (str[x] != ' ' && str[x] != '\t'))
                    {
                        qint32 newX = textLayoutOrig.nextCursorPosition(x);
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
            if(t.isEmpty() || bCtrl || !melIt->isEditableText())
            {
                e->ignore();
                return;
            }

            deleteSelection2(str, x, y, mlIt, melIt);

            setModified();
            // Characters to insert
            QString s = str;
            if(t[0] == '\t' && m_pOptions->m_bReplaceTabs)
            {
                qint32 spaces = (m_cursorXPos / m_pOptions->m_tabSize + 1) * m_pOptions->m_tabSize - m_cursorXPos;
                t.fill(' ', spaces);
            }
            if(m_bInsertMode)
                s.insert(x, t);
            else
                s.replace(x, t.length(), t);

            melIt->setString(s);
            x += t.length();
            bShift = false;
        } // default case
    } // switch(e->key())


    y = qBound(0, y, m_nofLines - 1);

    str = calcIteratorFromLineNr(y, mlIt, melIt)
            ? melIt->getString(m_pldA, m_pldB, m_pldC)
            : QString();

    x = qBound(0, x, (qint32)str.length());

    qint32 newFirstLine = m_firstLine;
    qint32 newHorizScrollOffset = m_horizScrollOffset;

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

    m_cursorXPixelPos = qCeil(textLayout.lineAt(0).cursorToX(x));
    qint32 hF = 1; // horizontal factor
    if(m_pOptions->m_bRightToLeftLanguage)
    {
        m_cursorXPixelPos += qCeil(textLayout.position().x() - m_horizScrollOffset);
        hF = -1;
    }
    qint32 cursorWidth = 5;
    if(m_cursorXPixelPos < hF * m_horizScrollOffset)
        newHorizScrollOffset = hF * m_cursorXPixelPos;
    else if(m_cursorXPixelPos > hF * m_horizScrollOffset + getVisibleTextAreaWidth() - cursorWidth)
        newHorizScrollOffset = hF * (m_cursorXPixelPos - (getVisibleTextAreaWidth() - cursorWidth));

    qint32 newCursorX = x;
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
        m_maxTextWidth = qCeil(textLayout.maximumWidth());
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

/**
 * Determine MergeLine and MergeEditLine from line number
 *
 * @param       line
 *              line number to look up
 * @param[out]  mlIt
 *              iterator to merge-line
 *              or m_mergeLineList.end() if not available
 * @param[out]  melIt
 *              iterator to MergeEditLine
 *              or mlIt->mergeEditLineList.end() if not available
 *              @warning uninitialized if mlIt is not available!
 * @return      whether line is available;
 *              when true, @p mlIt and @p melIt are set to valid iterators
 */
bool MergeResultWindow::calcIteratorFromLineNr(
    LineRef::LineType line,
    MergeLineListImp::iterator& mlIt,
    MergeEditLineList::iterator& melIt)
{
    for(mlIt = m_mergeLineList.list().begin(); mlIt != m_mergeLineList.list().end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        if(line > ml.lineCount())
        {
            line -= ml.lineCount();
        }
        else
        {
            for(melIt = ml.list().begin(); melIt != ml.list().end(); ++melIt)
            {
                --line;
                if(line <= LineRef::invalid) return true;
            }
        }
    }
    return false;
}

QString MergeResultWindow::getSelection() const
{
    QString selectionString;

    LineRef line = 0;
    for(const MergeLine& ml: m_mergeLineList.list())
    {
        for(const MergeEditLine& mel: ml.list())
        {
            if(m_selection.lineWithin(line))
            {
                qint32 outPos = 0;
                if(mel.isEditableText())
                {
                    const QString str = mel.getString(m_pldA, m_pldB, m_pldC);

                    // Consider tabs
                    for(qint32 i = 0; i < str.length(); ++i)
                    {
                        qint32 spaces = 1;
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

bool MergeResultWindow::deleteSelection2(QString& s, qint32& x, qint32& y,
                                         MergeLineListImp::iterator& mlIt, MergeEditLineList::iterator& melIt)
{
    if(m_selection.selectionContainsData())
    {
        assert(m_selection.isValidFirstLine());
        deleteSelection();
        y = m_cursorYPos;
        if(!calcIteratorFromLineNr(y, mlIt, melIt))
        {
            // deleteSelection() should never remove or empty the first line, so
            // resolving m_cursorYPos shall always succeed
            assert(false);
        }

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
    assert(m_selection.isValidFirstLine());

    setModified();

    LineRef line = 0;
    MergeLineListImp::iterator mlItFirst;
    MergeEditLineList::iterator melItFirst;
    QString firstLineString;

    LineRef firstLine;
    LineRef lastLine;

    for(const MergeLine& ml: m_mergeLineList.list())
    {
        for(const MergeEditLine& mel: ml.list())
        {
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

    MergeLineListImp::iterator mlIt;
    line = 0;
    for(mlIt = m_mergeLineList.list().begin(); mlIt != m_mergeLineList.list().end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        MergeEditLineList::iterator melIt, melIt1;
        for(melIt = ml.list().begin(); melIt != ml.list().end();)
        {
            const MergeEditLine& mel = *melIt;
            melIt1 = melIt;
            ++melIt1;

            if(mel.isEditableText() && m_selection.lineWithin(line))
            {
                const QString lineString = mel.getString(m_pldA, m_pldB, m_pldC);

                qint32 firstPosInLine = m_selection.firstPosInLine(line);
                qint32 lastPosInLine = m_selection.lastPosInLine(line);

                if(line == firstLine)
                {
                    mlItFirst = mlIt;
                    melItFirst = melIt;
                    qint32 pos = firstPosInLine;
                    firstLineString = lineString.left(pos);
                }

                if(line == lastLine)
                {
                    // This is the last line in the selection
                    qint32 pos = lastPosInLine;
                    firstLineString += QStringView(lineString).mid(pos); // rest of line
                    melItFirst->setString(firstLineString);
                }

                if(line != firstLine || (m_selection.endPos() - m_selection.beginPos()) == lineString.length())
                {
                    // Remove the line
                    if(ml.lineCount() > 1)
                        ml.list().erase(melIt);
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

    qint32 y = m_cursorYPos;
    MergeLineListImp::iterator mlIt;
    MergeEditLineList::iterator melIt, melItAfter;
    if (!calcIteratorFromLineNr(y, mlIt, melIt))
    {
        return;
    }
    melItAfter = melIt;
    ++melItAfter;
    QString str = melIt->getString(m_pldA, m_pldB, m_pldC);
    qint32 x = m_cursorXPos;

    if(!QApplication::clipboard()->supportsSelection())
        bFromSelection = false;

    QString clipBoard = QApplication::clipboard()->text(bFromSelection ? QClipboard::Selection : QClipboard::Clipboard);

    QString currentLine = str.left(x);
    QString endOfLine = str.mid(x);
    qint32 i;
    qint32 len = clipBoard.length();
    for(i = 0; i < len; ++i)
    {
        QChar c = clipBoard[i];
        if(c == '\n' || (c == '\r' && clipBoard[i + 1] != '\n'))
        {
            melIt->setString(currentLine);
            MergeEditLine mel(mlIt->id3l()); // Associate every mel with an id3l, even if not really valid.
            melIt = mlIt->list().insert(melItAfter, mel);
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
    if(getNumberOfUnsolvedConflicts() > 0)
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

    // Determine the line feed for this file
    const QString lineFeed(eLineEndStyle == eLineEndStyleDos ? QString("\r\n") : QString("\n"));

    bool isFirstLine = true;
    for(const MergeLine& ml: m_mergeLineList.list())
    {
        for(const MergeEditLine& mel: ml.list())
        {
            if(mel.isEditableText())
            {
                const QString str = mel.getString(m_pldA, m_pldB, m_pldC);

                if(!isFirstLine && !mel.isRemoved())
                {
                    // Put line feed between lines, but not for the first line
                    // or between lines that have been removed (because there
                    // isn't a line there).
                    textOutStream << lineFeed;
                }

                if(isFirstLine)
                    isFirstLine = mel.isRemoved();

                textOutStream << str;
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

QString MergeResultWindow::getString(qint32 lineIdx)
{
    MergeLineListImp::iterator mlIt;
    MergeEditLineList::iterator melIt;
    if(!calcIteratorFromLineNr(lineIdx, mlIt, melIt))
    {
        return QString();
    }
    return melIt->getString(m_pldA, m_pldB, m_pldC);
}

bool MergeResultWindow::findString(const QString& s, LineRef& d3vLine, qint32& posInLine, bool bDirDown, bool bCaseSensitive)
{
    qint32 it = d3vLine;
    qint32 endIt = bDirDown ? getNofLines() : -1;
    qint32 step = bDirDown ? 1 : -1;
    qint32 startPos = posInLine;

    for(; it != endIt; it += step)
    {
        QString line = getString(it);
        if(!line.isEmpty())
        {
            qint32 pos = line.indexOf(s, startPos, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
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

void MergeResultWindow::setSelection(qint32 firstLine, qint32 startPos, qint32 lastLine, qint32 endPos)
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

void MergeResultWindow::scrollVertically(qint32 deltaY)
{
    mVScrollBar->setValue(mVScrollBar->value()  + deltaY);
}

WindowTitleWidget::WindowTitleWidget(const QSharedPointer<Options>& pOptions)
{
    m_pOptions = pOptions;
    setAutoFillBackground(true);

    QHBoxLayout* pHLayout = new QHBoxLayout(this);
    pHLayout->setContentsMargins(2, 2, 2, 2);
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
    //chk_connect( m_pBrowseButton, &QPushButton::clicked), this, &MergeResultWindow::slotBrowseButtonClicked);

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

    m_pLineEndStyleSelector->addItem(i18n("Unix") + (unxUsers.isEmpty() ? QString("") : u8" (" + unxUsers + u8")"));
    m_pLineEndStyleSelector->addItem(i18n("DOS") + (dosUsers.isEmpty() ? QString("") : u8" (" + dosUsers + u8")"));

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

    qint32 current = m_pLineEndStyleSelector->currentIndex();
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
    const QList<qint32> mibs = QTextCodec::availableMibs();
    for(qint32 i: mibs)
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

    std::map<QString, QTextCodec*>::const_iterator it;
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
    qint32 idx = m_pEncodingSelector->findText(QLatin1String(pEncoding->name()));
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

        QColor c1 = m_pOptions->forgroundColor();
        QColor c2 = Qt::lightGray;
        if(e->type() == QEvent::FocusOut)
            c2 = m_pOptions->backgroundColor();

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
