// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#include "defmac.h"
#include "difftextwindow.h"
#include "DirectoryInfo.h"
#include "directorymergewindow.h"
#include "fileaccess.h"
#include "kdiff3.h"
#include "Logging.h"
#include "optiondialog.h"
#include "progress.h"
#include "Utils.h"

#include "mergeresultwindow.h"
#include "smalldialogs.h"

#include <algorithm>
#include <cstdio>
#include <list>

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QEvent> // QKeyEvent, QDropEvent, QInputEvent
#include <QFile>
#include <QLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QPointer>
#include <QProcess>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QTextCodec>
#include <QUrl>

#include <KLocalizedString>
#include <KMessageBox>
#include <KParts/MainWindow>
#include <KShortcutsDialog>

void KDiff3App::mainInit(TotalDiffStatus* pTotalDiffStatus, const InitFlags inFlags)
{
    ProgressScope pp;
    QStringList errors;
    // When doing a full analysis in the directory-comparison, then the statistics-results
    // will be stored in the given TotalDiffStatus. Otherwise pTotalDiffStatus will
    // point to m_totalDiffStatus.
    bool bLoadFiles = inFlags & InitFlag::loadFiles;
    bool bUseCurrentEncoding = inFlags & InitFlag::useCurrentEncoding;
    bool bAutoSolve = inFlags & InitFlag::autoSolve;

    bool bGUI = (inFlags & InitFlag::initGUI);

    IgnoreFlags eIgnoreFlags = IgnoreFlag::none;
    if(m_pOptions->ignoreComments())
        eIgnoreFlags |= IgnoreFlag::ignoreComments;

    if(m_pOptions->whiteSpaceIsEqual())
        eIgnoreFlags |= IgnoreFlag::ignoreWhiteSpace;

    assert(pTotalDiffStatus != nullptr);

    bool bVisibleMergeResultWindow = !m_outputFilename.isEmpty();

    //Easier to do here then have all eleven of our call points due the check.
    if(m_sd1->isEmpty() && m_sd2->isEmpty() && m_sd3->isEmpty())
        bLoadFiles = false;

    if(bGUI)
    {
        if(bVisibleMergeResultWindow && !m_pOptions->m_PreProcessorCmd.isEmpty())
        {
            QString msg = "- " + i18n("PreprocessorCmd: ") + m_pOptions->m_PreProcessorCmd + '\n';
            qint32 result = KMessageBox::warningYesNo(this,
                i18n("The following option(s) you selected might change data:\n") + msg +
                    i18n("\nMost likely this is not wanted during a merge.\n"
                         "Do you want to disable these settings or continue with these settings active?"),
                i18n("Option Unsafe for Merging"),
                KGuiItem(i18n("Use These Options During Merge")),
                KGuiItem(i18n("Disable Unsafe Options")));

            if(result == KMessageBox::No)
            {
                m_pOptions->m_PreProcessorCmd = "";
            }
        }

        // Because of the progressdialog paintevents can occur, but data is invalid,
        // so painting must be suppressed
        setLockPainting(true);
    }

    //insure merge result window never has stale iterators/[poiners.
    if(m_pMergeResultWindow) m_pMergeResultWindow->reset();
    //Clear stale pointers in DiffTextWindow.
    if(m_pDiffTextWindow1) m_pDiffTextWindow1->reset();
    if(m_pDiffTextWindow2) m_pDiffTextWindow2->reset();
    if(m_pDiffTextWindow3) m_pDiffTextWindow3->reset();
    m_diff3LineList.clear();
    mDiff3LineVector.clear();

    if(bLoadFiles)
    {
        m_manualDiffHelpList.clear();

        if(m_sd3->isEmpty())
            ProgressProxy::setMaxNofSteps(4); // Read 2 files, 1 comparison, 1 finediff
        else
            ProgressProxy::setMaxNofSteps(9); // Read 3 files, 3 comparisons, 3 finediffs

        // First get all input data.
        ProgressProxy::setInformation(i18n("Loading A"));
        qCInfo(kdiffMain) << i18n("Loading A: %1", m_sd1->getFilename());

        if(bUseCurrentEncoding)
            m_sd1->readAndPreprocess(m_sd1->getEncoding(), false);
        else
            m_sd1->readAndPreprocess(m_pOptions->m_pEncodingA, m_pOptions->m_bAutoDetectUnicodeA);

        ProgressProxy::step();

        ProgressProxy::setInformation(i18n("Loading B"));
        qCInfo(kdiffMain) << i18n("Loading B: %1", m_sd2->getFilename());

        if(bUseCurrentEncoding)
            m_sd2->readAndPreprocess(m_sd2->getEncoding(), false);
        else
            m_sd2->readAndPreprocess(m_pOptions->m_pEncodingB, m_pOptions->m_bAutoDetectUnicodeB);

        ProgressProxy::step();
        errors.append(m_sd1->getErrors());
        errors.append(m_sd2->getErrors());
    }
    else
    {
        if(m_sd3->isEmpty())
            ProgressProxy::setMaxNofSteps(2); // 1 comparison, 1 finediff
        else
            ProgressProxy::setMaxNofSteps(6); // 3 comparisons, 3 finediffs
    }

    pTotalDiffStatus->reset();

    if(errors.isEmpty())
    {
        // Run the diff.
        if(m_sd3->isEmpty())
        {
            pTotalDiffStatus->setBinaryEqualAB(m_sd1->isBinaryEqualWith(m_sd2));

            if(m_sd1->isText() && m_sd2->isText())
            {
                ProgressProxy::setInformation(i18n("Diff: A <-> B"));
                qCInfo(kdiffMain) << i18n("Diff: A <-> B");
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_diffList12, e_SrcSelector::A, e_SrcSelector::B,
                                             m_pOptionDialog->getOptions());

                ProgressProxy::step();

                ProgressProxy::setInformation(i18n("Linediff: A <-> B"));
                qCInfo(kdiffMain) << i18n("Linediff: A <-> B");
                m_diff3LineList.calcDiff3LineListUsingAB(&m_diffList12);

                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay(), eIgnoreFlags));
                if(m_sd1->getSizeBytes() == 0) pTotalDiffStatus->setTextEqualAB(false);

                ProgressProxy::step();
            }
            else
            {
                ProgressProxy::step();
                ProgressProxy::step();
            }
        }
        else
        {
            if(bLoadFiles)
            {
                ProgressProxy::setInformation(i18n("Loading C"));
                qCInfo(kdiffMain) << i18n("Loading C: %1", m_sd3->getFilename());

                if(bUseCurrentEncoding)
                    m_sd3->readAndPreprocess(m_sd3->getEncoding(), false);
                else
                    m_sd3->readAndPreprocess(m_pOptions->m_pEncodingC, m_pOptions->m_bAutoDetectUnicodeC);

                ProgressProxy::step();
            }

            pTotalDiffStatus->setBinaryEqualAB(m_sd1->isBinaryEqualWith(m_sd2));
            pTotalDiffStatus->setBinaryEqualAC(m_sd1->isBinaryEqualWith(m_sd3));
            pTotalDiffStatus->setBinaryEqualBC(m_sd3->isBinaryEqualWith(m_sd2));

            ProgressProxy::setInformation(i18n("Diff: A <-> B"));
            qCInfo(kdiffMain) << i18n("Diff: A <-> B");

            if(m_sd1->isText() && m_sd2->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_diffList12, e_SrcSelector::A, e_SrcSelector::B,
                                             m_pOptionDialog->getOptions());

                m_diff3LineList.calcDiff3LineListUsingAB(&m_diffList12);
            }
            ProgressProxy::step();

            ProgressProxy::setInformation(i18n("Diff: A <-> C"));
            qCInfo(kdiffMain) << i18n("Diff: A <-> C");

            if(m_sd1->isText() && m_sd3->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd3->getLineDataForDiff(), m_sd3->getSizeLines(), m_diffList13, e_SrcSelector::A, e_SrcSelector::C,
                                             m_pOptionDialog->getOptions());

                m_diff3LineList.calcDiff3LineListUsingAC(&m_diffList13);
                m_diff3LineList.correctManualDiffAlignment(&m_manualDiffHelpList);
                m_diff3LineList.calcDiff3LineListTrim(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff(), &m_manualDiffHelpList);
            }
            ProgressProxy::step();

            ProgressProxy::setInformation(i18n("Diff: B <-> C"));
            qCInfo(kdiffMain) << i18n("Diff: B <-> C");

            if(m_sd2->isText() && m_sd3->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_sd3->getLineDataForDiff(), m_sd3->getSizeLines(), m_diffList23, e_SrcSelector::B, e_SrcSelector::C,
                                             m_pOptionDialog->getOptions());
                if(m_pOptions->m_bDiff3AlignBC)
                {
                    m_diff3LineList.calcDiff3LineListUsingBC(&m_diffList23);
                    m_diff3LineList.correctManualDiffAlignment(&m_manualDiffHelpList);
                    m_diff3LineList.calcDiff3LineListTrim(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff(), &m_manualDiffHelpList);
                }
            }
            ProgressProxy::step();

            if(!m_pOptions->m_bDiff3AlignBC)
            {
                m_diff3LineList.debugLineCheck(m_sd1->getSizeLines(), e_SrcSelector::A);
                m_diff3LineList.debugLineCheck(m_sd2->getSizeLines(), e_SrcSelector::B);
                m_diff3LineList.debugLineCheck(m_sd3->getSizeLines(), e_SrcSelector::C);
            }

            ProgressProxy::setInformation(i18n("Linediff: A <-> B"));
            qCInfo(kdiffMain) << i18n("Linediff: A <-> B");
            if(m_sd1->hasData() && m_sd2->hasData() && m_sd1->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay(), eIgnoreFlags));
            ProgressProxy::step();

            ProgressProxy::setInformation(i18n("Linediff: B <-> C"));
            qCInfo(kdiffMain) << i18n("Linediff: B <-> C");
            if(m_sd2->hasData() && m_sd3->hasData() && m_sd2->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualBC(m_diff3LineList.fineDiff(e_SrcSelector::B, m_sd2->getLineDataForDisplay(), m_sd3->getLineDataForDisplay(), eIgnoreFlags));
            ProgressProxy::step();

            ProgressProxy::setInformation(i18n("Linediff: A <-> C"));
            qCInfo(kdiffMain) << i18n("Linediff: A <-> C");
            if(m_sd1->hasData() && m_sd3->hasData() && m_sd1->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualAC(m_diff3LineList.fineDiff(e_SrcSelector::C, m_sd3->getLineDataForDisplay(), m_sd1->getLineDataForDisplay(), eIgnoreFlags));
            if(!m_pOptions->m_bDiff3AlignBC)
            {
                m_diff3LineList.debugLineCheck(m_sd2->getSizeLines(), e_SrcSelector::B);
                m_diff3LineList.debugLineCheck(m_sd3->getSizeLines(), e_SrcSelector::C);
            }

            ProgressProxy::setInformation(i18n("Linediff: A <-> B"));
            if(m_sd1->hasData() && m_sd2->hasData() && m_sd1->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay(), eIgnoreFlags));
            ProgressProxy::step();

            ProgressProxy::setInformation(i18n("Linediff: B <-> C"));
            if(m_sd3->hasData() && m_sd2->hasData() && m_sd3->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualBC(m_diff3LineList.fineDiff(e_SrcSelector::B, m_sd2->getLineDataForDisplay(), m_sd3->getLineDataForDisplay(), eIgnoreFlags));
            ProgressProxy::step();

            ProgressProxy::setInformation(i18n("Linediff: A <-> C"));
            if(m_sd1->hasData() && m_sd3->hasData() && m_sd1->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualAC(m_diff3LineList.fineDiff(e_SrcSelector::C, m_sd3->getLineDataForDisplay(), m_sd1->getLineDataForDisplay(), eIgnoreFlags));
            ProgressProxy::step();
            if(m_sd1->getSizeBytes() == 0)
            {
                pTotalDiffStatus->setTextEqualAB(false);
                pTotalDiffStatus->setTextEqualAC(false);
            }
            if(m_sd2->getSizeBytes() == 0)
            {
                pTotalDiffStatus->setTextEqualAB(false);
                pTotalDiffStatus->setTextEqualBC(false);
            }

            errors.append(m_sd3->getErrors());
        }
    }
    else
    {
        ProgressProxy::clear();
    }

    if(errors.isEmpty() && m_sd1->isText() && m_sd2->isText())
    {
        Diff3Line::m_pDiffBufferInfo->init(&m_diff3LineList,
                               m_sd1->getLineDataForDiff(),
                               m_sd2->getLineDataForDiff(),
                               m_sd3->getLineDataForDiff());

        m_diff3LineList.calcWhiteDiff3Lines(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff(), m_pOptions->ignoreComments());
        m_diff3LineList.calcDiff3LineVector(mDiff3LineVector);
    }

    // Calc needed lines for display
    if(m_diff3LineList.size() <= TYPE_MAX(qint32))
        m_neededLines = SafeInt<qint32>(m_diff3LineList.size());
    else
    {
        errors.append("Too many lines in diff. Skiping file.");
    }

    //initView does first time setup for ui. Why is called it here?
    initView();
    m_pMergeResultWindow->connectActions();

    m_pMainWidget->setVisible(bGUI);

    m_bTripleDiff = !m_sd3->isEmpty();

    m_pMergeResultWindowTitle->setEncodings(m_sd1->getEncoding(), m_sd2->getEncoding(), m_sd3->getEncoding());
    if(!m_pOptions->m_bAutoSelectOutEncoding)
        m_pMergeResultWindowTitle->setEncoding(m_pOptions->m_pEncodingOut);

    m_pMergeResultWindowTitle->setLineEndStyles(m_sd1->getLineEndStyle(), m_sd2->getLineEndStyle(), m_sd3->getLineEndStyle());

    if(bGUI)
    {
        const ManualDiffHelpList* pMDHL = &m_manualDiffHelpList;
        m_pDiffTextWindow1->init(m_sd1->getAliasName(), m_sd1->getEncoding(), m_sd1->getLineEndStyle(),
                                 m_sd1->getLineDataForDisplay(), m_sd1->getSizeLines(), &mDiff3LineVector, pMDHL);
        m_pDiffTextWindowFrame1->init();

        m_pDiffTextWindow2->init(m_sd2->getAliasName(), m_sd2->getEncoding(), m_sd2->getLineEndStyle(),
                                 m_sd2->getLineDataForDisplay(), m_sd2->getSizeLines(), &mDiff3LineVector, pMDHL);
        m_pDiffTextWindowFrame2->init();

        m_pDiffTextWindow3->init(m_sd3->getAliasName(), m_sd3->getEncoding(), m_sd3->getLineEndStyle(),
                                 m_sd3->getLineDataForDisplay(), m_sd3->getSizeLines(), &mDiff3LineVector, pMDHL);
        m_pDiffTextWindowFrame3->init();

        m_pDiffTextWindowFrame3->setVisible(m_bTripleDiff);
    }

    m_bOutputModified = bVisibleMergeResultWindow;

    m_pMergeResultWindow->init(
        m_sd1->getLineDataForDisplay(), m_sd1->getSizeLines(),
        m_sd2->getLineDataForDisplay(), m_sd2->getSizeLines(),
        m_bTripleDiff ? m_sd3->getLineDataForDisplay() : nullptr, m_sd3->getSizeLines(),
        &m_diff3LineList,
        pTotalDiffStatus, bAutoSolve);
    m_pMergeResultWindowTitle->setFileName(m_outputFilename.isEmpty() ? QString("unnamed.txt") : m_outputFilename);

    if(bGUI)
    {
        m_pOverview->init(&m_diff3LineList);
        DiffTextWindow::mVScrollBar->setValue(0);
        m_pHScrollBar->setValue(0);
        MergeResultWindow::mVScrollBar->setValue(0);
        setLockPainting(false);

        if(!bVisibleMergeResultWindow)
            m_pMergeWindowFrame->hide();
        else
            m_pMergeWindowFrame->show();

        // Try to create a meaningful but not too long caption
        if(!isPart() && errors.isEmpty())
        {
            createCaption();
        }
        m_bFinishMainInit = true; // call slotFinishMainInit after finishing the word wrap
        m_bLoadFiles = bLoadFiles;
        postRecalcWordWrap();
    }
}

void KDiff3App::setLockPainting(bool bLock)
{
    if(m_pDiffTextWindow1) m_pDiffTextWindow1->setPaintingAllowed(!bLock);
    if(m_pDiffTextWindow2) m_pDiffTextWindow2->setPaintingAllowed(!bLock);
    if(m_pDiffTextWindow3) m_pDiffTextWindow3->setPaintingAllowed(!bLock);
    if(m_pOverview) m_pOverview->setPaintingAllowed(!bLock);
    if(m_pMergeResultWindow) m_pMergeResultWindow->setPaintingAllowed(!bLock);
}

void KDiff3App::createCaption()
{
    // Try to create a meaningful but not too long caption
    // 1. If the filenames are equal then show only one filename
    QString caption;
    QString f1 = m_sd1->getAliasName();
    QString f2 = m_sd2->getAliasName();
    QString f3 = m_sd3->getAliasName();
    qint32 p;

    if((p = f1.lastIndexOf('/')) >= 0 || (p = f1.lastIndexOf('\\')) >= 0)
        f1 = f1.mid(p + 1);
    if((p = f2.lastIndexOf('/')) >= 0 || (p = f2.lastIndexOf('\\')) >= 0)
        f2 = f2.mid(p + 1);
    if((p = f3.lastIndexOf('/')) >= 0 || (p = f3.lastIndexOf('\\')) >= 0)
        f3 = f3.mid(p + 1);

    if(!f1.isEmpty())
    {
        if((f2.isEmpty() && f3.isEmpty()) ||
           (f2.isEmpty() && f1 == f3) || (f3.isEmpty() && f1 == f2) || (f1 == f2 && f1 == f3))
            caption = f1;
    }
    else if(!f2.isEmpty())
    {
        if(f3.isEmpty() || f2 == f3)
            caption = f2;
    }
    else if(!f3.isEmpty())
        caption = f3;

    // 2. If the files don't have the same name then show all names
    if(caption.isEmpty() && (!f1.isEmpty() || !f2.isEmpty() || !f3.isEmpty()))
    {
        caption = (f1.isEmpty() ? QString("") : f1);
        caption += QLatin1String(caption.isEmpty() || f2.isEmpty() ? "" : " <-> ") + (f2.isEmpty() ? QString("") : f2);
        caption += QLatin1String(caption.isEmpty() || f3.isEmpty() ? "" : " <-> ") + (f3.isEmpty() ? QString("") : f3);
    }

    m_pKDiff3Shell->setWindowTitle(caption.isEmpty() ? QString("KDiff3") : caption + QString(" - KDiff3"));
}

void KDiff3App::setHScrollBarRange()
{
    qint32 w1 = m_pDiffTextWindow1 != nullptr && m_pDiffTextWindow1->isVisible() ? m_pDiffTextWindow1->getMaxTextWidth() : 0;
    qint32 w2 = m_pDiffTextWindow2 != nullptr && m_pDiffTextWindow2->isVisible() ? m_pDiffTextWindow2->getMaxTextWidth() : 0;
    qint32 w3 = m_pDiffTextWindow3 != nullptr && m_pDiffTextWindow3->isVisible() ? m_pDiffTextWindow3->getMaxTextWidth() : 0;

    qint32 wm = m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isVisible() ? m_pMergeResultWindow->getMaxTextWidth() : 0;

    qint32 v1 = m_pDiffTextWindow1 != nullptr && m_pDiffTextWindow1->isVisible() ? m_pDiffTextWindow1->getVisibleTextAreaWidth() : 0;
    qint32 v2 = m_pDiffTextWindow2 != nullptr && m_pDiffTextWindow2->isVisible() ? m_pDiffTextWindow2->getVisibleTextAreaWidth() : 0;
    qint32 v3 = m_pDiffTextWindow3 != nullptr && m_pDiffTextWindow3->isVisible() ? m_pDiffTextWindow3->getVisibleTextAreaWidth() : 0;
    qint32 vm = m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isVisible() ? m_pMergeResultWindow->getVisibleTextAreaWidth() : 0;

    // Find the minimum, but don't consider 0.
    qint32 pageStep = 0;
    if((pageStep == 0 || pageStep > v1) && v1 > 0)
        pageStep = v1;
    if((pageStep == 0 || pageStep > v2) && v2 > 0)
        pageStep = v2;
    if((pageStep == 0 || pageStep > v3) && v3 > 0)
        pageStep = v3;
    if((pageStep == 0 || pageStep > vm) && vm > 0)
        pageStep = vm;

    qint32 rangeMax = 0;
    if(w1 > v1 && w1 - v1 > rangeMax && v1 > 0)
        rangeMax = w1 - v1;
    if(w2 > v2 && w2 - v2 > rangeMax && v2 > 0)
        rangeMax = w2 - v2;
    if(w3 > v3 && w3 - v3 > rangeMax && v3 > 0)
        rangeMax = w3 - v3;
    if(wm > vm && wm - vm > rangeMax && vm > 0)
        rangeMax = wm - vm;

    m_pHScrollBar->setRange(0, rangeMax);
    m_pHScrollBar->setSingleStep(Utils::getHorizontalAdvance(fontMetrics(), '0') * 10);
    m_pHScrollBar->setPageStep(pageStep);
}

void KDiff3App::resizeDiffTextWindowHeight(qint32 newHeight)
{
    m_DTWHeight = newHeight;

    DiffTextWindow::mVScrollBar->setRange(0, std::max(0, qint32(m_neededLines + 1 - newHeight)));
    DiffTextWindow::mVScrollBar->setPageStep(newHeight);
    m_pOverview->setRange(DiffTextWindow::mVScrollBar->value(), DiffTextWindow::mVScrollBar->pageStep());

    setHScrollBarRange();
}

void KDiff3App::scrollDiffTextWindow(qint32 deltaX, qint32 deltaY)
{
    if(deltaY != 0 && DiffTextWindow::mVScrollBar != nullptr)
    {
        DiffTextWindow::mVScrollBar->setValue(DiffTextWindow::mVScrollBar->value() + deltaY);
    }
    if(deltaX != 0 && m_pHScrollBar != nullptr)
        m_pHScrollBar->QScrollBar::setValue(m_pHScrollBar->value() + deltaX);
}

void KDiff3App::scrollMergeResultWindow(qint32 deltaX, qint32 deltaY)
{
    if(deltaY != 0)
        MergeResultWindow::mVScrollBar->setValue(MergeResultWindow::mVScrollBar->value() + deltaY);
    if(deltaX != 0)
        m_pHScrollBar->setValue(m_pHScrollBar->value() + deltaX);
}

void KDiff3App::sourceMask(qint32 srcMask, qint32 enabledMask)
{
    chooseA->blockSignals(true);
    chooseB->blockSignals(true);
    chooseC->blockSignals(true);
    chooseA->setChecked((srcMask & 1) != 0);
    chooseB->setChecked((srcMask & 2) != 0);
    chooseC->setChecked((srcMask & 4) != 0);
    chooseA->blockSignals(false);
    chooseB->blockSignals(false);
    chooseC->blockSignals(false);
    chooseA->setEnabled((enabledMask & 1) != 0);
    chooseB->setEnabled((enabledMask & 2) != 0);
    chooseC->setEnabled((enabledMask & 4) != 0);
}

void KDiff3App::initView()
{
    // set the main widget here
    if(mInitCalled)
    {
        return;
    }

    mInitCalled = true;
    //m_pMainWidget // Contains vertical splitter and horiz scrollbar
    QVBoxLayout* pVLayout = new QVBoxLayout(m_pMainWidget);
    pVLayout->setContentsMargins(0, 0, 0, 0);
    pVLayout->setSpacing(0);

    QSplitter* pVSplitter = new QSplitter();
    pVSplitter->setObjectName("VSplitter");
    pVSplitter->setOpaqueResize(false);
    pVSplitter->setOrientation(Qt::Vertical);
    pVLayout->addWidget(pVSplitter);

    QWidget* pDiffWindowFrame = new QWidget(); // Contains diff windows, overview and vert scrollbar
    pDiffWindowFrame->setObjectName("DiffWindowFrame");
    QHBoxLayout* pDiffHLayout = new QHBoxLayout(pDiffWindowFrame);
    pDiffHLayout->setContentsMargins(0, 0, 0, 0);
    pDiffHLayout->setSpacing(0);
    pVSplitter->addWidget(pDiffWindowFrame);

    m_pDiffWindowSplitter = new QSplitter();
    m_pDiffWindowSplitter->setObjectName("DiffWindowSplitter");
    m_pDiffWindowSplitter->setOpaqueResize(false);

    m_pDiffWindowSplitter->setOrientation(m_pOptions->m_bHorizDiffWindowSplitting ? Qt::Horizontal : Qt::Vertical);
    pDiffHLayout->addWidget(m_pDiffWindowSplitter);

    m_pOverview = new Overview(m_pOptionDialog->getOptions());
    m_pOverview->setObjectName("Overview");
    pDiffHLayout->addWidget(m_pOverview);

    DiffTextWindow::mVScrollBar = new QScrollBar(Qt::Vertical, pDiffWindowFrame);
    pDiffHLayout->addWidget(DiffTextWindow::mVScrollBar);

    chk_connect(m_pOverview, &Overview::setLine, DiffTextWindow::mVScrollBar, &QScrollBar::setValue);
    chk_connect(this, &KDiff3App::showWhiteSpaceToggled, m_pOverview, &Overview::slotRedraw);
    chk_connect(this, &KDiff3App::changeOverViewMode, m_pOverview, &Overview::setOverviewMode);

    m_pDiffTextWindowFrame1 = new DiffTextWindowFrame(m_pDiffWindowSplitter, m_pOptionDialog->getOptions(), e_SrcSelector::A, m_sd1, *this);
    m_pDiffWindowSplitter->addWidget(m_pDiffTextWindowFrame1);
    m_pDiffTextWindowFrame2 = new DiffTextWindowFrame(m_pDiffWindowSplitter, m_pOptionDialog->getOptions(), e_SrcSelector::B, m_sd2, *this);
    m_pDiffWindowSplitter->addWidget(m_pDiffTextWindowFrame2);
    m_pDiffTextWindowFrame3 = new DiffTextWindowFrame(m_pDiffWindowSplitter, m_pOptionDialog->getOptions(), e_SrcSelector::C, m_sd3, *this);
    m_pDiffWindowSplitter->addWidget(m_pDiffTextWindowFrame3);
    m_pDiffTextWindow1 = m_pDiffTextWindowFrame1->getDiffTextWindow();
    m_pDiffTextWindow2 = m_pDiffTextWindowFrame2->getDiffTextWindow();
    m_pDiffTextWindow3 = m_pDiffTextWindowFrame3->getDiffTextWindow();

    m_pDiffTextWindowFrame1->setupConnections(this);
    m_pDiffTextWindowFrame2->setupConnections(this);
    m_pDiffTextWindowFrame3->setupConnections(this);

    // Merge window
    m_pMergeWindowFrame = new QWidget(pVSplitter);
    m_pMergeWindowFrame->setObjectName("MergeWindowFrame");
    pVSplitter->addWidget(m_pMergeWindowFrame);
    QHBoxLayout* pMergeHLayout = new QHBoxLayout(m_pMergeWindowFrame);
    pMergeHLayout->setContentsMargins(0, 0, 0, 0);
    pMergeHLayout->setSpacing(0);
    QVBoxLayout* pMergeVLayout = new QVBoxLayout();
    pMergeHLayout->addLayout(pMergeVLayout, 1);

    m_pMergeResultWindowTitle = new WindowTitleWidget(m_pOptionDialog->getOptions());
    pMergeVLayout->addWidget(m_pMergeResultWindowTitle);

    m_pMergeResultWindow = new MergeResultWindow(m_pMergeWindowFrame, m_pOptionDialog->getOptions(), statusBar());
    pMergeVLayout->addWidget(m_pMergeResultWindow, 1);

    MergeResultWindow::mVScrollBar = new QScrollBar(Qt::Vertical, m_pMergeWindowFrame);
    pMergeHLayout->addWidget(MergeResultWindow::mVScrollBar);

    autoAdvance->setEnabled(true);

    QList<qint32> sizes = pVSplitter->sizes();
    qint32 total = sizes[0] + sizes[1];
    if(total < 10)
        total = 100;
    sizes[0] = total / 2;
    sizes[1] = total / 2;
    pVSplitter->setSizes(sizes);

    QList<qint32> hSizes = {1, 1, 1};

    m_pDiffWindowSplitter->setSizes(hSizes);

    m_pMergeResultWindow->installEventFilter(m_pMergeResultWindowTitle); // for focus tracking

    QHBoxLayout* pHScrollBarLayout = new QHBoxLayout();
    pVLayout->addLayout(pHScrollBarLayout);
    m_pHScrollBar = new ReversibleScrollBar(Qt::Horizontal, &m_pOptions->m_bRightToLeftLanguage);
    pHScrollBarLayout->addWidget(m_pHScrollBar);
    m_pCornerWidget = new QWidget(m_pMainWidget);
    pHScrollBarLayout->addWidget(m_pCornerWidget);

    chk_connect(DiffTextWindow::mVScrollBar, &QScrollBar::valueChanged, m_pOverview, &Overview::setFirstLine);
    chk_connect(DiffTextWindow::mVScrollBar, &QScrollBar::valueChanged, m_pDiffTextWindow1, &DiffTextWindow::setFirstLine);
    chk_connect(m_pHScrollBar, &ReversibleScrollBar::valueChanged2, m_pDiffTextWindow1, &DiffTextWindow::setHorizScrollOffset);
    m_pDiffTextWindow1->setupConnections(this);

    chk_connect(DiffTextWindow::mVScrollBar, &QScrollBar::valueChanged, m_pDiffTextWindow2, &DiffTextWindow::setFirstLine);
    chk_connect(m_pHScrollBar, &ReversibleScrollBar::valueChanged2, m_pDiffTextWindow2, &DiffTextWindow::setHorizScrollOffset);
    m_pDiffTextWindow2->setupConnections(this);

    chk_connect(DiffTextWindow::mVScrollBar, &QScrollBar::valueChanged, m_pDiffTextWindow3, &DiffTextWindow::setFirstLine);
    chk_connect(m_pHScrollBar, &ReversibleScrollBar::valueChanged2, m_pDiffTextWindow3, &DiffTextWindow::setHorizScrollOffset);
    m_pDiffTextWindow3->setupConnections(this);

    MergeResultWindow* p = m_pMergeResultWindow;
    chk_connect(MergeResultWindow::mVScrollBar, &QScrollBar::valueChanged, p, &MergeResultWindow::setFirstLine);

    chk_connect(m_pHScrollBar, &ReversibleScrollBar::valueChanged2, p, &MergeResultWindow::setHorizScrollOffset);
    chk_connect(p, &MergeResultWindow::modifiedChanged, m_pMergeResultWindowTitle, &WindowTitleWidget::slotSetModified);
    p->setupConnections(this);
    sourceMask(0, 0);

    chk_connect(p, &MergeResultWindow::setFastSelectorRange, m_pDiffTextWindow1, &DiffTextWindow::setFastSelectorRange);
    chk_connect(p, &MergeResultWindow::setFastSelectorRange, m_pDiffTextWindow2, &DiffTextWindow::setFastSelectorRange);
    chk_connect(p, &MergeResultWindow::setFastSelectorRange, m_pDiffTextWindow3, &DiffTextWindow::setFastSelectorRange);
    chk_connect(m_pDiffTextWindow1, &DiffTextWindow::setFastSelectorLine, p, &MergeResultWindow::slotSetFastSelectorLine);
    chk_connect(m_pDiffTextWindow2, &DiffTextWindow::setFastSelectorLine, p, &MergeResultWindow::slotSetFastSelectorLine);
    chk_connect(m_pDiffTextWindow3, &DiffTextWindow::setFastSelectorLine, p, &MergeResultWindow::slotSetFastSelectorLine);
    chk_connect(m_pDiffTextWindow1, &DiffTextWindow::gotFocus, p, &MergeResultWindow::updateSourceMask);
    chk_connect(m_pDiffTextWindow2, &DiffTextWindow::gotFocus, p, &MergeResultWindow::updateSourceMask);
    chk_connect(m_pDiffTextWindow3, &DiffTextWindow::gotFocus, p, &MergeResultWindow::updateSourceMask);
    chk_connect(m_pDirectoryMergeInfo, &DirectoryMergeInfo::gotFocus, p, &MergeResultWindow::updateSourceMask);

    chk_connect(m_pDiffTextWindow1, &DiffTextWindow::resizeHeightChangedSignal, this, &KDiff3App::resizeDiffTextWindowHeight);
    // The following two connects cause the wordwrap to be recalced thrice, just to make sure. Better than forgetting one.
    chk_connect(m_pDiffTextWindow1, &DiffTextWindow::resizeWidthChangedSignal, this, &KDiff3App::postRecalcWordWrap);
    chk_connect(m_pDiffTextWindow2, &DiffTextWindow::resizeWidthChangedSignal, this, &KDiff3App::postRecalcWordWrap);
    chk_connect(m_pDiffTextWindow3, &DiffTextWindow::resizeWidthChangedSignal, this, &KDiff3App::postRecalcWordWrap);

    m_pDiffTextWindow1->setFocus();
    m_pMainWidget->setMinimumSize(50, 50);
    m_pCornerWidget->setFixedSize(DiffTextWindow::mVScrollBar->width(), m_pHScrollBar->height());
    showWindowA->setChecked(true);
    showWindowB->setChecked(true);
    showWindowC->setChecked(true);
}

// called after word wrap is complete
void KDiff3App::slotFinishMainInit()
{
    assert(m_pDiffTextWindow1 != nullptr && DiffTextWindow::mVScrollBar != nullptr && m_pOverview != nullptr);

    setHScrollBarRange();

    qint32 newHeight = m_pDiffTextWindow1->getNofVisibleLines();
    /*qint32 newWidth  = m_pDiffTextWindow1->getNofVisibleColumns();*/
    m_DTWHeight = newHeight;

    DiffTextWindow::mVScrollBar->setRange(0, std::max(0, qint32(m_neededLines + 1 - newHeight)));
    DiffTextWindow::mVScrollBar->setPageStep(newHeight);
    m_pOverview->setRange(DiffTextWindow::mVScrollBar->value(), DiffTextWindow::mVScrollBar->pageStep());

    qint32 d3l = -1;
    if(!m_manualDiffHelpList.empty())
        d3l = m_manualDiffHelpList.front().calcManualDiffFirstDiff3LineIdx(mDiff3LineVector);

    setUpdatesEnabled(true);

    if(d3l >= 0 && m_pDiffTextWindow1)
    {
        qint32 line = m_pDiffTextWindow1->convertDiff3LineIdxToLine(d3l);
        DiffTextWindow::mVScrollBar->setValue(std::max(0, line - 1));
    }
    else
    {
        m_pMergeResultWindow->slotGoTop();
        if(!m_outputFilename.isEmpty() && !m_pMergeResultWindow->isUnsolvedConflictAtCurrent())
            m_pMergeResultWindow->slotGoNextUnsolvedConflict();
    }

    if(m_pCornerWidget)
        m_pCornerWidget->setFixedSize(DiffTextWindow::mVScrollBar->width(), m_pHScrollBar->height());

    Q_EMIT updateAvailabilities();
    bool bVisibleMergeResultWindow = !m_outputFilename.isEmpty();

    if(m_bLoadFiles)
    {
        if(bVisibleMergeResultWindow)
            m_pMergeResultWindow->showNumberOfConflicts(!m_bAutoFlag);
        else if(
            // Avoid showing this message during startup without parameters.
            !(m_sd1->getAliasName().isEmpty() && m_sd2->getAliasName().isEmpty() && m_sd3->getAliasName().isEmpty()) &&
            (m_sd1->isValid() && m_sd2->isValid() && m_sd3->isValid()))
        {
            QString totalInfo;
            if(m_totalDiffStatus->isBinaryEqualAB() && m_totalDiffStatus->isBinaryEqualAC())
                totalInfo += i18n("All input files are binary equal.");
            else if(m_totalDiffStatus->isTextEqualAB() && m_totalDiffStatus->isTextEqualAC())
                totalInfo += i18n("All input files contain the same text, but are not binary equal.");
            else
            {
                if(m_totalDiffStatus->isBinaryEqualAB())
                    totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("A"), i18n("B"));
                else if(m_totalDiffStatus->isTextEqualAB())
                    totalInfo += i18n("Files %1 and %2 have equal text, but are not binary equal. \n", i18n("A"), i18n("B"));
                if(m_totalDiffStatus->isBinaryEqualAC())
                    totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("A"), i18n("C"));
                else if(m_totalDiffStatus->isTextEqualAC())
                    totalInfo += i18n("Files %1 and %2 have equal text, but are not binary equal. \n", i18n("A"), i18n("C"));
                if(m_totalDiffStatus->isBinaryEqualBC())
                    totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("B"), i18n("C"));
                else if(m_totalDiffStatus->isTextEqualBC())
                    totalInfo += i18n("Files %1 and %2 have equal text, but are not binary equal. \n", i18n("B"), i18n("C"));
            }

            if(!totalInfo.isEmpty())
                KMessageBox::information(this, totalInfo);
        }

        if(bVisibleMergeResultWindow && (!m_sd1->isText() || !m_sd2->isText() || !m_sd3->isText()))
        {
            KMessageBox::information(this, i18n(
                                               "Some input files do not seem to be pure text files.\n"
                                               "Note that the KDiff3 merge was not meant for binary data.\n"
                                               "Continue at your own risk."));
        }
        if(m_sd1->isIncompleteConversion() || m_sd2->isIncompleteConversion() || m_sd3->isIncompleteConversion())
        {
            QString files;
            if(m_sd1->isIncompleteConversion())
                files += i18n("A");
            if(m_sd2->isIncompleteConversion())
                files += files.isEmpty() ? i18n("B") : i18n(", B");
            if(m_sd3->isIncompleteConversion())
                files += files.isEmpty() ? i18n("C") : i18n(", C");

            KMessageBox::information(this, i18n("Some input characters could not be converted to valid unicode.\n"
                                                "You might be using the wrong codec. (e.g. UTF-8 for non UTF-8 files).\n"
                                                "Do not save the result if unsure. Continue at your own risk.\n"
                                                "Affected input files are in %1.", files));
        }
    }

    if(bVisibleMergeResultWindow && m_pMergeResultWindow)
    {
        m_pMergeResultWindow->setFocus();
    }
    else if(m_pDiffTextWindow1)
    {
        m_pDiffTextWindow1->setFocus();
    }
}

void KDiff3App::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
    if(m_pCornerWidget)
        m_pCornerWidget->setFixedSize(DiffTextWindow::mVScrollBar->width(), m_pHScrollBar->height());
}

void KDiff3App::wheelEvent(QWheelEvent* pWheelEvent)
{
    pWheelEvent->accept();
    QPoint delta = pWheelEvent->angleDelta();

    //Block diagonal scrolling easily generated unintentionally with track pads.
    if(delta.x() != 0 && abs(delta.y()) < abs(delta.x()) && m_pHScrollBar != nullptr)
        QCoreApplication::postEvent(m_pHScrollBar, new QWheelEvent(*pWheelEvent));
}

void KDiff3App::keyPressEvent(QKeyEvent* keyEvent)
{
    bool bCtrl = (keyEvent->modifiers() & Qt::ControlModifier) != 0;

    switch(keyEvent->key())
    {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            if(DiffTextWindow::mVScrollBar != nullptr)
                QCoreApplication::postEvent(DiffTextWindow::mVScrollBar, new QKeyEvent(*keyEvent));
            return;
        case Qt::Key_Left:
        case Qt::Key_Right:
            if(m_pHScrollBar != nullptr)
                QCoreApplication::postEvent(m_pHScrollBar, new QKeyEvent(*keyEvent));

            break;
        case Qt::Key_End:
        case Qt::Key_Home:
            if(bCtrl)
            {
                if(DiffTextWindow::mVScrollBar != nullptr)
                    QCoreApplication::postEvent(DiffTextWindow::mVScrollBar, new QKeyEvent(*keyEvent));
            }
            else
            {
                if(m_pHScrollBar != nullptr)
                    QCoreApplication::postEvent(m_pHScrollBar, new QKeyEvent(*keyEvent));
            }
            break;
        default:
            break;
    }
}

void KDiff3App::slotFinishDrop()
{
    raise();
    mainInit(m_totalDiffStatus);
}

void KDiff3App::slotFileOpen()
{
    if(!canContinue()) return;

    if(m_pDirectoryMergeWindow->isDirectoryMergeInProgress())
    {
        qint32 result = KMessageBox::warningYesNo(this,
                                               i18n("You are currently doing a folder merge. Are you sure, you want to abort?"),
                                               i18nc("Error dialog caption", "Warning"),
                                               KGuiItem(i18n("Abort")),
                                               KGuiItem(i18n("Continue Merging")));
        if(result != KMessageBox::Yes)
            return;
    }

    slotStatusMsg(i18n("Opening files..."));

    for(;;)
    {
        QPointer<OpenDialog> d = QPointer<OpenDialog>(new OpenDialog(this,
                     QDir::toNativeSeparators(m_bDirCompare ? gDirInfo->dirA().prettyAbsPath() : m_sd1->isFromBuffer() ? QString("") : m_sd1->getAliasName()),
                     QDir::toNativeSeparators(m_bDirCompare ? gDirInfo->dirB().prettyAbsPath() : m_sd2->isFromBuffer() ? QString("") : m_sd2->getAliasName()),
                     QDir::toNativeSeparators(m_bDirCompare ? gDirInfo->dirC().prettyAbsPath() : m_sd3->isFromBuffer() ? QString("") : m_sd3->getAliasName()),
                     m_bDirCompare ? !gDirInfo->destDir().prettyAbsPath().isEmpty() : !m_outputFilename.isEmpty(),
                     QDir::toNativeSeparators(m_bDefaultFilename ? QString("") : m_outputFilename), m_pOptionDialog->getOptions()));

        qint32 status = d->exec();
        if(status == QDialog::Accepted)
        {
            m_sd1->setFilename(d->getFileA());
            m_sd2->setFilename(d->getFileB());
            m_sd3->setFilename(d->getFileC());

            if(d->merge())
            {
                if(d->getOutputFile().isEmpty())
                {
                    m_outputFilename = "unnamed.txt";
                    m_bDefaultFilename = true;
                }
                else
                {
                    m_outputFilename = d->getOutputFile();
                    m_bDefaultFilename = false;
                }
            }
            else
                m_outputFilename = "";

            m_bDirCompare = m_sd1->isDir();

            if(m_bDirCompare)
            {
                bool bSuccess = doDirectoryCompare(false);
                if(bSuccess)
                {
                    m_pDirectoryMergeDock->show();
                    m_pDirectoryMergeInfoDock->show();
                    m_pMainWidget->hide();
                    break;
                }
            }
            else
            {
                improveFilenames();

                m_pDirectoryMergeDock->hide();
                m_pDirectoryMergeInfoDock->hide();
                mainInit(m_totalDiffStatus);

                if((!m_sd1->getErrors().isEmpty()) ||
                   (!m_sd2->getErrors().isEmpty()) ||
                   (!m_sd3->getErrors().isEmpty()))
                {
                    QString text(i18n("Opening of these files failed:"));
                    text += "\n\n";
                    if(!m_sd1->getErrors().isEmpty())
                        text += " - " + m_sd1->getAliasName() + '\n' + m_sd1->getErrors().join('\n') + '\n';
                    if(!m_sd2->getErrors().isEmpty())
                        text += " - " + m_sd2->getAliasName() + '\n' + m_sd2->getErrors().join('\n') + '\n';
                    if(!m_sd3->getErrors().isEmpty())
                        text += " - " + m_sd3->getAliasName() + '\n' + m_sd3->getErrors().join('\n') + '\n';

                    KMessageBox::error(this, text, i18n("File open error"));

                    continue;
                }
            }
        }
        break;
    }

    Q_EMIT updateAvailabilities();
    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotFileOpen2(QStringList& errors, const QString& fn1, const QString& fn2, const QString& fn3, const QString& ofn,
                              const QString& an1, const QString& an2, const QString& an3, TotalDiffStatus* pTotalDiffStatus)
{
    if(!canContinue()) return;

    if(fn1.isEmpty() && fn2.isEmpty() && fn3.isEmpty() && ofn.isEmpty())
    {
        m_pMainWidget->hide();
        return;
    }

    slotStatusMsg(i18n("Opening files..."));
    m_sd1->reset();
    m_sd2->reset();
    m_sd3->reset();

    m_sd1->setFilename(fn1);
    m_sd2->setFilename(fn2);
    m_sd3->setFilename(fn3);

    m_sd1->setAliasName(an1);
    m_sd2->setAliasName(an2);
    m_sd3->setAliasName(an3);

    if(!ofn.isEmpty())
    {
        m_outputFilename = ofn;
        m_bDefaultFilename = false;
    }
    else
    {
        m_outputFilename = "";
        m_bDefaultFilename = true;
    }

    if(!m_sd1->isDir())
    {
        improveFilenames();
        //KDiff3App::slotFileOpen2 needs to handle both GUI and non-GUI diffs.
        if(pTotalDiffStatus == nullptr)
            mainInit(m_totalDiffStatus);
        else
            mainInit(pTotalDiffStatus, InitFlag::loadFiles | InitFlag::autoSolve);

        if(m_bDirCompare)
        {
            errors.append(m_sd1->getErrors());
            errors.append(m_sd2->getErrors());
            errors.append(m_sd3->getErrors());

            return;
        }

        if(m_sd1->isValid() && m_sd2->isValid() && m_sd3->isValid())
        {
            if(m_pDirectoryMergeWindow != nullptr && m_pDirectoryMergeWindow->isVisible() && !dirShowBoth->isChecked())
            {
                slotDirViewToggle();
            }
        }
    }
    else
        doDirectoryCompare(true); // Create new window for KDiff3 for directory comparison.

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotFileNameChanged(const QString& fileName, e_SrcSelector winIdx)
{
    QStringList errors;
    QString fn1 = m_sd1->getFilename();
    QString an1 = m_sd1->getAliasName();
    QString fn2 = m_sd2->getFilename();
    QString an2 = m_sd2->getAliasName();
    QString fn3 = m_sd3->getFilename();
    QString an3 = m_sd3->getAliasName();

    if(winIdx == e_SrcSelector::A)
    {
        fn1 = fileName;
        an1 = "";
    }
    else if(winIdx == e_SrcSelector::B)
    {
        fn2 = fileName;
        an2 = "";
    }
    else if(winIdx == e_SrcSelector::C)
    {
        fn3 = fileName;
        an3 = "";
    }

    slotFileOpen2(errors, fn1, fn2, fn3, m_outputFilename, an1, an2, an3, nullptr);
}

void KDiff3App::slotEditCut()
{
    slotStatusMsg(i18n("Cutting selection..."));
    Q_EMIT cut();
    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditCopy()
{
    slotStatusMsg(i18n("Copying selection to clipboard..."));

    Q_EMIT copy();

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditPaste()
{
    slotStatusMsg(i18n("Inserting clipboard contents..."));

    if(m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isVisible())
    {
        m_pMergeResultWindow->pasteClipboard(false);
    }
    else
    {
        if(canContinue())
        {
            QString error;
            bool do_init = false;

            if(m_pDiffTextWindow1->hasFocus())
            {
                error = m_sd1->setData(QApplication::clipboard()->text(QClipboard::Clipboard));
                do_init = true;
            }
            else if(m_pDiffTextWindow2->hasFocus())
            {
                error = m_sd2->setData(QApplication::clipboard()->text(QClipboard::Clipboard));
                do_init = true;
            }
            else if(m_pDiffTextWindow3->hasFocus())
            {
                error = m_sd3->setData(QApplication::clipboard()->text(QClipboard::Clipboard));
                do_init = true;
            }

            if(!error.isEmpty())
            {
                KMessageBox::error(m_pOptionDialog, error);
            }

            if(do_init)
            {
                mainInit(m_totalDiffStatus);
            }
        }
    }

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditSelectAll()
{

    Q_EMIT selectAll();

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotGoCurrent()
{
    Q_EMIT goCurrent();
}
void KDiff3App::slotGoTop()
{
    Q_EMIT goTop();
}
void KDiff3App::slotGoBottom()
{
    Q_EMIT goBottom();
}
void KDiff3App::slotGoPrevUnsolvedConflict()
{
    Q_EMIT goPrevUnsolvedConflict();
}
void KDiff3App::slotGoNextUnsolvedConflict()
{
    m_bTimerBlock = false;
    Q_EMIT goNextUnsolvedConflict();
}
void KDiff3App::slotGoPrevConflict()
{
    Q_EMIT goPrevConflict();
}
void KDiff3App::slotGoNextConflict()
{
    m_bTimerBlock = false;
    Q_EMIT goNextConflict();
}
void KDiff3App::slotGoPrevDelta()
{
    Q_EMIT goPrevDelta();
}
void KDiff3App::slotGoNextDelta()
{
    Q_EMIT goNextDelta();
}

void KDiff3App::slotGoToLine()
{
    QDialog pDialog;
    QVBoxLayout* l = new QVBoxLayout(&pDialog);

    QLineEdit* pLineNumEdit = new QLineEdit();
    //Limit imput to valid 1 based line numbers
    pLineNumEdit->setValidator(new QIntValidator(1, DiffTextWindow::mVScrollBar->maximum(), pLineNumEdit));

    QPushButton* pOkButton = new QPushButton(i18n("Ok"));
    l->addWidget(pLineNumEdit);
    l->addWidget(pOkButton);

    chk_connect(pOkButton, &QPushButton::clicked, &pDialog,
                ([&pDialog, pLineNumEdit]() {
                    if(pLineNumEdit->text() != "")
                    {
                        qint32 lineNum = pLineNumEdit->text().toInt();
                        lineNum = qMax(lineNum - 2, 0);
                        //No need for anything else here setValue triggers a valueChanged signal internally.
                        DiffTextWindow::mVScrollBar->setValue(lineNum);
                    }
                    pDialog.close();
                }));

    pDialog.setWindowTitle(i18n("Go to Line"));
    pDialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    pDialog.setFixedSize(260, 110);
    pDialog.exec();
}

void KDiff3App::choose(e_SrcSelector choice)
{
    if(!m_bTimerBlock)
    {
        if(m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->hasFocus())
        {
            if(choice == e_SrcSelector::A) m_pDirectoryMergeWindow->slotCurrentChooseA();
            if(choice == e_SrcSelector::B) m_pDirectoryMergeWindow->slotCurrentChooseB();
            if(choice == e_SrcSelector::C) m_pDirectoryMergeWindow->slotCurrentChooseC();

            chooseA->setChecked(false);
            chooseB->setChecked(false);
            chooseC->setChecked(false);
        }
        else if(m_pMergeResultWindow)
        {
            m_pMergeResultWindow->choose(choice);
            if(autoAdvance->isChecked())
            {
                m_bTimerBlock = true;
                QTimer::singleShot(m_pOptions->m_autoAdvanceDelay, this, &KDiff3App::slotGoNextUnsolvedConflict);
            }
        }
    }
}

void KDiff3App::slotChooseA()
{
    choose(e_SrcSelector::A);
}
void KDiff3App::slotChooseB()
{
    choose(e_SrcSelector::B);
}
void KDiff3App::slotChooseC()
{
    choose(e_SrcSelector::C);
}

void KDiff3App::slotAutoSolve()
{
    Q_EMIT autoSolve();

    Q_EMIT updateAvailabilities();
}

void KDiff3App::slotUnsolve()
{
    Q_EMIT unsolve();
}

void KDiff3App::slotMergeHistory()
{
    Q_EMIT mergeHistory();
}

void KDiff3App::slotRegExpAutoMerge()
{
    Q_EMIT regExpAutoMerge();
}

void KDiff3App::slotSplitDiff()
{
    LineRef firstLine;
    LineRef lastLine;
    QPointer<DiffTextWindow> pDTW = nullptr;
    if(m_pDiffTextWindow1)
    {
        pDTW = m_pDiffTextWindow1;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(!firstLine.isValid() && m_pDiffTextWindow2)
    {
        pDTW = m_pDiffTextWindow2;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(!firstLine.isValid() && m_pDiffTextWindow3)
    {
        pDTW = m_pDiffTextWindow3;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(pDTW && firstLine.isValid() && m_pMergeResultWindow)
    {
        pDTW->resetSelection();

        m_pMergeResultWindow->slotSplitDiff(firstLine, lastLine);
    }
}

void KDiff3App::slotJoinDiffs()
{
    LineRef firstLine;
    LineRef lastLine;
    DiffTextWindow* pDTW = nullptr;
    if(m_pDiffTextWindow1)
    {
        pDTW = m_pDiffTextWindow1;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(!firstLine.isValid() && m_pDiffTextWindow2)
    {
        pDTW = m_pDiffTextWindow2;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(!firstLine.isValid() && m_pDiffTextWindow3)
    {
        pDTW = m_pDiffTextWindow3;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(pDTW && firstLine.isValid() && m_pMergeResultWindow)
    {
        pDTW->resetSelection();

        m_pMergeResultWindow->slotJoinDiffs(firstLine, lastLine);
    }
}

void KDiff3App::slotConfigure()
{
    m_pOptionDialog->setState();
    m_pOptionDialog->setMinimumHeight(m_pOptionDialog->minimumHeight() + 40);
    m_pOptionDialog->exec();
    mEscapeAction->setEnabled(m_pOptions->m_bEscapeKeyQuits);
    slotRefresh();
}

void KDiff3App::slotConfigureKeys()
{
    KShortcutsDialog::configure(actionCollection(), KShortcutsEditor::LetterShortcutsDisallowed, this);
}

void KDiff3App::slotRefresh()
{
    QApplication::setFont(m_pOptions->appFont());

    Q_EMIT doRefresh();

    if(m_pHScrollBar != nullptr)
    {
        m_pHScrollBar->setAgain();
    }
    if(m_pDiffWindowSplitter != nullptr)
    {
        m_pDiffWindowSplitter->setOrientation(m_pOptions->m_bHorizDiffWindowSplitting ? Qt::Horizontal : Qt::Vertical);
    }
}

void KDiff3App::slotSelectionStart()
{
    const QObject* s = sender();

    if(m_pDiffTextWindow1 && s != m_pDiffTextWindow1) m_pDiffTextWindow1->resetSelection();
    if(m_pDiffTextWindow2 && s != m_pDiffTextWindow2) m_pDiffTextWindow2->resetSelection();
    if(m_pDiffTextWindow3 && s != m_pDiffTextWindow3) m_pDiffTextWindow3->resetSelection();
    if(m_pMergeResultWindow && s != m_pMergeResultWindow) m_pMergeResultWindow->resetSelection();
}

void KDiff3App::slotSelectionEnd()
{
    if(m_pOptions->m_bAutoCopySelection)
    {
        slotEditCopy();
    }
    else
    {
        QClipboard* clipBoard = QApplication::clipboard();

        if(clipBoard->supportsSelection())
        {
            QString sCurSelection = getSelection();

            if(!sCurSelection.isEmpty())
            {
                clipBoard->setText(sCurSelection, QClipboard::Selection);
            }
        }
    }

    Q_EMIT updateAvailabilities();
}

void KDiff3App::slotClipboardChanged()
{
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    if(mimeData && mimeData->hasText())
    {
        QString s = clipboard->text();
        editPaste->setEnabled(!s.isEmpty());
    }
    else
    {
        editPaste->setEnabled(false);
    }
}

void KDiff3App::slotOutputModified(bool bModified)
{
    if(bModified && !m_bOutputModified)
    {
        m_bOutputModified = true;
        Q_EMIT updateAvailabilities();
    }
}

void KDiff3App::slotAutoAdvanceToggled()
{
    m_pOptions->m_bAutoAdvance = autoAdvance->isChecked();
}

void KDiff3App::slotWordWrapToggled()
{
    m_pOptions->setWordWrap(wordWrap->isChecked());
    postRecalcWordWrap();
}

// Enable or disable all widgets except the status bar widget.
void KDiff3App::mainWindowEnable(bool bEnable)
{
    if(QMainWindow* pWindow = dynamic_cast<QMainWindow*>(window()))
    {
        QWidget* pStatusBarWidget = pWindow->statusBar();
        pWindow->setEnabled(bEnable);
        pStatusBarWidget->setEnabled(true);
    }
}

void KDiff3App::postRecalcWordWrap()
{
    if(!m_bRecalcWordWrapPosted)
    {
        while(DiffTextWindow::maxThreads() > 0) {} //Clear wordwrap threads.
        m_bRecalcWordWrapPosted = true;
        m_firstD3LIdx = -1;
        Q_EMIT sigRecalcWordWrap();
    }
    else
    {
        g_pProgressDialog->cancel(ProgressDialog::eResize);
    }
}

void KDiff3App::slotRecalcWordWrap()
{
    recalcWordWrap();
}

// visibleTextWidthForPrinting is >=0 only for printing, otherwise the really visible width is used
void KDiff3App::recalcWordWrap(qint32 visibleTextWidthForPrinting)
{
    m_bRecalcWordWrapPosted = true;
    mainWindowEnable(false);

    if(m_firstD3LIdx < 0)
    {
        m_firstD3LIdx = 0;
        if(!m_pDiffTextWindow1)
            return; //Nothing that happens from here on makes any sense if we don't have a DiffTextWindow.

        m_firstD3LIdx = m_pDiffTextWindow1->convertLineToDiff3LineIdx(m_pDiffTextWindow1->getFirstLine());
    }

    // Convert selection to D3L-coords (converting back happens in DiffTextWindow::recalcWordWrap()
    if(m_pDiffTextWindow1)
        m_pDiffTextWindow1->convertSelectionToD3LCoords();
    if(m_pDiffTextWindow2)
        m_pDiffTextWindow2->convertSelectionToD3LCoords();
    if(m_pDiffTextWindow3)
        m_pDiffTextWindow3->convertSelectionToD3LCoords();

    g_pProgressDialog->clearCancelState(); // clear cancelled state if previously set

    if(!m_diff3LineList.empty())
    {
        if(m_pOptions->wordWrapOn())
        {
            m_diff3LineList.recalcWordWrap(true);

            // Let every window calc how many lines will be needed.
            if(m_pDiffTextWindow1)
            {
                m_pDiffTextWindow1->recalcWordWrap(true, 0, visibleTextWidthForPrinting);
            }
            if(m_pDiffTextWindow2)
            {
                m_pDiffTextWindow2->recalcWordWrap(true, 0, visibleTextWidthForPrinting);
            }
            if(m_pDiffTextWindow3)
            {
                m_pDiffTextWindow3->recalcWordWrap(true, 0, visibleTextWidthForPrinting);
            }
        }
        else
        {
            m_neededLines = m_diff3LineList.size();
            if(m_pDiffTextWindow1)
                m_pDiffTextWindow1->recalcWordWrap(false, 0, 0);
            if(m_pDiffTextWindow2)
                m_pDiffTextWindow2->recalcWordWrap(false, 0, 0);
            if(m_pDiffTextWindow3)
                m_pDiffTextWindow3->recalcWordWrap(false, 0, 0);
        }
        mRunnablesStarted = DiffTextWindow::startRunnables();
        if(!mRunnablesStarted)
            slotFinishRecalcWordWrap(visibleTextWidthForPrinting);
        else
        {
            g_pProgressDialog->setInformation(m_pOptions->wordWrapOn()
                                                  ? i18n("Word wrap (Cancel disables word wrap)")
                                                  : i18n("Calculating max width for horizontal scrollbar"),
                                              false);
        }
    }
    else
    {
        //don't leave proccessing incomplete if m_diff3LineList isEmpty as when an error occurs during reading.
        slotFinishRecalcWordWrap(visibleTextWidthForPrinting);
    }
}

void KDiff3App::slotFinishRecalcWordWrap(qint32 visibleTextWidthForPrinting)
{
    assert(m_firstD3LIdx >= 0);

    if(mRunnablesStarted)
    {
        ProgressProxy::endBackgroundTask();
        mRunnablesStarted = false;
    }

    if(m_pOptions->wordWrapOn() && g_pProgressDialog->wasCancelled())
    {
        if(g_pProgressDialog->cancelReason() == ProgressDialog::eUserAbort)
        {
            wordWrap->setChecked(false);
            m_pOptions->setWordWrap(wordWrap->isChecked());
        }

        Q_EMIT sigRecalcWordWrap();
        return;
    }
    else
    {
        m_bRecalcWordWrapPosted = false;
    }

    g_pProgressDialog->setStayHidden(false);

    const bool bPrinting = visibleTextWidthForPrinting >= 0;

    if(!m_diff3LineList.empty())
    {
        if(m_pOptions->wordWrapOn())
        {
            LineCount sumOfLines = m_diff3LineList.recalcWordWrap(false);

            // Finish the word wrap
            if(m_pDiffTextWindow1)
                m_pDiffTextWindow1->recalcWordWrap(true, sumOfLines, visibleTextWidthForPrinting);
            if(m_pDiffTextWindow2)
                m_pDiffTextWindow2->recalcWordWrap(true, sumOfLines, visibleTextWidthForPrinting);
            if(m_pDiffTextWindow3)
                m_pDiffTextWindow3->recalcWordWrap(true, sumOfLines, visibleTextWidthForPrinting);

            m_neededLines = sumOfLines;
        }
        else
        {
            if(m_pDiffTextWindow1)
                m_pDiffTextWindow1->recalcWordWrap(false, 1, 0);
            if(m_pDiffTextWindow2)
                m_pDiffTextWindow2->recalcWordWrap(false, 1, 0);
            if(m_pDiffTextWindow3)
                m_pDiffTextWindow3->recalcWordWrap(false, 1, 0);
        }
        slotStatusMsg(QString());
    }

    if(!bPrinting)
    {
        if(m_pOverview)
            m_pOverview->slotRedraw();
        if(DiffTextWindow::mVScrollBar)
            DiffTextWindow::mVScrollBar->setRange(0, std::max(0, qint32(m_neededLines + 1 - m_DTWHeight)));
        if(m_pDiffTextWindow1)
        {
            if(DiffTextWindow::mVScrollBar)
                DiffTextWindow::mVScrollBar->setValue(m_pDiffTextWindow1->convertDiff3LineIdxToLine(m_firstD3LIdx));

            setHScrollBarRange();
            m_pHScrollBar->setValue(0);
        }
    }
    mainWindowEnable(true);

    if(m_bFinishMainInit)
    {
        m_bFinishMainInit = false;
        slotFinishMainInit();
    }
    if(m_pEventLoopForPrinting)
        m_pEventLoopForPrinting->quit();
}

void KDiff3App::slotShowWhiteSpaceToggled()
{
    m_pOptions->m_bShowWhiteSpaceCharacters = showWhiteSpaceCharacters->isChecked();
    m_pOptions->m_bShowWhiteSpace = showWhiteSpace->isChecked();

    Q_EMIT showWhiteSpaceToggled();
}

void KDiff3App::slotShowLineNumbersToggled()
{
    m_pOptions->m_bShowLineNumbers = showLineNumbers->isChecked();

    if(wordWrap->isChecked())
        recalcWordWrap();

    Q_EMIT showLineNumbersToggled();
}

/// Return true for success, else false
bool KDiff3App::doDirectoryCompare(const bool bCreateNewInstance)
{
    FileAccess f1(m_sd1->getFilename());
    FileAccess f2(m_sd2->getFilename());
    FileAccess f3(m_sd3->getFilename());
    FileAccess f4(m_outputFilename);

    assert(f1.isDir());

    if(bCreateNewInstance)
    {
        Q_EMIT createNewInstance(f1.absoluteFilePath(), f2.absoluteFilePath(), f3.absoluteFilePath());
    }
    else
    {
        //Only a debugging aid now. Used to insure m_bDirCompare is not changed
        [[maybe_unused]] const bool bDirCompare = m_bDirCompare;

        FileAccess destDir;

        if(!m_bDefaultFilename) destDir = f4;
        m_pDirectoryMergeDock->show();
        m_pDirectoryMergeInfoDock->show();
        m_pMainWidget->hide();
        setUpdatesEnabled(true);

        (*gDirInfo) = DirectoryInfo(f1, f2, f3, destDir);

        bool bSuccess = m_pDirectoryMergeWindow->init(
            !m_outputFilename.isEmpty());
        //This is a bug if it still happens.
        assert(m_bDirCompare == bDirCompare);

        if(bSuccess)
        {
            m_sd1->reset();
            if(m_pDiffTextWindow1 != nullptr)
            {
                m_pDiffTextWindow1->init(QString(""), nullptr, eLineEndStyleDos, nullptr, 0, nullptr, nullptr);
                m_pDiffTextWindowFrame1->init();
            }
            m_sd2->reset();
            if(m_pDiffTextWindow2 != nullptr)
            {
                m_pDiffTextWindow2->init(QString(""), nullptr, eLineEndStyleDos, nullptr, 0, nullptr, nullptr);
                m_pDiffTextWindowFrame2->init();
            }
            m_sd3->reset();
            if(m_pDiffTextWindow3 != nullptr)
            {
                m_pDiffTextWindow3->init(QString(""), nullptr, eLineEndStyleDos, nullptr, 0, nullptr, nullptr);
                m_pDiffTextWindowFrame3->init();
            }
        }
        Q_EMIT updateAvailabilities();
        return bSuccess;
    }

    return true;
}
/*
    If A is targetted to an existing file and the paths point to directories attempt to find that file in the corrisponding
    directory. If it exists then the filename from A will be appended to the path.
*/
void KDiff3App::improveFilenames()
{
    FileAccess f1(m_sd1->getFilename());
    FileAccess f2(m_sd2->getFilename());
    FileAccess f3(m_sd3->getFilename());
    FileAccess f4(m_outputFilename);

    if(f1.isFile() && f1.exists())
    {
        if(f2.isDir())
        {
            f2.addPath(f1.fileName());
            if(f2.isFile() && f2.exists())
                m_sd2->setFileAccess(f2);
        }
        if(f3.isDir())
        {
            f3.addPath(f1.fileName());
            if(f3.isFile() && f3.exists())
                m_sd3->setFileAccess(f3);
        }
        if(f4.isDir())
        {
            f4.addPath(f1.fileName());
            if(f4.isFile() && f4.exists())
                m_outputFilename = f4.absoluteFilePath();
        }
    }
}

void KDiff3App::slotReload()
{
    if(!canContinue()) return;

    mainInit(m_totalDiffStatus);
}

bool KDiff3App::canContinue()
{
    // First test if anything must be saved.
    if(m_bOutputModified)
    {
        qint32 result = KMessageBox::warningYesNoCancel(this,
                                                     i18n("The merge result has not been saved."),
                                                     i18nc("Error dialog caption", "Warning"),
                                                     KGuiItem(i18n("Save && Continue")),
                                                     KGuiItem(i18n("Continue Without Saving")));
        if(result == KMessageBox::Cancel)
            return false;
        else if(result == KMessageBox::Yes)
        {
            slotFileSave();
            if(m_bOutputModified)
            {
                KMessageBox::error(this, i18n("Saving the merge result failed."), i18nc("Error dialog caption", "Warning"));
                return false;
            }
        }
    }

    m_bOutputModified = false;
    return true;
}

void KDiff3App::slotDirShowBoth()
{
    if(dirShowBoth->isChecked())
    {
        if(m_pDirectoryMergeDock)
            m_pDirectoryMergeDock->setVisible(m_bDirCompare);
        if(m_pDirectoryMergeInfoDock)
            m_pDirectoryMergeInfoDock->setVisible(m_bDirCompare);

        m_pMainWidget->show();
    }
    else
    {
        bool bTextDataAvailable = (m_sd1->hasData() || m_sd2->hasData() || m_sd3->hasData());
        if(bTextDataAvailable)
        {
            m_pMainWidget->show();
            m_pDirectoryMergeDock->hide();
            m_pDirectoryMergeInfoDock->hide();
        }
        else if(m_bDirCompare)
        {
            m_pDirectoryMergeDock->show();
            m_pDirectoryMergeInfoDock->show();
        }
    }

    Q_EMIT updateAvailabilities();
}

void KDiff3App::slotDirViewToggle()
{
    if(m_bDirCompare)
    {
        if(!m_pDirectoryMergeDock->isVisible())
        {
            m_pDirectoryMergeDock->show();
            m_pDirectoryMergeInfoDock->show();
            m_pMainWidget->hide();
        }
        else
        {
            m_pDirectoryMergeDock->hide();
            m_pDirectoryMergeInfoDock->hide();
            m_pMainWidget->show();
        }
    }
    Q_EMIT updateAvailabilities();
}

void KDiff3App::slotShowWindowAToggled()
{
    if(m_pDiffTextWindow1 != nullptr)
    {
        m_pDiffTextWindowFrame1->setVisible(showWindowA->isChecked());
        Q_EMIT updateAvailabilities();
    }
}

void KDiff3App::slotShowWindowBToggled()
{
    if(m_pDiffTextWindow2 != nullptr)
    {
        m_pDiffTextWindowFrame2->setVisible(showWindowB->isChecked());
        Q_EMIT updateAvailabilities();
    }
}

void KDiff3App::slotShowWindowCToggled()
{
    if(m_pDiffTextWindow3 != nullptr)
    {
        m_pDiffTextWindowFrame3->setVisible(showWindowC->isChecked());
        Q_EMIT updateAvailabilities();
    }
}

void KDiff3App::slotEditFind()
{
    m_pFindDialog->restartFind();

    // Use currently selected text:
    QString sCurSelection = getSelection();

    if(!sCurSelection.isEmpty() && !sCurSelection.contains('\n'))
    {
        m_pFindDialog->m_pSearchString->setText(sCurSelection);
    }

    if(QDialog::Accepted == m_pFindDialog->exec())
    {
        slotEditFindNext();
    }
}

void KDiff3App::slotEditFindNext()
{
    QString s = m_pFindDialog->m_pSearchString->text();
    if(s.isEmpty())
    {
        slotEditFind();
        return;
    }

    bool bDirDown = true;
    bool bCaseSensitive = m_pFindDialog->m_pCaseSensitive->isChecked();

    LineRef d3vLine = m_pFindDialog->currentLine;
    qint32 posInLine = m_pFindDialog->currentPos;
    LineRef l;
    qint32 p = 0;
    if(m_pFindDialog->getCurrentWindow() == eWindowIndex::A)
    {
        if(m_pFindDialog->m_pSearchInA->isChecked() && m_pDiffTextWindow1 != nullptr &&
           m_pDiffTextWindow1->findString(s, d3vLine, posInLine, bDirDown, bCaseSensitive))
        {
            m_pDiffTextWindow1->setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length(), l, p);
            DiffTextWindow::mVScrollBar->setValue(l - DiffTextWindow::mVScrollBar->pageStep() / 2);
            m_pHScrollBar->setValue(std::max(0, p + s.length() - m_pHScrollBar->pageStep()));
            m_pFindDialog->currentLine = d3vLine;
            m_pFindDialog->currentPos = posInLine + 1;
            return;
        }
        m_pFindDialog->nextWindow();
    }

    d3vLine = m_pFindDialog->currentLine;
    posInLine = m_pFindDialog->currentPos;
    if(m_pFindDialog->getCurrentWindow() == eWindowIndex::B)
    {
        if(m_pFindDialog->m_pSearchInB->isChecked() && m_pDiffTextWindow2 != nullptr &&
           m_pDiffTextWindow2->findString(s, d3vLine, posInLine, bDirDown, bCaseSensitive))
        {
            m_pDiffTextWindow2->setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length(), l, p);
            DiffTextWindow::mVScrollBar->setValue(l - DiffTextWindow::mVScrollBar->pageStep() / 2);
            m_pHScrollBar->setValue(std::max(0, p + s.length() - m_pHScrollBar->pageStep()));
            m_pFindDialog->currentLine = d3vLine;
            m_pFindDialog->currentPos = posInLine + 1;
            return;
        }

        m_pFindDialog->nextWindow();
    }

    d3vLine = m_pFindDialog->currentLine;
    posInLine = m_pFindDialog->currentPos;
    if(m_pFindDialog->getCurrentWindow() == eWindowIndex::C)
    {
        if(m_pFindDialog->m_pSearchInC->isChecked() && m_pDiffTextWindow3 != nullptr &&
           m_pDiffTextWindow3->findString(s, d3vLine, posInLine, bDirDown, bCaseSensitive))
        {
            m_pDiffTextWindow3->setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length(), l, p);
            DiffTextWindow::mVScrollBar->setValue(l - DiffTextWindow::mVScrollBar->pageStep() / 2);
            m_pHScrollBar->setValue(std::max(0, p + s.length() - m_pHScrollBar->pageStep()));
            m_pFindDialog->currentLine = d3vLine;
            m_pFindDialog->currentPos = posInLine + 1;
            return;
        }

        m_pFindDialog->nextWindow();
    }

    d3vLine = m_pFindDialog->currentLine;
    posInLine = m_pFindDialog->currentPos;
    if(m_pFindDialog->getCurrentWindow() == eWindowIndex::Output)
    {
        if(m_pFindDialog->m_pSearchInOutput->isChecked() && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isVisible() &&
           m_pMergeResultWindow->findString(s, d3vLine, posInLine, bDirDown, bCaseSensitive))
        {
            m_pMergeResultWindow->setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length());
            MergeResultWindow::mVScrollBar->setValue(d3vLine - MergeResultWindow::mVScrollBar->pageStep() / 2);
            m_pHScrollBar->setValue(std::max(0, posInLine + s.length() - m_pHScrollBar->pageStep()));
            m_pFindDialog->currentLine = d3vLine;
            m_pFindDialog->currentPos = posInLine + 1;
            return;
        }

        m_pFindDialog->nextWindow();
    }

    KMessageBox::information(this, i18n("Search complete."), i18n("Search Complete"));
    m_pFindDialog->restartFind();
}

void KDiff3App::slotMergeCurrentFile()
{
    if(m_bDirCompare && m_pDirectoryMergeWindow->isVisible() && m_pDirectoryMergeWindow->isFileSelected())
    {
        m_pDirectoryMergeWindow->mergeCurrentFile();
    }
    else if(m_pMainWidget->isVisible())
    {
        if(!canContinue()) return;

        if(m_outputFilename.isEmpty())
        {
            if(!m_sd3->isEmpty() && !m_sd3->isFromBuffer())
            {
                m_outputFilename = m_sd3->getFilename();
            }
            else if(!m_sd2->isEmpty() && !m_sd2->isFromBuffer())
            {
                m_outputFilename = m_sd2->getFilename();
            }
            else if(!m_sd1->isEmpty() && !m_sd1->isFromBuffer())
            {
                m_outputFilename = m_sd1->getFilename();
            }
            else
            {
                m_outputFilename = "unnamed.txt";
                m_bDefaultFilename = true;
            }
        }
        mainInit(m_totalDiffStatus);
    }
}

void KDiff3App::slotWinFocusNext()
{
    QWidget* focus = qApp->focusWidget();
    if(focus == m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->isVisible() && !dirShowBoth->isChecked())
    {
        slotDirViewToggle();
    }

    std::list<QWidget*> visibleWidgetList;
    if(m_pDiffTextWindow1 && m_pDiffTextWindow1->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow1);
    if(m_pDiffTextWindow2 && m_pDiffTextWindow2->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow2);
    if(m_pDiffTextWindow3 && m_pDiffTextWindow3->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow3);
    if(m_pMergeResultWindow && m_pMergeResultWindow->isVisible()) visibleWidgetList.push_back(m_pMergeResultWindow);
    if(m_bDirCompare /*m_pDirectoryMergeWindow->isVisible()*/) visibleWidgetList.push_back(m_pDirectoryMergeWindow);
    //if ( m_pDirectoryMergeInfo->isVisible() ) visibleWidgetList.push_back(m_pDirectoryMergeInfo->getInfoList());
    if(visibleWidgetList.empty())
        return;

    std::list<QWidget*>::iterator i = std::find(visibleWidgetList.begin(), visibleWidgetList.end(), focus);
    ++i;
    if(i == visibleWidgetList.end())
        i = visibleWidgetList.begin();

    if(*i == m_pDirectoryMergeWindow && !dirShowBoth->isChecked())
    {
        slotDirViewToggle();
    }
    (*i)->setFocus();
}

void KDiff3App::slotWinFocusPrev()
{
    QWidget* focus = qApp->focusWidget();
    if(focus == m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->isVisible() && !dirShowBoth->isChecked())
    {
        slotDirViewToggle();
    }

    std::list<QWidget*> visibleWidgetList;
    if(m_pDiffTextWindow1 && m_pDiffTextWindow1->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow1);
    if(m_pDiffTextWindow2 && m_pDiffTextWindow2->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow2);
    if(m_pDiffTextWindow3 && m_pDiffTextWindow3->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow3);
    if(m_pMergeResultWindow && m_pMergeResultWindow->isVisible()) visibleWidgetList.push_back(m_pMergeResultWindow);
    if(m_bDirCompare /* m_pDirectoryMergeWindow->isVisible() */) visibleWidgetList.push_back(m_pDirectoryMergeWindow);
    //if ( m_pDirectoryMergeInfo->isVisible() ) visibleWidgetList.push_back(m_pDirectoryMergeInfo->getInfoList());
    if(visibleWidgetList.empty())
        return;

    std::list<QWidget*>::iterator i = std::find(visibleWidgetList.begin(), visibleWidgetList.end(), focus);
    if(i == visibleWidgetList.begin())
        i = visibleWidgetList.end();
    --i;
    //i will never be
    if(*i == m_pDirectoryMergeWindow && !dirShowBoth->isChecked())
    {
        slotDirViewToggle();
    }
    (*i)->setFocus();
}

void KDiff3App::slotWinToggleSplitterOrientation()
{
    if(m_pDiffWindowSplitter != nullptr)
    {
        m_pDiffWindowSplitter->setOrientation(
            m_pDiffWindowSplitter->orientation() == Qt::Vertical ? Qt::Horizontal : Qt::Vertical);

        m_pOptions->m_bHorizDiffWindowSplitting = m_pDiffWindowSplitter->orientation() == Qt::Horizontal;
    }
}

void KDiff3App::slotOverviewNormal()
{
    Q_EMIT changeOverViewMode(e_OverviewMode::eOMNormal);

    Q_EMIT updateAvailabilities();
}

void KDiff3App::slotOverviewAB()
{
    Q_EMIT changeOverViewMode(e_OverviewMode::eOMAvsB);

    Q_EMIT updateAvailabilities();
}

void KDiff3App::slotOverviewAC()
{
    Q_EMIT changeOverViewMode(e_OverviewMode::eOMAvsC);

    Q_EMIT updateAvailabilities();
}

void KDiff3App::slotOverviewBC()
{
    Q_EMIT changeOverViewMode(e_OverviewMode::eOMBvsC);

    Q_EMIT updateAvailabilities();
}

void KDiff3App::slotNoRelevantChangesDetected()
{
    if(m_bTripleDiff && !m_outputFilename.isEmpty())
    {
        //KMessageBox::information( this, "No relevant changes detected", "KDiff3" );
        if(!m_pOptions->m_IrrelevantMergeCmd.isEmpty())
        {
            /*
                QProcess doesn't check for single quotes and uses non-standard escaping syntax for double quotes.
                    The distinction between single and double quotes is purely a command shell issue. So
                    we split the command string ourselves.
            */
            QStringList args;
            QString program;
            Utils::getArguments(m_pOptions->m_IrrelevantMergeCmd, program, args);
            QProcess process;
            process.start(program, args);
            process.waitForFinished(-1);
        }
    }
}

void KDiff3App::slotAddManualDiffHelp()
{
    LineRef firstLine;
    LineRef lastLine;
    e_SrcSelector winIdx = e_SrcSelector::Invalid;
    if(m_pDiffTextWindow1)
    {
        m_pDiffTextWindow1->getSelectionRange(&firstLine, &lastLine, eFileCoords);
        winIdx = e_SrcSelector::A;
    }
    if(!firstLine.isValid() && m_pDiffTextWindow2)
    {
        m_pDiffTextWindow2->getSelectionRange(&firstLine, &lastLine, eFileCoords);
        winIdx = e_SrcSelector::B;
    }
    if(!firstLine.isValid() && m_pDiffTextWindow3)
    {
        m_pDiffTextWindow3->getSelectionRange(&firstLine, &lastLine, eFileCoords);
        winIdx = e_SrcSelector::C;
    }

    if(!firstLine.isValid() || !lastLine.isValid() || lastLine < firstLine)
        KMessageBox::information(this, i18n("Nothing is selected in either diff input window."), i18n("Error while adding manual diff range"));
    else
    {
        m_manualDiffHelpList.insertEntry(winIdx, firstLine, lastLine);

        mainInit(m_totalDiffStatus, InitFlag::autoSolve | InitFlag::initGUI); // Init without reload
        slotRefresh();
    }
}

void KDiff3App::slotClearManualDiffHelpList()
{
    m_manualDiffHelpList.clear();
    mainInit(m_totalDiffStatus, InitFlag::autoSolve | InitFlag::initGUI); // Init without reload
    slotRefresh();
}

void KDiff3App::slotEncodingChanged(QTextCodec*)
{
    mainInit(m_totalDiffStatus, InitFlag::loadFiles | InitFlag::useCurrentEncoding | InitFlag::autoSolve); // Init with reload
    slotRefresh();
}

void KDiff3App::slotUpdateAvailabilities()
{
    if(m_pDiffTextWindow2 == nullptr || m_pDiffTextWindow1 == nullptr || m_pDiffTextWindow3 == nullptr)
        return;

    bool bTextDataAvailable = (m_sd1->hasData() || m_sd2->hasData() || m_sd3->hasData());

    if(dirShowBoth->isChecked())
    {
        if(m_pDirectoryMergeDock != nullptr)
            m_pDirectoryMergeDock->setVisible(m_bDirCompare);
        if(m_pDirectoryMergeInfoDock != nullptr)
            m_pDirectoryMergeInfoDock->setVisible(m_bDirCompare);

        if(!m_pMainWidget->isVisible() &&
           bTextDataAvailable && !m_pDirectoryMergeWindow->isScanning())
            m_pMainWidget->show();
    }

    bool bDiffWindowVisible = m_pMainWidget->isVisible();
    bool bMergeEditorVisible = m_pMergeWindowFrame != nullptr && m_pMergeWindowFrame->isVisible() && m_pMergeResultWindow != nullptr;

    m_pDirectoryMergeWindow->updateAvailabilities(bMergeEditorVisible, m_bDirCompare, bDiffWindowVisible, chooseA, chooseB, chooseC);

    dirShowBoth->setEnabled(m_bDirCompare);
    dirViewToggle->setEnabled(
        m_bDirCompare &&
        (m_pDirectoryMergeDock != nullptr && m_pDirectoryMergeInfoDock != nullptr &&
         ((!m_pDirectoryMergeDock->isVisible() && m_pMainWidget->isVisible()) ||
          (m_pDirectoryMergeDock->isVisible() && !m_pMainWidget->isVisible() && bTextDataAvailable))));

    showWhiteSpaceCharacters->setEnabled(bDiffWindowVisible);
    autoAdvance->setEnabled(bMergeEditorVisible);
    mAutoSolve->setEnabled(bMergeEditorVisible && m_bTripleDiff);
    mUnsolve->setEnabled(bMergeEditorVisible);

    editUndo->setEnabled(false); //Not yet implemented but planned.
    editCut->setEnabled(allowCut());
    editCopy->setEnabled(allowCopy());

    mMergeHistory->setEnabled(bMergeEditorVisible);
    mergeRegExp->setEnabled(bMergeEditorVisible);
    showWindowA->setEnabled(bDiffWindowVisible && (m_pDiffTextWindow2->isVisible() || m_pDiffTextWindow3->isVisible()));
    showWindowB->setEnabled(bDiffWindowVisible && (m_pDiffTextWindow1->isVisible() || m_pDiffTextWindow3->isVisible()));
    showWindowC->setEnabled(bDiffWindowVisible && m_bTripleDiff && (m_pDiffTextWindow1->isVisible() || m_pDiffTextWindow2->isVisible()));
    editFind->setEnabled(bDiffWindowVisible);
    editFindNext->setEnabled(bDiffWindowVisible);
    m_pFindDialog->m_pSearchInC->setEnabled(m_bTripleDiff);
    m_pFindDialog->m_pSearchInOutput->setEnabled(bMergeEditorVisible);

    bool bSavable = bMergeEditorVisible && m_pMergeResultWindow->getNumberOfUnsolvedConflicts() == 0;
    fileSave->setEnabled(m_bOutputModified && bSavable);
    fileSaveAs->setEnabled(bSavable);

    mGoTop->setEnabled(bDiffWindowVisible && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isDeltaAboveCurrent());
    mGoBottom->setEnabled(bDiffWindowVisible && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isDeltaBelowCurrent());
    mGoCurrent->setEnabled(bDiffWindowVisible);
    mGoPrevUnsolvedConflict->setEnabled(bMergeEditorVisible && m_pMergeResultWindow->isUnsolvedConflictAboveCurrent());
    mGoNextUnsolvedConflict->setEnabled(bMergeEditorVisible && m_pMergeResultWindow->isUnsolvedConflictBelowCurrent());
    mGoPrevConflict->setEnabled(bDiffWindowVisible && bMergeEditorVisible && m_pMergeResultWindow->isConflictAboveCurrent());
    mGoNextConflict->setEnabled(bDiffWindowVisible && bMergeEditorVisible && m_pMergeResultWindow->isConflictBelowCurrent());
    mGoPrevDelta->setEnabled(bDiffWindowVisible && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isDeltaAboveCurrent());
    mGoNextDelta->setEnabled(bDiffWindowVisible && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isDeltaBelowCurrent());

    overviewModeNormal->setEnabled(m_bTripleDiff && bDiffWindowVisible);
    overviewModeAB->setEnabled(m_bTripleDiff && bDiffWindowVisible);
    overviewModeAC->setEnabled(m_bTripleDiff && bDiffWindowVisible);
    overviewModeBC->setEnabled(m_bTripleDiff && bDiffWindowVisible);
    e_OverviewMode overviewMode = m_pOverview == nullptr ? e_OverviewMode::eOMNormal : m_pOverview->getOverviewMode();
    overviewModeNormal->setChecked(overviewMode == e_OverviewMode::eOMNormal);
    overviewModeAB->setChecked(overviewMode == e_OverviewMode::eOMAvsB);
    overviewModeAC->setChecked(overviewMode == e_OverviewMode::eOMAvsC);
    overviewModeBC->setChecked(overviewMode == e_OverviewMode::eOMBvsC);

    winToggleSplitOrientation->setEnabled(bDiffWindowVisible && m_pDiffWindowSplitter != nullptr);
}
